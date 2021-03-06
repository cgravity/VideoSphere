#include "player.h"
#include "util.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <iomanip>
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
    else if(type == NT_HEADLESS)
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
        client = new Client(server_address, VIDEO_SPHERE_PORT);
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
                            if(use_multicast)
                            {
                                mc_client.width = m.read_uint32();
                                mc_client.height = m.read_uint32();
                                mc_client.setup_data();
                            }
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
        
        if(!use_multicast)
        {
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
            cout << "Client wants multicast\n";
            mc_client.start_thread();
        }
    }
    else
    {
        cerr << "You must specify --client, --server, or --headless!\n";
        exit(EXIT_FAILURE);
    }
    
    if(looping)
    {
        decoder.looping = true;
    }
    
    #ifndef NO_AUDIO
    if(audio.setup_state == AuSS_READY_TO_PLAY)
    {
        audio.start();
    }
    #endif
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
    
    // multicast settings
    // need to wait to determine if we're client or server before setup
    string mc_group_ip;
    string mc_iface_ip;
    unsigned short mc_port = MULTICAST_DEFAULT_PORT;
    int mc_ttl = 1;
    
    cerr << "DEBUG arguments\n";
    for(int i = 0; i < argc; i++)
        cerr << "ARG " << i << " : " << argv[i] << '\n';
    
    bool config_mode_is_calvr = false;
    
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
        
        if(argv[i] == string("--headless"))
        {
            player.type = NT_HEADLESS;
            continue;
        }
        
        if(argv[i] == string("--mcgroup"))
        {
            i++;
            if(i >= argc)
                fatal("expected multicast group IP after --mcgroup");
            
            mc_group_ip = argv[i];
            continue;
        }
        
        if(argv[i] == string("--mciface"))
        {
            i++;
            if(i >= argc)
                fatal("expected multicast interface IP after --mciface");
            
            mc_iface_ip = argv[i];
            continue;
        }
        
        if(argv[i] == string("--mcport"))
        {
            i++;
            if(i >= argc)
                fatal("expected multicast port after --mcport");
            
            bool ok = parse_ushort(mc_port, argv[i]);
            if(!ok)
                fatal("Failed to parse multicast port");
            continue;
        }
        
        if(argv[i] == string("--mcttl"))
        {
            i++;
            if(i >= argc)
                fatal("expected multicast TTL after --mcttl");
            
            bool ok = parse_int(mc_ttl, argv[i]);
            if(!ok)
                fatal("Failed to parse multicast TTL");
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
        
        if(argv[i] == string("--calvr-config"))
        {
            i++;
            if(i >= argc)
                fatal("expected path to screen config xml after --calvr-config");
            
            player.config_path = argv[i];
            config_mode_is_calvr = true;
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
        
        if(argv[i] == string("--monitor"))
        {
            i++;
            if(i >= argc)
                fatal("expected explicit monitor index after --monitor");
            
            bool ok = parse_int(player.monitor, argv[i]);
            if(!ok)
                fatal("Failed to parse monitor index!");
            continue;
        }
        
        if(argv[i] == string("--stereo"))
        {
            player.stereo = true;
            player.stereo_type = STEREO_TOP_BOTTOM;
            continue;
        }
        
        if(argv[i] == string("--stereo-half"))
        {
            i++;
            if(i >= argc)
                fatal("expected 'top' or 'bottom' after --stereo-half");
            
            if(argv[i] == string("top"))
            {
                player.stereo_type = STEREO_HALF_TOP;
            }
            else if(argv[i] == string("bottom"))
            {
                player.stereo_type = STEREO_HALF_BOTTOM;
            }
            else
            {
                fatal("--stereo-half followed by something other than 'top' or 'bottom'");
            }
            
            continue;
        }
        
        if(argv[i] == string("--stereo-interleaved") || 
           argv[i] == string("--stereo-interlaced"))
        {
            player.stereo = true;
            player.stereo_type = STEREO_TOP_BOTTOM_INTERLEAVED;
            continue;
        }
        
        if(argv[i] == string("--loop") || argv[i] == string("looping"))
        {
            player.looping = true;
            continue;
        }
        
        #ifndef NO_AUDIO
        if(argv[i] == string("--audio"))
        {
            player.audio.setup_state = AuSS_START_DECODING;
        }
        #endif
        
        if(argv[i] == string("--suncave"))
        {
            player.suncave_workarounds = true;
        }
    }
    
    // sanity checks
    if(player.type == NT_UNDEFINED)
        fatal("must use one of --server, --client, or --headless as an argument!");
    
    if(player.type == NT_SERVER && player.video_path.size()==0)
        fatal("--video [path] is required for servers!");
    
    if(player.type == NT_HEADLESS && player.video_path.size()==0)
        fatal("--video [path] is required for headless mode!");
    
    if(player.type == NT_CLIENT && player.server_address.size()==0)
        fatal("clients must provide IP or hostname of server!");
    
    if(player.type == NT_CLIENT && player.config_path.size()==0)
        fatal("clients must provide --config [path] arguments!");
    
    if(player.type == NT_HEADLESS && player.config_path.size()==0)
        fatal("headless mode must provide --config [path] arguments!");
    
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
        if(config_mode_is_calvr)
        {
            parse_calvr_screen_config(
                player.screen_config, 
                player.config_path,
                player.hostname);
        }
        else
        {
            parse_screen_config(
                player.screen_config,
                player.config_path,
                player.hostname);
        }
        
        if(player.screen_config.size() == 0)
            fatal("Failed to find any screens in config file for host: " +
                player.hostname);
    }
    
    if(!mc_group_ip.empty())
    {
        if(player.type == NT_SERVER)
        {
            player.mc_server.setup_fd(
                mc_group_ip, 
                mc_iface_ip, 
                mc_port,
                mc_ttl);
        }
        else if(player.type == NT_CLIENT)
        {
            player.mc_client.setup_fd(
                mc_group_ip,
                mc_iface_ip,
                mc_port);
        }
        else
        {
            fatal("Must set client or server to use multicast settings");
        }
        
        player.use_multicast = true;
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
    
//    // workaround for starting over SSH on WAVE
    if(this->monitor >= 0)
    {
        if(this->monitor > screen_config.size())
            fatal("Requested monitor out of range");
        
        ScreenConfig sc = screen_config[this->monitor];
        sc.index = 0;
        screen_config.clear();
        screen_config.push_back(sc);
    }
    
    for(size_t i = 0; i < screen_config.size(); i++)
    {
        ScreenConfig& sc = screen_config[i];
        
        if(sc.mode == SCM_X11)
        {
            Window_* w = new Window_();
            w->create_x11(sc.display.c_str(), "Video Sphere", 
                sc.fullscreen, sc.override_redirect, sc.x,
                sc.y, sc.pixel_width, sc.pixel_height);
            windows.push_back(w);
            continue;
        }
        
        // If we got here, sc.mode == SCM_GLFW
        
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
            glfwWindowHint(GLFW_DECORATED, false);
            glfwWindowHint(GLFW_AUTO_ICONIFY, false);
            
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
            
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            
            glfwMakeContextCurrent(window);
            glViewport(0,0,mode->width,mode->height);
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
        
        if(!window)
            fatal("Failed to open window!");
        
        if(share_context == NULL)
            share_context = window;
        
        glfwSetWindowSizeCallback(window, on_window_resize);
        
        Window_* w = new Window_();
        w->glfw_window = window;
        windows.push_back(w);
    }
}

