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

#include "rapidxml.hpp"

#define TURN (2*3.1415926535)

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

struct Screen
{
    int index;
    
    float width;
    float height;
    
    float heading;
    float pitch;
    float roll;
    
    float originX;
    float originY;
    float originZ;
    
    Screen() : index(0), heading(0), pitch(0), roll(0),
        originX(0), originY(0), originZ(0) {}
        
    void debug_print() const
    {
        cout << "Screen " << index << "\n"
             << "  width:   " << width << '\n'
             << "  height:  " << height << '\n'
             << "\n"
             << "  heading: " << heading << '\n'
             << "  pitch:   " << pitch << '\n'
             << "  roll:    " << roll << '\n'
             << "\n"
             << "  originX: " << originX << '\n'
             << "  originY: " << originY << '\n'
             << "  originZ: " << originZ << '\n';
    }
};

bool parse_float(float& into, char* from)
{
    return sscanf(from, "%f", &into) == 1;
}

bool parse_int(int& into, char* from)
{
    return sscanf(from, "%d", &into) == 1;
}

void fatal(string why)
{
    cerr << why << '\n';
    exit(EXIT_FAILURE);
}

void parse_float_attr(rapidxml::xml_node<char>* screen_node, float& into, string attr_name)
{
    rapidxml::xml_attribute<char>* attr;
    
    attr = screen_node->first_attribute(attr_name.c_str());
    if(!attr)
        fatal("Missing '" + attr_name +"' in screen config");
    
    if(!parse_float(into, attr->value()))
        fatal("Failed to parse '" + attr_name + "' in screen config");
}

void parse_int_attr(rapidxml::xml_node<>* screen_node, int& into, string attr_name)
{
    rapidxml::xml_attribute<char>* attr;
    
    attr = screen_node->first_attribute(attr_name.c_str());
    if(!attr)
        fatal("Missing '" + attr_name +"' in screen config");
    
    if(!parse_int(into, attr->value()))
        fatal("Failed to parse '" + attr_name + "' in screen config");
}

