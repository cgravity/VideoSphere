#pragma once

#include<vector>
#include<string>

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
        
    void debug_print() const;
};

void parse_calvr_screen_config(
    std::vector<Screen>& screens_out, 
    std::string filename, 
    std::string host);