string describe_seek(int64_t target, int64_t duration, AVRational time_base)
{
    target /= AV_TIME_BASE;
    duration = av_rescale(duration, time_base.num, time_base.den);
    
    int64_t t_secs  = target;
    int64_t t_mins  = t_secs / 60.0;
    int64_t t_hours = t_mins / 60.0;
    
    t_secs %= 60;
    t_mins %= 60;
    
    int64_t d_secs  = duration;
    int64_t d_mins  = d_secs / 60.0;
    int64_t d_hours = d_mins / 60.0;
    
    d_secs %= 60;
    d_mins %= 60;
    
    stringstream ss;
    ss << std::setfill('0');
    
    // note: use d_hours here since we want to show if the duration
    // stretches into hours regardless of whether the current target time
    // is over or under an hour.
    if(abs(d_hours) > 0)
        ss << std::setw(2) << t_hours << ":";
    
    ss << std::setw(2) << t_mins << ":";
    ss << std::setw(2) << t_secs << " / ";

    if(abs(d_hours) > 0)
        ss << std::setw(2) << d_hours << ":";
    
    ss << std::setw(2) << d_mins << ":";
    ss << std::setw(2) << d_secs;
    
    return ss.str();
}

void Player::seek(int64_t target)
{
    /*
     *  == Explanation of time ==
     *
     *  now
     *      how far (in microseconds) that the current playback of the
     *      video has advanced.
     *
     *  start
     *      Value returned by av_gettime_relative() when playback started.
     *      now is calculated from it as: now = av_gettime_relative() - start.
     *
     *  av_seek_frame(...)
     *      requires timestamp in AVStream.time_base units
     *
     *  AVStream.time_base 
     *      Gives a fraction of a second (e.g. 1/30000) used to timestamp frames
     *      Timestamps are given as an integer multiple of this base unit. 
     *      It's done this way to avoid floating point math and all the 
     *      weirdness that would come with tracking timestamps like that.
     *      Unfortunately, it's not just a simple unit like microseconds 
     *      and probably varies from file to file.
     *
     *  AV_TIME_BASE
     *      The "internal timebase", which is set to the number of 
     *      microseconds in a second (i.e. 1000000) in the version of FFMPEG
     *      used here.
     */

    if(target < 0)
        target = 0;
    
    // find duration time in microseconds
    // note that time in decoder is in terms of the decoder time_base scale.
    // multiplying by the decoder scale gives time in seconds; to get
    // microseconds from seconds, we multiply by AV_TIME_BASE.
    int64_t duration_usecs = decoder.duration;
    duration_usecs = av_rescale(duration_usecs, 
        decoder.time_base.num, decoder.time_base.den);
    duration_usecs *= AV_TIME_BASE;
    
    if(target > duration_usecs)
        target = duration_usecs;
    
    if(server)
    {        
        Message seek;
        seek.write_char('S');
        seek.write_int64(target);
        
        server->send(seek);
        
        string time_description = describe_seek(
            target, decoder.duration, decoder.time_base);
        cout << "Seek to: " << time_description << "\n";
    }
    
    start = av_gettime_relative() - target;
    now = target;
    
    seek_flag = true;
    
    // now / AV_TIME_BASE = time to seek to, in seconds
    // time_base^-1 = frame-time-base ticks per second
    // so:
    //  now / AV_TIME_BASE * time_base^-1 = time to seek to in frame time base.
    // Note that the multiplication order below is shifted, but equivalent.
    int64_t seek_to = av_rescale(now, 
            decoder.time_base.den, decoder.time_base.num);    
    seek_to /= AV_TIME_BASE;
    decoder.seek(seek_to);
    
    #ifndef NO_AUDIO
    audio.seek(target);
    #endif
}

