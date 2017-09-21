#include "window.h"
using namespace std;


void Window_::create_x11(
    const char* display_string, 
    const char* title,
    bool fullscreen,
    bool override_redirect,
    int x, 
    int y,
    int w,
    int h)
{
    display = XOpenDisplay(display_string);
    
    if(!display)
        fatal("create_x11: Failed to open X11 display");
    
    int screen = DefaultScreen(display);
    
    int visual_attribs[] = 
    {
        GLX_X_RENDERABLE,   True,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_DEPTH_SIZE,     24,
        GLX_STENCIL_SIZE,   8,
        GLX_DOUBLEBUFFER,   True,
        None
    };
    
    int fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(display, screen,
        visual_attribs, &fbcount);
    
    if(!fbc)
    {
        fatal("Could not retrieve framebuffer configs\n");
    }
    
    GLXFBConfig fb = fbc[0];
    
    XFree(fbc);
    
    XVisualInfo* vi = glXGetVisualFromFBConfig(display, fb);
    
    XSetWindowAttributes swa;
    swa.colormap = cmap = XCreateColormap(
        display,
        RootWindow(display, vi->screen),
        vi->visual,
        AllocNone);
    
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask | KeyPressMask;
    swa.override_redirect = (override_redirect? True : False);
    swa.cursor = None;
    
    x11_window = XCreateWindow(
        display, 
        RootWindow(display, vi->screen),
        x,y,w,h, 0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWBorderPixel|CWColormap|CWEventMask|CWOverrideRedirect|CWCursor, 
        &swa);
    
    if(!x11_window)
    {
        fatal("Failed to create window\n");
    }
    
    XFree(vi);
    XStoreName(display, x11_window, title);
    XMapRaised(display, x11_window);
    
    if(fullscreen)
    {
        Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
        Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
        
        XEvent xev;
        memset(&xev, 0, sizeof(xev));
        xev.type = ClientMessage;
        xev.xclient.window = x11_window;
        xev.xclient.message_type = wm_state;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = 1;
        xev.xclient.data.l[1] = fullscreen;
        xev.xclient.data.l[2] = 0;
        
        XSendEvent(display, DefaultRootWindow(display), False,
            SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }
        
    XFlush(display);
    
    glx_context = glXCreateNewContext(
        display,
        fb,
        GLX_RGBA_TYPE,
        NULL,
        True);
    
    if(glx_context == NULL)
    {
        printf("Failed to create GLX context!\n");
        exit(1);
    }
    
    XSync(display, False);
    
//    if(glXIsDirect(display, glx_context))
//        printf("Direct context\n");
//    else
//        printf("Indirect context\n");
    
    glXMakeCurrent(display, x11_window, glx_context);
}


