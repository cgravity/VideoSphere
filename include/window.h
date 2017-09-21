#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cstdio>

#include "util.h"

// named with _ to avoid conflict with X11 "Window"
struct Window_
{
    // should only use ONE of GLFW or X11 window methods below
    
    // === GLFW details ===
    GLFWwindow* glfw_window;
    
    // === X11 details (direct usage) ===
    Display* display;
    Window x11_window;
    GLXContext glx_context;
    Colormap cmap;
    
    Window_()
    {
        glfw_window = NULL;
        display = NULL;
        x11_window = NULL;
    }
    
    void create_x11(
        const char* display_string = NULL, 
        const char* title = "Video Sphere",
        bool fullscreen=true,
        bool override_redirect=false,
        int x = 0, int y = 0, int w = 1920, int h = 1080);
    
    void close()
    {
        if(glfw_window)
        {
            glfwDestroyWindow(glfw_window);
            glfw_window = NULL;
        }
        else if(x11_window)
        {
            glXMakeCurrent(display, 0, 0);
            glXDestroyContext(display, glx_context);
            XDestroyWindow(display, x11_window);
            XFreeColormap(display, cmap);
            XCloseDisplay(display);
            
            x11_window = NULL;
            display = NULL;
        }
    }
    
    void make_current()
    {
        if(glfw_window)
            glfwMakeContextCurrent(glfw_window);
        else if(x11_window)
        {
            glXMakeCurrent(display, x11_window, glx_context);
        }
        else
            std::fprintf(stderr, "Can't make NULL window current!\n");
    }
    
    void swap_buffers()
    {
        make_current();
        
        if(glfw_window)
        {
            glfwSwapBuffers(glfw_window);
        }
        else
        {
            glXSwapBuffers(display, x11_window);
        }
    }
};


