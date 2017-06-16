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
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <list>
using namespace std;

#include "decoder.h"
#include "network.h"

#include <GL/glew.h>
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

string slurp(const string& filename)
{
    ifstream src(filename.c_str());
    
    if(src.fail())
    {
        cerr << "Failed to load: " << filename << "\n";
        exit(EXIT_FAILURE);
    }
    
    stringstream buffer;
    buffer << src.rdbuf();
    
    return buffer.str();
}

bool endswith(const string& data, const string& pattern)
{
    if(data.size() < pattern.size())
        return false;
    
    const char* data_str = data.c_str();
    data_str += data.size() - pattern.size();
    
    const char* pattern_str = pattern.c_str();
    while(*pattern_str)
    {
        if(tolower(*data_str) != tolower(*pattern_str))
            return false;
        
        pattern_str++;
        data_str++;
    }
    
    return true;
}


// given a list of filenames, loads files, compiles, and links them
// returns the program id if successful, or quits with error if not
GLuint load_shaders(vector<string> filenames)
{
    GLint program = glCreateProgram();
    
    for(size_t i = 0; i < filenames.size(); i++)
    {
        string src = slurp(filenames[i]);
        const char* src_c = src.c_str();
        
        GLint type = 0;
        
        if(endswith(filenames[i], ".frag"))
            type = GL_FRAGMENT_SHADER;
        else if(endswith(filenames[i], ".vert"))
            type = GL_VERTEX_SHADER;
        else
        {
            cerr << "Unsupported shader type!\n";
            cerr << "File: " << filenames[i] << '\n';
            exit(EXIT_FAILURE);
        }
        
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src_c, NULL);
        glCompileShader(shader);
        
        GLint shader_success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_success);
        
        if(shader_success == GL_FALSE)
        {
            cerr << "Failed to compile shader!\n";
            cerr << "File: " << filenames[i] << '\n';
            cerr << "Error:\n";
            
            GLint logSize = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
            
            char* error_msg = (char*)malloc(logSize);
            
            glGetShaderInfoLog(shader, logSize, NULL, error_msg);
            cerr << error_msg << '\n';
            
            free(error_msg);
            exit(EXIT_FAILURE);
        }
        
        glAttachShader(program, shader);
    }    
    
    glLinkProgram(program);
    
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(success == GL_FALSE)
    {
        cerr << "Failed to link shaders!\n";
        
        GLint logSize = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
        
        char* error_msg = (char*)malloc(logSize);
        
        glGetProgramInfoLog(program, logSize, NULL, error_msg);
        cerr << error_msg << '\n';
        
        free(error_msg);
        exit(EXIT_FAILURE);
    }
    
    return program;
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
    int64_t now = 0;
    
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
        
        while(path == "" || now == 0)
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
                    
                    char* out = (char*)&now;
                    char* in  = (char*)&m.bytes[1];
                    
                    for(size_t j = 0; j < sizeof(int64_t); j++)
                        *out++ = *in++;
                    
                    start = av_gettime_relative() - now;
                    
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
        
        // now is set in microseconds (assuming AV_TIME_BASE = 1000000)
        // decoder.time_base is a fraction (in seconds) per frame
        // discrete ticks of it are times to seek to?
        
        int64_t seek_to = av_rescale(now, 
            decoder.time_base.den, decoder.time_base.num);
        
        seek_to /= AV_TIME_BASE;
        
        decoder.seek(seek_to);
        
        cout << "SEEK TO: " << seek_to << '\n';
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
    
    // initialize GLEW
    GLenum glew_error = glewInit();
    if (GLEW_OK != glew_error)
    {
      cerr << "GLEW Error: " << glewGetErrorString(glew_error);
      glfwTerminate();
      return EXIT_FAILURE;
    }
    
    // load shaders
    vector<string> no_distort_files;
    no_distort_files.push_back("shaders/no-distort.vert");
    no_distort_files.push_back("shaders/no-distort.frag");
    
    GLint no_distort_program = load_shaders(no_distort_files);
    
    
    vector<string> mono_equirect_files;
    mono_equirect_files.push_back("shaders/simple-mono.vert");
    mono_equirect_files.push_back("shaders/simple-mono.frag");
    
    GLint mono_equirect_program = load_shaders(mono_equirect_files);
    
    
    // select which shader to use
    GLint shader_program = mono_equirect_program;
    
    
    GLint pos = glGetAttribLocation(shader_program, "pos_in");
    
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
                
//                int64_t seek = av_rescale(now, decoder.time_base.den,
//                    decoder.time_base.num);
                
                s.push_back((unsigned char)'S');
                
                cout << "Now: " << now << '\n';
                unsigned char* ptr = (unsigned char*)& now;
                for(size_t i = 0; i < sizeof(now); i++)
                {
                    s.push_back(*ptr);
                    ptr++;
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
                //cerr << "Playing faster than decode\n";
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
        
        glUseProgram(shader_program);
        
        // for 'pos', using convention where x goes to the right, y goes in,
        // and z goes up.
        glBegin(GL_QUADS);
        {
            //glTexCoord2f(0,1);
            glVertexAttrib3f(pos, -11, 1.5, 6);
            glVertex2f(-1, 1);
            
            //glTexCoord2f(0,0);
            glVertexAttrib3f(pos, -11, 1.5, -6);
            glVertex2f(-1, -1);

            //glTexCoord2f(1,0);
            glVertexAttrib3f(pos, 11, 1.5, -6);
            glVertex2f(1,-1);
            
            //glTexCoord2f(1,1);
            glVertexAttrib3f(pos, 11, 1.5, 6);
            glVertex2f(1,1);
            //glVertexAttrib3f(pos, 5,1,5);
        }
        glEnd();
        
        glfwSwapBuffers(window);
    }
    
end_of_video:
    cout << "end of video detected\n";
    decoder.set_quit();
    decoder.join();
    glfwTerminate();
}