void parse_calvr_screen_config(vector<Screen>& screens_out, 
    string filename, string host)
{
    using namespace rapidxml;
    
    string xml_source = slurp(filename);
    
    xml_document<> doc;
    doc.parse<0>(&xml_source[0]);
    
    xml_node<>* node;
    
    for(node = doc.first_node("LOCAL"); node; node = node->next_sibling("LOCAL"))
    {
        xml_attribute<char>* attr = node->first_attribute("host");
        if(!attr)
            continue;
        
        if(host == attr->value())
        {
            break; // found node for this screen
        }
    }
    
    if(!node)
    {
        cerr << "Failed to find screen configuration for host: " << host << '\n';
        exit(EXIT_FAILURE);
    }
    
    xml_node<>* sc = node->first_node("ScreenConfig");
    
    if(!sc)
        fatal("Failed to find ScreenConfig");
    
    xml_node<>* screen_node = sc->first_node("Screen");
    
    if(!screen_node)
        fatal("Failed to find any Screen entries");
    
    while(screen_node)
    {
        Screen screen;
        
        parse_int_attr(screen_node, screen.index, "screen");
        
        parse_float_attr(screen_node, screen.width, "width");
        parse_float_attr(screen_node, screen.height, "height");

        parse_float_attr(screen_node, screen.heading, "h");
        parse_float_attr(screen_node, screen.pitch, "p");
        parse_float_attr(screen_node, screen.roll, "r");
        
        parse_float_attr(screen_node, screen.originX, "originX");
        parse_float_attr(screen_node, screen.originY, "originY");
        parse_float_attr(screen_node, screen.originZ, "originZ");
        
        screens_out.push_back(screen);
        screen_node = screen_node->next_sibling("Screen");
    }
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

void test_screen_parse()
{
    vector<Screen> screens;
    parse_calvr_screen_config(screens, "../../data/wave-full-screens.xml",
        "wave-2-2.local");
    
    
    cout << "Screen count: " << screens.size() << "\n";
    for(size_t i = 0; i < screens.size(); i++)
    {
        screens[i].debug_print();
    }
}

enum NetworkType
{
    NT_UNDEFINED,
    NT_SERVER,
    NT_CLIENT
};

struct Args
{
    NetworkType type;
    vector<Screen> screens; // used by client to list screen properties
    
    string video_path;     // used by server or client to indicate video path
                           // by default, clients will get this from server
                           // so should usually only be set on server.
                           // If set on client, client ignores path from server.
                           
    string server_address; // used by clients to indicate server ip/hostnam
    string config_path;    // used by clients to indicate path to CalVR config
    string hostname;       // used by clients to indicate name in CalVR config
};

void parse_args(Args& args, int argc, char* argv[])
{
    for(int i = 0; i < argc; i++)
    {
        if(argv[i] == string("--server"))
        {
            args.type = NT_SERVER;
            continue;
        }
        
        if(argv[i] == string("--client"))
        {
            i++;
            if(i >= argc)
                fatal("expected hostname or ip of server after --client");
            
            args.type = NT_CLIENT;
            args.server_address = argv[i];
            continue;
        }
        
        if(argv[i] == string("--video"))
        {
            i++;
            if(i >= argc)
                fatal("expected path to video after --video");
            
            args.video_path = argv[i];
            continue;
        }
        
        if(argv[i] == string("--config"))
        {
            i++;
            if(i >= argc)
                fatal("expected path to screen config xml after --config");
            
            args.config_path = argv[i];
            continue;
        }
        
        if(argv[i] == string("--host"))
        {
            i++;
            if(i >= argc)
                fatal("expected explicit hostname after --host");
            
            args.hostname = argv[i];
            continue;
        }
    }
    
    // sanity checks
    if(args.type == NT_UNDEFINED)
        fatal("must use one of --server or --client as an argument!");
    
    if(args.type == NT_SERVER && args.video_path.size()==0)
        fatal("--video [path] is required for servers!");
    
    if(args.type == NT_CLIENT && args.server_address.size()==0)
        fatal("clients must provide IP or hostname of server!");
    
    if(args.type == NT_CLIENT && args.config_path.size()==0)
        fatal("clients must provide --config [path] arguments!");
    
    
    if(args.hostname.size()==0)
    {
        // automatically get hostname if not explicitly specified
        char buffer[1024];
        gethostname(buffer, sizeof(buffer));
        buffer[sizeof(buffer)-1] = 0; // make sure null terminated
        args.hostname = buffer;
    }
    
    if(args.config_path.size())
    {
        parse_calvr_screen_config(
            args.screens, 
            args.config_path,
            args.hostname);
        
        if(args.screens.size() == 0)
            fatal("Failed to find any screens in config file for host: " +
                args.hostname);
    }
}

void monitor_test()
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    
    int x, y;
    
    cout << " --- Monitor Debug ---\n";
    
    for(int i = 0; i < count; i++)
    {
        cout << "Monitor " << i << ": " << glfwGetMonitorName(monitors[i])
             << '\n';
        
        glfwGetMonitorPos(monitors[i], &x, &y);
        
        cout << "Pixel pos: " << x << ", " << y << '\n';
        
        glfwGetMonitorPhysicalSize(monitors[i], &x, &y);
        
        cout << "Phys size: " << x << ", " << y << '\n';
    }
    
    cout << " --- End Monitor Debug ---\n";
}

