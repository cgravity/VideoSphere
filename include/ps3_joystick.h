#pragma once

#include <vector>
#include <cstddef>

class PS3_NetworkThread;

struct JS_State
{
    bool valid;
    std::vector<bool>  buttons;
    std::vector<float> axes;
    
    float left_stick_x;
    float left_stick_y;
    float right_stick_x;
    float right_stick_y;
    float left_trigger;
    float right_trigger;
    
    float left_shoulder;
    float right_shoulder;
    
    int button_x;
    int button_select;
    int button_triangle;
    int button_circle;
    int button_square;
    int button_start;
    int button_arrow_left;
    int button_arrow_right;
    int button_arrow_up;
    int button_arrow_down;
                
    JS_State()
    {
        valid = false;
    }
    
    void swap(JS_State& other);
};

class PS3Joystick
{
    PS3_NetworkThread* thread;
    
  public:
    PS3Joystick() : thread(NULL) {}
    virtual ~PS3Joystick() { shutdown(); }
    
    virtual void start();
    virtual void shutdown();
    
    virtual void update(JS_State& result);
};

