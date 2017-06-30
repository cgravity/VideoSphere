#include "ps3_joystick.h"

#include <pthread.h>

#include "oscpack_1_1_0/osc/OscReceivedElements.h"
#include "oscpack_1_1_0/osc/OscPacketListener.h"
#include "oscpack_1_1_0/osc/OscPrintReceivedElements.h"
#include "oscpack_1_1_0/ip/UdpSocket.h"

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <iostream>
using namespace std;


#define PORT 7000

struct PS3_NetworkThread : public osc::OscPacketListener
{
    UdpListeningReceiveSocket listener;
    
    pthread_t js_thread;
    pthread_mutex_t mutex;
    JS_State state;
    
    // buffer for parsing input into
    // preserved to avoid dynamic memory allocation
    JS_State tmp_state;
    
    void set_state(JS_State& from)
    {
        pthread_mutex_lock(&mutex);
            state.buttons.swap(from.buttons);
            state.axes.swap(from.axes);
            state.valid = from.valid;
            
            from.buttons.clear();
            from.axes.clear();
            from.valid = false;
        pthread_mutex_unlock(&mutex);
    }
    
  public:
    PS3_NetworkThread() : listener(
        IpEndpointName( IpEndpointName::ANY_ADDRESS, PORT ),
        this)
    {
    }
    
    void get_state(JS_State& into)
    {
        pthread_mutex_lock(&mutex);
            if(state.valid)
            {
                into.buttons.swap(state.buttons);
                into.axes.swap(state.axes);
                into.valid = state.valid;
                
                state.valid = false;
                state.buttons.clear();
                state.axes.clear();
            }
        pthread_mutex_unlock(&mutex);
    }
    
    
    virtual void run()
    {
        while(true)
        {
            try
            {
                listener.Run();
            }
            catch(osc::Exception& e)
            {
                cerr << "OSC Error: " << e.what() << '\n';
            }
        }
    }
    
    virtual void ProcessMessage(const osc::ReceivedMessage& msg_in, 
        const IpEndpointName& endpoint)
    {
        // reset read buffer
        tmp_state.valid = false;
        tmp_state.buttons.clear();
        tmp_state.axes.clear();
        
        // make sure message is for us
        if(strcmp(msg_in.AddressPattern(), "/jsdata") != 0)
        {
            return; // can't handle this message
        }
        
        osc::ReceivedMessageArgumentStream args = msg_in.ArgumentStream();
        
        // FIXME: Should probably be unsigned, but need to change server too...
        osc::int32 button_count;
        osc::int32 axes_count;
        
        args >> button_count;
        args >> axes_count;
        
        // Do as few memory allocations as possible; these reservations
        // should only run *once* when the first message arrives since the
        // packet size *should always have the same number of buttons and axes*
        
        // FIXME: Should probably put a sanity check on the reserve request size
        tmp_state.buttons.reserve(button_count);
        tmp_state.axes.reserve(axes_count);
        
        for(int i = 0; i < button_count; i++)
        {
            int pressed = 0;
            args >> pressed;
            
            tmp_state.buttons.push_back((pressed != 0));
        }
        
        for(int i = 0; i < axes_count; i++)
        {
            float axis = 0.0;
            args >> axis;
            
            tmp_state.axes.push_back(axis);
        }
        
        args >> osc::EndMessage;
        
        // if we got here, we parsed the message successfully -- so it's valid
        // (otherwise an exception would have been thrown)
        tmp_state.valid = true;
        set_state(tmp_state);
    }
};

static void* js_thread_main(void* arg)
{
    PS3_NetworkThread* thread = (PS3_NetworkThread*)arg;
    thread->run();
    
    return NULL;
}

void PS3Joystick::start()
{
    if(thread)
        return;
    
    thread = new PS3_NetworkThread();
    pthread_create(&thread->js_thread, NULL, js_thread_main, thread);
}

void PS3Joystick::shutdown()
{
    if(!thread)
        return;
        
    pthread_cancel(thread->js_thread);
    
    void* return_value;
    pthread_join(thread->js_thread, &return_value);
    
    delete thread;
    thread = NULL;
}

void JS_State::swap(JS_State& other)
{
    std::swap(valid, other.valid);
    
    buttons.swap(other.buttons);
    axes.swap(other.axes);
    
    std::swap(left_stick_x, other.left_stick_x);
    std::swap(left_stick_y, other.left_stick_y);
    std::swap(right_stick_x, other.right_stick_x);
    std::swap(right_stick_y, other.right_stick_y);
    std::swap(left_trigger, other.left_trigger);
    std::swap(right_trigger, other.right_trigger);
    
    std::swap(left_shoulder, other.left_shoulder);
    std::swap(right_shoulder, other.right_shoulder);
    
    std::swap(button_x, other.button_x);
    std::swap(button_select, other.button_select);
    std::swap(button_triangle, other.button_triangle);
    std::swap(button_circle, other.button_circle);
    std::swap(button_square, other.button_square);
    
    std::swap(button_start, other.button_start);
    std::swap(button_arrow_up, other.button_arrow_up);
    std::swap(button_arrow_down, other.button_arrow_down);
    std::swap(button_arrow_left, other.button_arrow_left);
    std::swap(button_arrow_right, other.button_arrow_right);
}

template<typename T>
T fancy_sqr(T x)
{
    if(x < 0)
        return -x*x;
    else
        return x*x;
}

void PS3Joystick::update(JS_State& result)
{    
    result.valid = false;
    
    if(!thread)
    {
        return;
    }
    
    thread->get_state(result);
    
    if(!result.valid)
        return;
    
    try {
        result.left_stick_x  = fancy_sqr(result.axes.at(0));
        result.left_stick_y  = fancy_sqr(-result.axes.at(1));
        result.right_stick_x = fancy_sqr(-result.axes.at(2));
        result.right_stick_y = fancy_sqr(-result.axes.at(3));
        
        result.left_trigger  = fancy_sqr(result.axes.at(12));
        result.right_trigger = fancy_sqr(result.axes.at(13));
        
        result.left_shoulder = result.axes.at(14);
        result.right_shoulder = result.axes.at(15);
    } 
    catch(std::out_of_range& e)
    {
        cerr << "Joystick: Invalid PS3 Axis\n";
        result.valid = false;
        return;
    }
    
    try {
        result.button_x = result.buttons.at(14);
        result.button_select = result.buttons.at(0);
        result.button_triangle = result.buttons.at(12);
        result.button_circle = result.buttons.at(13);
        result.button_square = result.buttons.at(15);
        
        result.button_start = result.buttons.at(3);
        result.button_arrow_up = result.buttons.at(4);
        result.button_arrow_left = result.buttons.at(7);
        result.button_arrow_right = result.buttons.at(5);
        result.button_arrow_down = result.buttons.at(6);
    }
    catch(std::out_of_range& e)
    {
        cerr << "Joystick: Invalid PS3 Button\n";
        result.valid = false;
        return;
    }
}

