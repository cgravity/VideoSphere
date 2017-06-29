#pragma once

#include "screen.h"
#include "network.h"
#include "decoder.h"

#include <vector>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>


enum NetworkType
{
    NT_UNDEFINED,
    NT_SERVER,
    NT_CLIENT
};

struct Player
{
    NetworkType type;
    
    // one of server and client will be constructed based on type.
    // nt will point to it in a generic way.
    Server* server;
    Client* client;
    NetworkThread* nt;
    
    // video decoder thread
    Decoder decoder;
    
    // indicates the current timestamp and start timestamp of the video
    // in units understood by FFMPEG. (usually microseconds)
    // note: start holds a timestamp, while now is the count of microseconds
    // (or whatever) since start -- it does not include the current timestamp!
    int64_t start;
    int64_t now;
    
    // set to tell viewer code that seeking has happened
    // will be unset by display code once noticed
    bool seek_flag;
    
    // used by client to list screen properties
    std::vector<ScreenConfig> screen_config; 
    
    // windows created based on screen_config, in same order
    std::vector<GLFWwindow*> windows;
    
    // used by server or client to indicate video path
    // by default, clients will get this from server
    // so should usually only be set on server.
    // If set on client, client ignores path from server.
    std::string video_path;
    
    // used by clients to indicate server ip/hostname
    std::string server_address; 
    
    // used by clients to indicate path to CalVR config
    std::string config_path;
    
    // used by clients to indicate name in CalVR config
    std::string hostname;       
    
    int monitor;
    
    Player()
    {
        type = NT_UNDEFINED;
        
        server = NULL;
        client = NULL;
        nt = NULL;
        
        monitor = -1; // indicates create a window on all monitors listed
        
        seek_flag = false;
    }
    
    // starts up the networking and decoder based on type.
    // assumes arguments are fully parsed first.
    void start_threads();
    
    // opens GLFW windows based on settings in screen_config
    void create_windows();
    
    // seek to time (in microseconds)
    void seek(int64_t target);
};

void parse_args(Player& player, int argc, char** argv);


