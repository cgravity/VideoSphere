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

#include "window.h"

#include <X11/Xlib.h>

#include "ps3_joystick.h"

#define TURN (2*3.1415926535)

// USE FFMPEG 3.3.1
// USE PortAudio pa_stable_v190600_20161030

int main(int argc, char* argv[]) 
{        
    if(!glfwInit())
    {
        glfwTerminate();
        cerr << "Failed to init GLFW3\n";
        return EXIT_FAILURE;
    }
    
    monitor_test();
    
    Player player;
    
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    
    int64_t& start = player.start;
    int64_t& now = player.now;
    bool& seek_flag = player.seek_flag;
    
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

    if(server && player.screen_config.size() == 0)
    {
        ScreenConfig sc;
        sc.pixel_width = 960;
        sc.pixel_height = 480;
        sc.fullscreen = false;
        
        player.screen_config.push_back(sc);
    }
    
    player.create_windows();
    
    if(player.windows.size() == 0)
    {
        glfwTerminate();
        cerr << "Failed to open window\n";
        exit(EXIT_FAILURE);
    }
    
    //glfwMakeContextCurrent(player.windows[0]);
    player.windows[0]->make_current();
        
    // initialize GLEW
    GLenum glew_error = glewInit();
    if (GLEW_OK != glew_error)
    {
      cerr << "GLEW Error: " << glewGetErrorString(glew_error);
      glfwTerminate();
      return EXIT_FAILURE;
    }
    
    // Fixes weird X Cursor bug when you have two GPUs on CentOS 7
    // This is very important for the SunCAVE
    if(player.suncave_workarounds)
    {
        Display* dpy = XOpenDisplay(":0");
        Window root = XRootWindow(dpy, 1);
        XWarpPointer(dpy, None, root, 0,0,0,0,  0,0);
        XCloseDisplay(dpy);
    }
    
    // load shaders
    vector<string> no_distort_files;
    no_distort_files.push_back("shaders/no-distort.vert");
    no_distort_files.push_back("shaders/no-distort.frag");
    
    vector<string> mono_equirect_files;
    mono_equirect_files.push_back("shaders/simple-mono.vert");
    mono_equirect_files.push_back("shaders/simple-mono.frag");
    
    vector<string> aa_mono_equirect_files;
    aa_mono_equirect_files.push_back("shaders/simple-mono.vert");
    aa_mono_equirect_files.push_back("shaders/aa-mono.frag");
    
    vector<string> stereo_equirect_files;
    stereo_equirect_files.push_back("shaders/simple-stereo.vert");
    stereo_equirect_files.push_back("shaders/simple-stereo.frag");
    
    vector<string> stereo_interleaved_files;
    stereo_interleaved_files.push_back("shaders/simple-stereo.vert");
    stereo_interleaved_files.push_back("shaders/interleaved-stereo.frag");
    
    // need to compile shaders for each window
    for(size_t i = 0; i < player.windows.size(); i++)
    {    
        player.windows[i]->make_current();
        
        player.windows[i]->no_distort_program = load_shaders(no_distort_files);
        player.windows[i]->mono_equirect_program = load_shaders(mono_equirect_files);
        player.windows[i]->aa_mono_equirect_program = load_shaders(aa_mono_equirect_files);
        player.windows[i]->stereo_equirect_program = load_shaders(stereo_equirect_files);
        player.windows[i]->stereo_interleaved_program = load_shaders(stereo_interleaved_files);
    }
    
    // select which shader to use
    // note use of pointer-to-member so we can access it on each window
    GLint Window_::* shader_program;
    
    if(server && player.type != NT_HEADLESS)
        shader_program = &Window_::no_distort_program;
    else
    {
        if(player.stereo)
        {
            if(player.stereo_type == STEREO_TOP_BOTTOM_INTERLEAVED)
                shader_program = &Window_::stereo_interleaved_program;
            else
                shader_program = &Window_::stereo_equirect_program;
        }
        else
            shader_program = &Window_::aa_mono_equirect_program;
    }
    
    if(player.stereo_type == STEREO_HALF_TOP || 
        player.stereo_type == STEREO_HALF_BOTTOM)
    {
        shader_program = &Window_::stereo_equirect_program;
    }
    
    
    float theta = 0.0;
    float phi = 0.0;
    
    for(size_t i = 0; i < player.windows.size(); i++)
    {
        //glfwMakeContextCurrent(player.windows[i]);
        player.windows[i]->make_current();
        glUseProgram(player.windows[i]->*shader_program);
        glEnable(GL_TEXTURE_2D);
        
        glGenTextures(1, &player.windows[i]->tex);

        glBindTexture(GL_TEXTURE_2D, player.windows[i]->tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // FIXME
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // FIXME
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, player.windows[i]->tex);
        GLint video_texture_ = glGetUniformLocation(
            player.windows[i]->*shader_program, 
            "video_texture");
        glUniform1i(video_texture_, 0);
    }
    
    int64_t last_server_seek = -AV_TIME_BASE;
    
    bool decoded_all = false;
    
    double current_frame_end = -1;
    
    vector<Message> messages;
    
    bool left_down = false;
    bool right_down = false;
    bool up_down = false;
    bool down_down = false;
    
    bool space_down = false;
    bool enter_down = false;
    
    //GLFWwindow* window = player.windows[0];
    Window_* window = player.windows[0];
    
    JS_State js_prev, js_curr, js_new;
    PS3Joystick joystick;
    
    if(server)
        joystick.start();
    
    bool quit = false;
    
    bool dump_frame_flag = false;
    
    int64_t last_frame_start = av_gettime_relative();
    int64_t current_frame_start = av_gettime_relative();
    while(!quit)
    {
        last_frame_start = current_frame_start;
        current_frame_start = av_gettime_relative();
        
        double dt = current_frame_start - last_frame_start;
        dt /= AV_TIME_BASE;
        
        bool send_pos = false;
        
        glfwPollEvents();
        
        if(server)
        {   
            joystick.update(js_new);
            
            if(js_new.valid)
            {
                js_prev.swap(js_curr);
                js_curr.swap(js_new);
                
                
                // after all swaps, new state mapped as:
                //
                //  curr -> prev
                //  prev -> new
                //  new  -> curr
                //
                // i.e. curr holds new current state, prev holds new prev state
                // (old curr state) and new is a buffer than can be 
                // overwritten (old prev state).
                
                if(js_curr.button_arrow_down && !js_prev.button_arrow_down)
                {
                    if(server) {
                        Message k;
                        k.write_char('K');
                         
                        server->send(k);
                    }
                }
                
                if(js_curr.button_arrow_right && !js_prev.button_arrow_right)
                {
                    int64_t target = now + 10*AV_TIME_BASE;
                    player.seek(target);
                }
                
                if(js_curr.button_arrow_left && !js_prev.button_arrow_left)
                {
                    int64_t target = now - 10*AV_TIME_BASE;
                    player.seek(target);
                }
                
                if(js_curr.button_start && !js_prev.button_start)
                {
                    cout << "pause pressed\n";
                    player.paused = !player.paused;
                    #ifndef NO_AUDIO
                    player.audio.lock();
                        player.audio.paused = player.paused;
                    player.audio.unlock();
                    #endif
                    
                    if(server)
                    {
                        Message x;
                        x.write_char('X');
                        server->send(x);
                    }
                }
                
                // this debug tool helps to discover button numbers
//                if(js_prev.valid && js_curr.valid)
//                    for(size_t i = 0; i < js_curr.buttons.size(); i++)
//                    {
//                        if(js_curr.buttons[i] && !js_prev.buttons[i])
//                            cout << "Button " << i << " pressed\n";
//                    }
            }
         
            send_pos = true;
             
            if(js_curr.valid)
            {
                float js_x = js_curr.right_stick_x;
                float js_y = js_curr.right_stick_y;
                
                // deadzone
                if(fabs(js_x) < 0.15)
                    js_x = 0.0;
                if(fabs(js_y) < 0.15)
                    js_y = 0.0;
            
                theta += dt * js_x * TURN/3.0;
                phi   -= dt * js_y * (TURN/4.0) / 2.0;
            }
        }
      
      for(size_t i = player.windows.size()-1; i < player.windows.size(); i++)
      {
        if(!player.windows[i]->glfw_window)
            continue;
        
        #if 0
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_SPACE))
        {
            if(!space_down)
            {
                cout << "!!! NOW: " 
                     << print_timestamp(now) << '\n';
                 
                 //dump_frame_flag = true;
                 
                 if(server) {
                     Message k;
                     k.write_char('K');
                     
                     server->send(k);
                 }
                 
            }
             
            space_down = true;
        }
        else
            space_down = false;
        #endif
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_ESCAPE))
            quit = true;
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_SPACE))
        {
            if(!space_down)
            {
                player.paused = !player.paused;
                #ifndef NO_AUDIO
                player.audio.lock();
                    player.audio.paused = player.paused;
                player.audio.unlock();
                #endif
                
                if(server)
                {
                    Message x;
                    x.write_char('X');
                    server->send(x);
                }
            }
            
            space_down = true;
        }
        else
            space_down = false;
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_LEFT))
        {
            if(!left_down)
            {
                if(glfwGetKey(player.windows[i]->glfw_window, 
                    GLFW_KEY_LEFT_SHIFT) || 
                   glfwGetKey(player.windows[i]->glfw_window,
                    GLFW_KEY_LEFT_CONTROL))
                {
                    // go back 10 seconds
                    int64_t target = now - 10*AV_TIME_BASE;
                    player.seek(target);
                }
                else
                {
                    theta += TURN / 25;
                    send_pos = true;
                }
            }
            
            left_down = true;
        }
        else
            left_down = false;
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_RIGHT))
        {
            if(!right_down)
            {
                
                if(glfwGetKey(player.windows[i]->glfw_window, 
                    GLFW_KEY_LEFT_SHIFT) || 
                   glfwGetKey(player.windows[i]->glfw_window,
                    GLFW_KEY_LEFT_CONTROL))
                {
                    // go forward 10 seconds
                    int64_t target = now + 10 * AV_TIME_BASE;
                    cout << "RAW SEEK RIGHT: " << target << '\n';
                    player.seek(target);
                }
                else
                {
                    theta -= TURN / 25;
                    send_pos = true;
                }
            }
            
            right_down = true;
        }
        else
            right_down = false;
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_UP))
        {
            if(!up_down)
            {
                phi += (TURN/4) / 10.0;
                send_pos = true;
            }
            
            up_down = true;
        }
        else
            up_down = false;
        
        if(glfwGetKey(player.windows[i]->glfw_window, GLFW_KEY_DOWN))
        {
            if(!down_down)
            {
                phi -= (TURN/4) / 10.0;
                send_pos = true;
            }
            
            down_down = true;
        }
        else
            down_down = false;
      } // end of for each window
              
        theta = fmod(theta, TURN);
        if(theta < 0)
            theta += TURN;
        //printf("%0.0f\n", 180 / M_PI * theta);
        
        if(phi > TURN/4)
            phi = TURN/4;
        else if(phi < -TURN/4)
            phi = -TURN/4;

        
        if(send_pos && player.server)
        {
            Message turn;
            turn.write_byte('T');
            turn.write_float(phi);
            turn.write_float(theta);
            
            server->send(turn);
        }
        
        player.audio.lock();
        player.audio.direction = fmod(theta, TURN);
        player.audio.unlock();
        
        QuadBinaural qb(player.audio.direction);
        //printf("dir = %0.3f, A = %d, B = %d, fa = %0.3f, fb = %0.3f\n",
        //    player.audio.direction / TURN,
        //    qb.A, qb.B, qb.fraction_a, qb.fraction_b);
        
        decoder.lock();
        decoded_all = decoder.decoded_all_flag;
        decoder.unlock();
        
        int64_t now_prev = now;
        
        if(!player.paused)
        {
            #ifndef NO_AUDIO
            if(player.audio.setup_state == AuSS_PLAYING)
            {
                // sync video to audio timestamp
                player.audio.lock();
                now = player.audio.now;
                player.audio.unlock();
                
                // convert from audio timestamp (sample count) to microseconds
                now = 1000000.0/(2*player.audio.sample_rate) * now;
            }
            else
            {
                now = av_gettime_relative() - start;
            }
            #else
            now = av_gettime_relative() - start;
            #endif
        }
        
        // if we've jumped backwards more than a second, seek video
        // audio sync loop fix
        if(now < now_prev && abs(now - now_prev ) > 1000000)
        {
            player.seek(now);
        }
        
        double now_f = now / 1000000.0;

        // send a server time sync every 1/10th of a second
        if(server && abs(now-last_server_seek) > AV_TIME_BASE/10)
        {
            //cout << "Sync\n";
            Message seek;
            seek.write_char('N');
            seek.write_int64(now);
            server->send(seek);
            last_server_seek = now;
        }
        
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
                    // frame size attached to video path for multicast clients
                    Message path;
                    Message seek;
                    
                    path.write_char('P');
                    path.write_string(player.video_path);
                    path.write_uint32(decoder.codec_context->width);
                    path.write_uint32(decoder.codec_context->height);
                    
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
                    case 'X':
                    {
                        player.paused = !player.paused;
                    }
                    break;
                    
                    case 'P':
                    {
                        cerr << "Network path value sent at inappropriate time\n";
                        continue;
                    }
                    break;
                    
                    case 'S':
                    {
                        int64_t value = m.read_int64();
                        //decoder.seek(value);
                        player.seek(value);
                    }
                    break;
                    
                    case 'T':
                    {
                        phi = m.read_float();
                        theta = m.read_float();
                        //cout << "T";
                    }
                    break;
                    
                    // seek with just 'now' value
                    case 'N':
                    {
                        int64_t server_now = m.read_int64();
                        start = av_gettime_relative() - server_now;
                        now = server_now;
                    }
                    break;
                    
                    case 'K':
                    {
                        if(server)
                            break;
                        
                        if(shader_program == &Window_::mono_equirect_program)
                            shader_program = &Window_::aa_mono_equirect_program;
                        else
                            shader_program = &Window_::mono_equirect_program;
                        
                        for(size_t i = 0; i < player.windows.size(); i++)
                        {
                            //glfwMakeContextCurrent(player.windows[i]);
                            player.windows[i]->make_current();
                            glUseProgram(player.windows[i]->*shader_program);
                            glEnable(GL_TEXTURE_2D);
                        }
                        //glfwMakeContextCurrent(player.windows[0]);
                        player.windows[0]->make_current();
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
        
        DecoderFrame show_frame;
        DecoderFrame show_frame_prev;
        
//        if(seek_flag)
//        {
//            cout << "Seek flag!\n";
//            seek_flag = false;
//            
//            if(show_frame.frame)
//                decoder.return_frame(show_frame);
//                
//            if(show_frame_prev.frame)
//                decoder.return_frame(show_frame_prev);
//            
//            show_frame = show_frame_prev = NULL;
//            current_frame_end = 0;
//        }
        
    if(!(player.type == NT_CLIENT && player.use_multicast)) {
        while(seek_flag || now_f > current_frame_end)
        //while(now_f > current_frame_end)
        {
            show_frame_prev = show_frame;
            show_frame = decoder.get_frame();
            
            // adjust time if we're seeking and got a seek frame
            if(show_frame.frame && show_frame.seek_result)
            {
                int64_t new_now = av_rescale(
                    show_frame.frame->pts,
                    decoder.time_base.num,
                    decoder.time_base.den);
                
                new_now *= AV_TIME_BASE;
                start = av_gettime_relative() - new_now;
                now = new_now;
                
                seek_flag = false;
                cout << "SEEK DETECTED, NEW NOW: " << now << '\n';
            }
            
            if(show_frame.frame && show_frame_prev.frame)
            {
                decoder.return_frame(show_frame_prev);
                show_frame_prev.frame = NULL;
            }
            
            if(!show_frame.frame && decoded_all)
                goto end_of_video; // end of video
            
            if(!show_frame.frame)
            {
                //cerr << "Playing faster than decode\n";
                break;
            }
            
            current_frame_end = show_frame.frame->pts;
            current_frame_end /= decoder.time_base.den;
            current_frame_end *= decoder.time_base.num;
            
//            cout << "frame end: " << current_frame_end << '\n';
//            cout << "now_f:     " << now_f << '\n';
        }
        
        if(!show_frame.frame && !show_frame_prev.frame)
        {
            // no new frames ready
            //continue;
            goto REDRAW;
        }
        
        if(!show_frame.frame && show_frame_prev.frame)
        {
            // lagged frame is ready, but current data not available
            // show it anyway as current frame (better late than nothing)
            //cout << "lagged\n";
            show_frame.frame = show_frame_prev.frame;
            show_frame_prev.frame = NULL;
        }
        
        //printf("Frame end: %f, NOW: %f\n", current_frame_end, now_f);
        
//        // fill bottom half with red (Debug tool)
//        for(size_t y = decoder.codec_context->height/2; 
//            y < decoder.codec_context->height;
//            y++)
//        {
//            for(size_t x = 0; x < decoder.codec_context->width; x++)
//            {
//                show_frame.frame->data[0][
//                    3*(y*decoder.codec_context->width + x)+0] = 0xFF;
//                    
//                show_frame.frame->data[0][
//                    3*(y*decoder.codec_context->width + x)+1] = 0x00;
//                    
//                show_frame.frame->data[0][
//                    3*(y*decoder.codec_context->width + x)+2] = 0x00;
//            }
//        }
        
    } // only update decoder if not client multicast
    
        //glfwMakeContextCurrent(player.windows[0]);
        //player.windows[0]->make_current();
        
        if(player.use_multicast && player.type == NT_CLIENT)
        {
            // clients running multicast should load frame from mc_client
            player.mc_client.player_poll();
            
            for(size_t i = 0; i < player.windows.size(); i++)
            {
                player.windows[i]->make_current();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                    player.mc_client.width, player.mc_client.height,
                    0, GL_RGB, GL_UNSIGNED_BYTE, &player.mc_client.buffer[0][0]);
            }
        }
        else
        {
            for(size_t i = 0; i < player.windows.size(); i++)
            {
                player.windows[i]->make_current();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                    decoder.codec_context->width, decoder.codec_context->height,
                    0, GL_RGB, GL_UNSIGNED_BYTE, show_frame.frame->data[0]);
            }
        }
        
        if(player.use_multicast && player.type == NT_SERVER)
        {
            // servers running multicast need to transmit frame data
            size_t size = decoder.codec_context->width;
            size *= decoder.codec_context->height;
            size *= 3; // RGB
            
            player.mc_server.send(size, show_frame.frame->data[0]);
        }
        
        
        //glGenerateMipmap(GL_TEXTURE_2D);
        
        if(dump_frame_flag)
        {
            SaveFrame(show_frame.frame, 
                decoder.codec_context->width, 
                decoder.codec_context->height,
                0);
            
            dump_frame_flag = false;
        }
        
        decoder.return_frame(show_frame);
        