int main(int argc, char* argv[]) 
{
    if(argc < 3)
    {
        cerr << "USAGE: ./video_sphere --server --video <path-to-video>\n";
        cerr << "or\n";
        cerr << "USAGE: ./video_sphere --client <hostname-or-ip> --config <path> [--host <host>]\n";
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

    Args args;
    parse_args(args, argc, argv);

    Server* server = NULL;
    Client* client = NULL;
    NetworkThread* nt = NULL;
    
    Decoder decoder;
    
    if(args.type == NT_SERVER)
    {
        server = new Server();
        nt = server;
        nt->start_thread();
            
        bool ok = decoder.open(args.video_path);
        
        if(!ok)
            return EXIT_FAILURE;    
        
        decoder.add_fillable_frames(24*5);
        decoder.start_thread();
    }
    else if(args.type == NT_CLIENT)
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
                
                try
                {
                    char type = m.read_char();
                    
                    switch(type)
                    {
                        case 'S':
                            now = m.read_int64();
                            start = av_gettime_relative() - now;
                            break;
                            
                        case 'P':
                            path = m.read_string();
                            break;
                    }
                }
                catch(ParseError pe)
                {
                    cerr << "Client start network parser error: " << pe.what() << '\n';
                }   
                
//                if(m.bytes[0] == 'S')
//                {
//                    if(m.size() != sizeof(int64_t) + 1)
//                    {
//                        cerr << "Invalid network seek at client start!\n";
//                        continue;
//                    }
//                    
//                    char* out = (char*)&now;
//                    char* in  = (char*)&m.bytes[1];
//                    
//                    for(size_t j = 0; j < sizeof(int64_t); j++)
//                        *out++ = *in++;
//                    
//                    start = av_gettime_relative() - now;
//                    
//                    continue;
//                }
//                
//                if(m.bytes[0] == 'P')
//                {
//                    path = m.as_string().c_str() + 1;
//                    continue;
//                }
            }
        }
        
        // explicitly passed path overrides network request
        if(args.video_path.size() > 0)
        {
            cout << "Using explicit path override\n";
            path = args.video_path;
        }
        else
        {
            cout << "Using network received path\n";
        }
            
        bool ok = decoder.open(path);
        
        if(!ok)
        {
            cerr << "Can't open that path!\n";
            cerr << "Path: " << path << '\n';
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
    else
    {
        cerr << "You must specify --client or --server!\n";
        exit(EXIT_FAILURE);
    }

    // 10 seconds of buffer is ~750MB if video size is 1920x1080
    
    GLFWwindow* window;
    
    if(!glfwInit())
    {
        glfwTerminate();
        cerr << "Failed to init GLFW3\n";
        return EXIT_FAILURE;
    }
    
    monitor_test();
    
    window = glfwCreateWindow(1920/2, 1080/2, "Video Sphere", NULL, NULL);
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
    
    //GLint no_distort_program = load_shaders(no_distort_files);
    
    
    vector<string> mono_equirect_files;
    mono_equirect_files.push_back("shaders/simple-mono.vert");
    mono_equirect_files.push_back("shaders/simple-mono.frag");
    
    GLint mono_equirect_program = load_shaders(mono_equirect_files);
    
    
    // select which shader to use
    GLint shader_program = mono_equirect_program;
    
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
    
    
    int i = 0;
    
    
    
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
        
        if(glfwGetKey(window, GLFW_KEY_ESCAPE))
            break;
        
        if(glfwGetKey(window, GLFW_KEY_LEFT))
        {
            if(!left_down)
                theta -= TURN / 25;
            
            left_down = true;
        }
        else
            left_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_RIGHT))
        {
            if(!right_down)
                theta += TURN / 25;
            
            right_down = true;
        }
        else
            right_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_UP))
        {
            if(!up_down)
                phi -= (TURN/4) / 10.0;
            
            up_down = true;
        }
        else
            up_down = false;
        
        if(glfwGetKey(window, GLFW_KEY_DOWN))
        {
            if(!down_down)
                phi += (TURN/4) / 10.0;
            
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
                    path.write_string(args.video_path);
                    
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
            glVertex2f(-1, 1);
            
            glVertexAttrib3f(pos, -11, 2*1.5, -6);
            glVertex2f(-1, -1);

            glVertexAttrib3f(pos, 11, 2*1.5, -6);
            glVertex2f(1,-1);
            
            glVertexAttrib3f(pos, 11, 2*1.5, 6);
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

