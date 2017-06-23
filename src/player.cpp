#include "player.h"
#include "util.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
using namespace std;

void Player::start_threads()
{
    if(type == NT_SERVER)
    {
        server = new Server();
        nt = server;
        nt->start_thread();
            
        bool ok = decoder.open(video_path);
        
        if(!ok)
            exit(EXIT_FAILURE);
        
        decoder.add_fillable_frames(24*5);
        decoder.start_thread();
    }
    else if(type == NT_CLIENT)
    {
        client = new Client(server_address, 2345);
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
            }
        }
        
        // explicitly passed path overrides network request
        if(video_path.size() > 0)
        {
            cout << "Using explicit path override\n";
            path = video_path;
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
            exit(EXIT_FAILURE);   
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
}


void parse_args(Player& player, int argc, char* argv[])
{
    if(argc < 3)
    {
        cerr << "USAGE: ./video_sphere --server --video <path-to-video>\n";
        cerr << "or\n";
        cerr << "USAGE: ./video_sphere --client <hostname-or-ip> --config <path> [--host <host>]\n";
        exit(EXIT_FAILURE);
    }
    
    for(int i = 0; i < argc; i++)
    {
        if(argv[i] == string("--server"))
        {
            player.type = NT_SERVER;
            continue;
        }
        
        if(argv[i] == string("--client"))
        {
            i++;
            if(i >= argc)
                fatal("expected hostname or ip of server after --client");
            
            player.type = NT_CLIENT;
            player.server_address = argv[i];
            continue;
        }
        
        if(argv[i] == string("--video"))
        {
            i++;
            if(i >= argc)
                fatal("expected path to video after --video");
            
            player.video_path = argv[i];
            continue;
        }
        
        if(argv[i] == string("--config"))
        {
            i++;
            if(i >= argc)
                fatal("expected path to screen config xml after --config");
            
            player.config_path = argv[i];
            continue;
        }
        
        if(argv[i] == string("--host"))
        {
            i++;
            if(i >= argc)
                fatal("expected explicit hostname after --host");
            
            player.hostname = argv[i];
            continue;
        }
    }
    
    // sanity checks
    if(player.type == NT_UNDEFINED)
        fatal("must use one of --server or --client as an argument!");
    
    if(player.type == NT_SERVER && player.video_path.size()==0)
        fatal("--video [path] is required for servers!");
    
    if(player.type == NT_CLIENT && player.server_address.size()==0)
        fatal("clients must provide IP or hostname of server!");
    
    if(player.type == NT_CLIENT && player.config_path.size()==0)
        fatal("clients must provide --config [path] arguments!");
    
    
    if(player.hostname.size()==0)
    {
        // automatically get hostname if not explicitly specified
        char buffer[1024];
        gethostname(buffer, sizeof(buffer));
        buffer[sizeof(buffer)-1] = 0; // make sure null terminated
        player.hostname = buffer;
    }
    
    if(player.config_path.size())
    {
        parse_calvr_screen_config(
            player.screen_config, 
            player.config_path,
            player.hostname);
        
        if(player.screen_config.size() == 0)
            fatal("Failed to find any screens in config file for host: " +
                player.hostname);
    }
}

void on_window_resize(GLFWwindow* window, int w, int h)
{
    glViewport(0,0,w,h);
}

void Player::create_windows()
{
    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    
    if(monitor_count == 0)
    {
        cerr << "Failed to detect monitors!\n";
        exit(EXIT_FAILURE);
    }
    
    GLFWwindow* share_context = NULL;
    
    for(size_t i = 0; i < screen_config.size(); i++)
    {
        ScreenConfig& sc = screen_config[i];
        
        GLFWmonitor* monitor = NULL;
        GLFWwindow* window = NULL;
        
        if(sc.index >= 0)
        {
            if(sc.index < monitor_count)
                monitor = monitors[sc.index];
            else
                fatal("Monitor out of range");
        }
        
        if(sc.fullscreen && monitor)
        {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwWindowHint(GLFW_RED_BITS, mode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            
            window = glfwCreateWindow(
                mode->width,
                mode->height,
                "Video Sphere",
                monitor,
                share_context);
        }
        else
        {
            window = glfwCreateWindow(
                sc.pixel_width,
                sc.pixel_height,
                "Video Sphere",
                monitor,
                share_context);
        }
        
        if(share_context == NULL)
            share_context = window;
        
        glfwSetWindowSizeCallback(window, on_window_resize);
        windows.push_back(window);
    }
}

