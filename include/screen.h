#pragma once

#include<vector>
#include<string>

enum ScreenCreateMode
{
    SCM_GLFW,   // use GLFW to create the window
    SCM_X11     // use X11 directly to create the window
};

struct ScreenConfig
{
    // monitor index to use, -1 for default monitor (e.g. for server)
    int index;
    
    float width;        // millimeters
    float height;       // millimeters
    
    float heading;
    float pitch;
    float roll;
    
    float originX;
    float originY;
    float originZ;
    
    int pixel_width;
    int pixel_height;
    
    bool fullscreen;
    
    ScreenCreateMode mode;
    bool override_redirect;
    std::string display;  // used for X11 Display initialization (e.g. ":0.0")
    int x; // X11 position to create window. see also: pixel_width & pixel_size
    int y;
    
    ScreenConfig() : index(-1), heading(0), pitch(0), roll(0),
        originX(0), originY(0), originZ(0), pixel_width(640), pixel_height(320),
        fullscreen(true), override_redirect(false), mode(SCM_GLFW), x(0), y(0)
        {}
        
    void debug_print() const;
};

void parse_screen_config(
    std::vector<ScreenConfig>& screens_out, 
    std::string filename, 
    std::string host);

void parse_calvr_screen_config(
    std::vector<ScreenConfig>& screens_out, 
    std::string filename, 
    std::string host);


