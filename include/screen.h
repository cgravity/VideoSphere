#pragma once

#include<vector>
#include<string>

struct ScreenConfig
{
    // monitor index to use, -1 for default monitor (e.g. for server)
    int index;
    
    float width;
    float height;
    
    float heading;
    float pitch;
    float roll;
    
    float originX;
    float originY;
    float originZ;
    
    int pixel_width;
    int pixel_height;
    
    bool fullscreen;
    
    ScreenConfig() : index(-1), heading(0), pitch(0), roll(0),
        originX(0), originY(0), originZ(0), pixel_width(640), pixel_height(320),
        fullscreen(true)
        {}
        
    void debug_print() const;
};

void parse_calvr_screen_config(
    std::vector<ScreenConfig>& screens_out, 
    std::string filename, 
    std::string host);


