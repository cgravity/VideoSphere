extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

#include <pthread.h>

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <list>
using namespace std;

#include "decoder.h"

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
    if(argc != 2)
    {
        cerr << "USAGE: ./video_sphere <path-to-video>\n";
        return EXIT_FAILURE;
    }

#if 0
    PaError paerror = Pa_Initialize();
    if(paerror != paNoError)
    {
        cerr << "Failed to init PortAudio\n";
        return EXIT_FAILURE;
    }
#endif

    av_register_all();
    avcodec_register_all();
    
    Decoder decoder;
    bool ok = decoder.open(argv[1]);
    
    if(!ok)
        return EXIT_FAILURE;
    
    // 10 seconds of buffer is ~750MB if video size is 1920x1080
    decoder.add_fillable_frames(24*5);
    decoder.start_thread();
    
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
    
    int64_t start = av_gettime_relative();
    int64_t now;
    
    
    bool decoded_all = false;
    
    double current_frame_end = -1;
    
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