REDRAW:        
        for(size_t i = 0; i < player.windows.size(); i++)
        {
            //glfwMakeContextCurrent(player.windows[i]);
            player.windows[i]->make_current();
            GLuint sp = player.windows[i]->*shader_program;
            
            GLint theta_ = glGetUniformLocation(sp, "theta");
            GLint phi_   = glGetUniformLocation(sp, "phi");
            GLint pos = glGetAttribLocation(sp, "pos_in");
            
            GLint roll_    = glGetUniformLocation(sp, "roll");
            GLint pitch_   = glGetUniformLocation(sp, "pitch");
            GLint heading_ = glGetUniformLocation(sp, "heading");
            GLint originX_ = glGetUniformLocation(sp, "originX");
            GLint originY_ = glGetUniformLocation(sp, "originY");
            GLint originZ_ = glGetUniformLocation(sp, "originZ");
            GLint width_   = glGetUniformLocation(sp, "width");
            GLint height_  = glGetUniformLocation(sp, "height");
            
            // for 'pos', using convention where x goes to the right, y goes in,
            // and z goes up.
            glUniform1f(theta_, theta);
            glUniform1f(phi_, phi);
            
            glUniform1f(roll_, player.screen_config[i].roll);
            glUniform1f(pitch_, player.screen_config[i].pitch);
            glUniform1f(heading_, player.screen_config[i].heading);
            glUniform1f(originX_, player.screen_config[i].originX);
            glUniform1f(originY_, player.screen_config[i].originY);
            glUniform1f(originZ_, player.screen_config[i].originZ);
            glUniform1f(width_, player.screen_config[i].width);
            glUniform1f(height_, player.screen_config[i].height);
            
            if(player.stereo_type == STEREO_TOP_BOTTOM_INTERLEAVED)
            {
                // top/bottom stereo
                GLint stereo_  = 
                    glGetUniformLocation(sp, "stereo_half");
                    
                glUniform1f(stereo_, 1.0);
                glBegin(GL_QUADS);
                    glVertexAttrib3f(pos, -1, 1, 0);
                    glVertex2f(-1, 1);
                    glVertexAttrib3f(pos, -1, -1, 0);
                    glVertex2f(-1, -1);
                    glVertexAttrib3f(pos, 1, -1, 0);
                    glVertex2f(1,-1);
                    glVertexAttrib3f(pos, 1, 1, 0);
                    glVertex2f(1,1);
                glEnd();
                    
                glUniform1f(stereo_, 0.0);
                glBegin(GL_QUADS);
                    glVertexAttrib3f(pos, -1, 1, 0);
                    glVertex2f(-1, 1);
                    glVertexAttrib3f(pos, -1, -1, 0);
                    glVertex2f(-1, -1);
                    glVertexAttrib3f(pos, 1, -1, 0);
                    glVertex2f(1,-1);
                    glVertexAttrib3f(pos, 1, 1, 0);
                    glVertex2f(1,1);
                glEnd();
            }
            else if(server || !player.stereo)
            {
                if(player.stereo_type == STEREO_HALF_TOP)
                {
                    GLint stereo_  = 
                        glGetUniformLocation(sp, "stereo_half");
                    
                    glUniform1f(stereo_, 1.0);
                }
                else if(player.stereo_type == STEREO_HALF_BOTTOM)
                {
                    GLint stereo_  = 
                        glGetUniformLocation(sp, "stereo_half");
                        
                    glUniform1f(stereo_, 0.0);                
                }
            
                glBegin(GL_QUADS);
                    // top left
                    //glVertexAttrib3f(pos, -11, 2*1.5, 6);
                    glVertexAttrib3f(pos, -1, 1, 0);
                    glTexCoord2f(0, 0); // only used by server
                    glVertex2f(-1, 1);
                    
                    // bottom left
                    //glVertexAttrib3f(pos, -11, 2*1.5, -6);
                    glVertexAttrib3f(pos, -1, -1, 0);
                    glTexCoord2f(0, 1);
                    glVertex2f(-1, -1);

                    // bottom right
                    //glVertexAttrib3f(pos, 11, 2*1.5, -6);
                    glVertexAttrib3f(pos, 1, -1, 0);
                    glTexCoord2f(1,1);
                    glVertex2f(1,-1);
                    
                    // top right
                    //glVertexAttrib3f(pos, 11, 2*1.5, 6);
                    glVertexAttrib3f(pos, 1, 1, 0);
                    glTexCoord2f(1, 0);
                    glVertex2f(1,1);            
                glEnd();
            }
            else
            {
                // top/bottom stereo
                GLint stereo_  = 
                    glGetUniformLocation(sp, "stereo_half");
                    
                glUniform1f(stereo_, 1.0);
                glBegin(GL_QUADS);
                    glVertexAttrib3f(pos, -1, 1, 0);
                    glVertex2f(-1, 1);
                    glVertexAttrib3f(pos, -1, -1, 0);
                    glVertex2f(-1, 0);
                    glVertexAttrib3f(pos, 1, -1, 0);
                    glVertex2f(1,0);
                    glVertexAttrib3f(pos, 1, 1, 0);
                    glVertex2f(1,1);
                glEnd();
                    
                glUniform1f(stereo_, 0.0);
                glBegin(GL_QUADS);
                    glVertexAttrib3f(pos, -1, 1, 0);
                    glVertex2f(-1, 0);
                    glVertexAttrib3f(pos, -1, -1, 0);
                    glVertex2f(-1, -1);
                    glVertexAttrib3f(pos, 1, -1, 0);
                    glVertex2f(1,-1);
                    glVertexAttrib3f(pos, 1, 1, 0);
                    glVertex2f(1,0);
                glEnd();
            }            
        }
        
        // swap all together after drawing
        for(size_t i = 0; i < player.windows.size(); i++)
        {
            player.windows[i]->swap_buffers();
        }
    }
    
end_of_video:
    cout << "end of video detected\n";
    decoder.set_quit();
    decoder.join();
    
    if(server)
        joystick.shutdown();
    
    glfwTerminate();
}

