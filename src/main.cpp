extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <list>
using namespace std;

#include "decoder.h"
#include "network.h"

#include <GLFW/glfw3.h>

// USE FFMPEG 3.3.1
// USE PortAudio pa_stable_v190600_20161030

// see http://dranger.com/ffmpeg/tutorial01.html for reference

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  
  // Close file
  fclose(pFile);
}

void on_window_resize(GLFWwindow* window, int w, int h)
{
    glViewport(0,0,w,h);
}

int main(int argc, char* argv[]) 
{
    if(argc != 3)
    {
        cerr << "USAGE: ./video_sphere --server <path-to-video>\n";
        cerr << "or\n";
        cerr << "USAGE: ./video_sphere --client <hostname-or-ip>\n";
        return EXIT_FAILURE;
    }
    
    av_register_all();
    avcodec_register_all();
    
    int64_t start = av_gettime_relative();
    int64_t now;
    
#if 0
    PaError paerror = Pa_Initialize();
    if(paerror != paNoError)
    {
        cerr << "Failed to init PortAudio\n";
        return EXIT_FAILURE;
    }
#endif

    Server* server = NULL;
    Client* client = NULL;
    NetworkThread* nt = NULL;
    
    Decoder decoder;
    
    if(string(argv[1]) == "--server")
    {
        server = new Server();
        nt = server;
        nt->start_thread();
            
        bool ok = decoder.open(argv[2]);
        
        if(!ok)
            return EXIT_FAILURE;    
        
        decoder.add_fillable_frames(24*5);
        decoder.start_thread();
    }
    else if(string(argv[1]) == "--client")
    {
        client = new Client(argv[2], 2345);
        nt = client;
        nt->start_thread();
        
        nt->send("HELLO");
        
        cout << "Waiting for path...\n";
        
        string path = "";
        int64_t seek_to = 0;
        
        while(path == "" && seek_to == 0)
        {
            vector<Message> msgs;
            client->get_messages(msgs);
            
            for(size_t i = 0; i < msgs.size(); i++)
            {
                Message& m = msgs[i];
                
                if(m.size() == 0)
                    continue; // empty message
                
                if(m.bytes[0] == 'S')
                {
                    if(m.size() != sizeof(int64_t) + 1)
                    {
                        cerr << "Invalid network seek at client start!\n";
                        continue;
                    }
                    
                    seek_to = *(int64_t*)&m.bytes[1];
                    continue;
                }
                
                if(m.bytes[0] == 'P')
                {
                    path = m.as_string().c_str() + 1;
                    continue;
                }
            }
        }
        
        bool ok = decoder.open(path);
        
        if(!ok)
        {
            cerr << "Can't open that path!\n";
            return EXIT_FAILURE;   
        }
        
        decoder.add_fillable_frames(24*5);
        decoder.start_thread();
        decoder.seek(seek_to);
        
        start = av_gettime_relative() - seek_to;
        now = seek_to;
    }

    // 10 seconds of buffer is ~750MB if video size is 1920x1080
    
    GLFWwindow* window;
    
    if(!glfwInit())
    {
        glfwTerminate();
        cerr << "Failed to init GLFW3\n";
        return EXIT_FAILURE;
    }
    
    window = glfwCreateWindow(640, 360, "Video Sphere", NULL, NULL);
    if(!window)
    {
        glfwTerminate();
        cerr << "Failed to open window\n";
        return EXIT_FAILURE;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, on_window_resize);
    
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    
    int i = 0;
    
    
    
    bool decoded_all = false;
    
    double current_frame_end = -1;
    
    vector<Message> messages;
    
    while(true)
    {
        glfwPollEvents();
        
        if(glfwGetKey(window, GLFW_KEY_ESCAPE))
            break;
        
        decoder.lock();
        decoded_all = decoder.decoded_all_flag;
        decoder.unlock();
        
        now = av_gettime_relative() - start;
        
        double now_f = now / 1000000.0;
        
        // FIXME: factor out message parsing. Handle client/server separately?
        nt->get_messages(messages);
        for(size_t i = 0; i < messages.size(); i++)
        {
            Message& m = messages[i];
            if(m.size() == 0)
            {
                cerr << "Error: Empty network message\n";
                continue;
            }
            
            if(m.as_string()[0] == 'S') // seek
            {
                if(m.size() != sizeof(int64_t) + 1)
                {
                    cerr << "Error: Invalid network seek\n";
                    continue;
                }
                
                int64_t value = *(int64_t*)&m.bytes[1];
                decoder.seek(value);
            }
            else if(m.as_string() == "HELLO")
            {
                // new connection -- sent it 
                string p = "P" + string(argv[2]);
                vector<unsigned char> s;
                
                int64_t seek = now;
                seek *= decoder.time_base.den;
                seek /= decoder.time_base.num;
                
                s.push_back((unsigned char)'S');
                for(size_t i = 0; i < sizeof(now); i++)
                {
                    s.push_back(*((unsigned char*)&seek + i));
                }
                
                m.reply(p);
                m.reply(s);
            }
        }
        
        AVFrame* show_frame = NULL;
        AVFrame* show_frame_prev = NULL;
        
        while(now_f > current_frame_end)
        {
            show_frame_prev = show_frame;
            show_frame = decoder.get_frame();
            
            if(show_frame && show_frame_prev)
            {
                decoder.return_frame(show_frame_prev);
                show_frame_prev = NULL;
            }
            
            if(!show_frame && decoded_all)
                goto end_of_video; // end of video
            
            if(!show_frame)
            {
                cerr << "Playing faster than decode\n";
                break;
            }
            
            current_frame_end = show_frame->pts;
            current_frame_end /= decoder.time_base.den;
            current_frame_end *= decoder.time_base.num;
        }
        
        if(!show_frame && !show_frame_prev)
        {
            // no new frames ready
            continue;
        }
        
        if(!show_frame && show_frame_prev)
        {
            // lagged frame is ready, but current data not available
            // show it anyway as current frame (better late than nothing)
            show_frame = show_frame_prev;
            show_frame_prev = NULL;
        }
        
        //printf("Frame end: %f, NOW: %f\n", current_frame_end, now_f);
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
            decoder.codec_context->width, decoder.codec_context->height,
            0, GL_RGB, GL_UNSIGNED_BYTE, show_frame->data[0]);
        
        decoder.return_frame(show_frame);
        
        glBegin(GL_QUADS);
        {
            glTexCoord2f(0,1);
            glVertex2f(-1,-1);
            
            glTexCoord2f(0,0);
            glVertex2f(-1,1);
            
            glTexCoord2f(1,0);
            glVertex2f(1,1);
            
            glTexCoord2f(1,1);
            glVertex2f(1,-1);
        }
        glEnd();
        
        glfwSwapBuffers(window);
    }
    
end_of_video:
    decoder.set_quit();
    decoder.join();
    glfwTerminate();
}

