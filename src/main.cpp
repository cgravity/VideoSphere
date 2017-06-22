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
#include "screen.h"
#include "player.h"
#include "shader.h"
#include "util.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>


#define TURN (2*3.1415926535)

// USE FFMPEG 3.3.1
// USE PortAudio pa_stable_v190600_20161030


void on_window_resize(GLFWwindow* window, int w, int h)
{
    glViewport(0,0,w,h);
}

int main(int argc, char* argv[]) 
{    
    Player player;
    
    av_register_all();
    avcodec_register_all();
    
    int64_t& start = player.start;
    int64_t& now = player.now;
    
    start = av_gettime_relative();
    now = 0;
    
#if 0
    PaError paerror = Pa_Initialize();
    if(paerror != paNoError)
    {
        cerr << "Failed to init PortAudio\n";
        return EXIT_FAILURE;
    }
#endif

    parse_args(player, argc, argv);

    Server*& server = player.server;
    Client*& client = player.client;
    NetworkThread*& nt = player.nt;
    
    Decoder& decoder = player.decoder;
    
    player.start_threads();

    // 10 seconds of buffer is ~750MB if video size is 1920x1080
    
    GLFWwindow* window;
    
    if(!glfwInit())
    {
        glfwTerminate();
        cerr << "Failed to init GLFW3\n";
        return EXIT_FAILURE;
    }
    
    //monitor_test();
    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    
    if(monitor_count == 0)
    {
        cerr << "Failed to detect monitors!\n";
        return EXIT_FAILURE;
    }
    
    if(client)
    {
        GLFWmonitor* monitor = monitors[0];
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        
        //window = glfwCreateWindow(mode->width, mode->height, "Video Sphere", monitor, NULL);
        window = glfwCreateWindow(1920/2, 1080/2, "Video Sphere", NULL, NULL);
    }
    else
    {
        // server
        window = glfwCreateWindow(1920/2, 1920/2/2, "Video Sphere", NULL, NULL);
    }
    
    
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
    GLint shader_program;
    
    if(server)
        shader_program = no_distort_program;
    else
        shader_program = mono_equirect_program;
    
    GLint theta_ = glGetUniformLocation(shader_program, "theta");
    GLint phi_   = glGetUniformLocation(shader_program, "phi");
    GLint pos = glGetAttribLocation(shader_program, "pos_in");
    
    float theta = 0.0;
    float phi = 0.0;
    
    glUseProgram(shader_program);
    
    glfwSetWindowSizeCallback(window, on_window_resize);
    
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    
    
    bool decoded_all = false;
    
    double current_frame_end = -1;
    
    vector<Message> messages;
    
    bool left_down = false;
    bool right_down = false;
    bool up_down = false;
    bool down_down = false;
    
    while(true)
    {
        glfwPollEvents();
        
        bool send_pos = false;
        
        if(glfwGetKey(window, GLFW_KEY_ESCAPE))
            break;
        
        if(glfwGetKey(window, GLFW_KEY_LEFT))
        {
            if(!left_down)
            {
                theta -= TURN / 25;
                send_pos = true;
            }
            
            left_down = true;
        }
        else
            left_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_RIGHT))
        {
            if(!right_down)
            {
                theta += TURN / 25;
                send_pos = true;
            }
            
            right_down = true;
        }
        else
            right_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_UP))
        {
            if(!up_down)
            {
                phi -= (TURN/4) / 10.0;
                send_pos = true;
            }
            
            up_down = true;
        }
        else
            up_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_DOWN))
        {
            if(!down_down)
            {
                phi += (TURN/4) / 10.0;
                send_pos = true;
            }
            
            down_down = true;
        }
        else
            down_down = false;
        
        theta = fmod(theta, TURN);
        if(theta < 0)
            theta += TURN;
        //printf("%0.0f\n", 180 / M_PI * theta);
        
        if(phi > TURN/4)
            phi = TURN/4;
        else if(phi < -TURN/4)
            phi = -TURN/4;
        
        if(send_pos && player.type == NT_SERVER)
        {
            Message turn;
            turn.write_byte('T');
            turn.write_float(phi);
            turn.write_float(theta);
            
            server->send(turn);
        }
        
        decoder.lock();
        decoded_all = decoder.decoded_all_flag;
        decoder.unlock();
        
        now = av_gettime_relative() - start;
        
        double now_f = now / 1000000.0;
        
        // FIXME: factor out message parsing. Handle client/server separately?
        nt->get_messages(messages);
        for(size_t i = 0; i < messages.size(); i++)
        {
            try
            {
                Message& m = messages[i];
                if(m.size() == 0)
                {
                    cerr << "Error: Empty network message\n";
                    continue;
                }
                
                if(m.as_string() == "HELLO")
                {
                    // new connection
                    // send the video path and seek time
                    Message path;
                    Message seek;
                    
                    path.write_char('P');
                    path.write_string(player.video_path);
                    
                    seek.write_char('S');
                    seek.write_int64(now);
                    
                    m.reply(path);
                    m.reply(seek);
                    
//                    cout << "Now: " << now << '\n';
//                    unsigned char* ptr = (unsigned char*)& now;
//                    for(size_t i = 0; i < sizeof(now); i++)
//                    {
//                        s.push_back(*ptr);
//                        ptr++;
//                    }
                    continue;
                }
                
                char type = m.read_char();
                
                switch(type)
                {
                    case 'P':
                    {
                        cerr << "Network path value sent at inappropriate time\n";
                        continue;
                    }
                    break;
                    
                    case 'S':
                    {
                        int64_t value = m.read_int64();
                        decoder.seek(value);
                    }
                    break;
                    
                    case 'T':
                    {
                        phi = m.read_float();
                        theta = m.read_float();
                    }
                    break;
                    
                    default:
                        cerr << "Failed to parse message with type '"
                             << type << "'\n";
                        continue;
                }
            
            } // try
            catch(ParseError pe)
            {
                cerr << "Network parse error: " << pe.what() << '\n';
            }
        } // for each message
        
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
        
        
        // for 'pos', using convention where x goes to the right, y goes in,
        // and z goes up.
        glUniform1f(theta_, theta);
        glUniform1f(phi_, phi);
        
        glBegin(GL_QUADS);
        {
            glVertexAttrib3f(pos, -11, 2*1.5, 6);
            glTexCoord2f(0, 0);
            glVertex2f(-1, 1);
            
            glVertexAttrib3f(pos, -11, 2*1.5, -6);
            glTexCoord2f(0, 1);
            glVertex2f(-1, -1);

            glVertexAttrib3f(pos, 11, 2*1.5, -6);
            glTexCoord2f(1,1);
            glVertex2f(1,-1);
            
            glVertexAttrib3f(pos, 11, 2*1.5, 6);
            glTexCoord2f(1, 0);
            glVertex2f(1,1);
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

