#include "util.h"
#include "player.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace std;

#include <GLFW/glfw3.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}


string slurp(const string& filename)
{
    ifstream src(filename.c_str());
    
    if(src.fail())
    {
        cerr << "Failed to load: " << filename << "\n";
        exit(EXIT_FAILURE);
    }
    
    stringstream buffer;
    buffer << src.rdbuf();
    
    return buffer.str();
}

bool endswith(const string& data, const string& pattern)
{
    if(data.size() < pattern.size())
        return false;
    
    const char* data_str = data.c_str();
    data_str += data.size() - pattern.size();
    
    const char* pattern_str = pattern.c_str();
    while(*pattern_str)
    {
        if(tolower(*data_str) != tolower(*pattern_str))
            return false;
        
        pattern_str++;
        data_str++;
    }
    
    return true;
}


bool parse_float(float& into, char* from)
{
    return sscanf(from, "%f", &into) == 1;
}

bool parse_int(int& into, char* from)
{
    return sscanf(from, "%d", &into) == 1;
}

bool parse_ushort(unsigned short& into, char* from)
{
    return sscanf(from, "%hu", &into) == 1;
}

void fatal(string why)
{
    cerr << why << '\n';
    exit(EXIT_FAILURE);
}


// ---------------------------------------------------------------------------
void test_screen_parse()
{
    vector<ScreenConfig> screens;
    parse_calvr_screen_config(screens, "../../data/wave-full-screens.xml",
        "wave-2-2.local");
    
    
    cout << "Screen count: " << screens.size() << "\n";
    for(size_t i = 0; i < screens.size(); i++)
    {
        screens[i].debug_print();
    }
}

void monitor_test()
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    
    int x, y;
    
    cout << " --- Monitor Debug ---\n";
    
    for(int i = 0; i < count; i++)
    {
        cout << "Monitor " << i << ": " << glfwGetMonitorName(monitors[i])
             << '\n';
        
        glfwGetMonitorPos(monitors[i], &x, &y);
        
        cout << "Pixel pos: " << x << ", " << y << '\n';
        
        glfwGetMonitorPhysicalSize(monitors[i], &x, &y);
        
        cout << "Phys size: " << x << ", " << y << '\n';
    }
    
    cout << " --- End Monitor Debug ---\n";
}


// see http://dranger.com/ffmpeg/tutorial01.html for reference

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  cout << "save frame\n";
  
  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
  
  // Close file
  fclose(pFile);
}

void SaveFrame(unsigned char* bytes, int width, int height) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  cout << "save frame\n";
  
  // Open file
  sprintf(szFilename, "frame.ppm");
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  
  // Write pixel data
  fwrite(bytes, height, width*3, pFile);
  
  // Close file
  fclose(pFile);
}


string print_timestamp(int64_t time)
{
    time /= AV_TIME_BASE;

    int64_t t_secs  = time;
    int64_t t_mins  = t_secs / 60.0;
    int64_t t_hours = t_mins / 60.0;
    
    t_secs %= 60;
    t_mins %= 60;
    
    stringstream ss;
    ss << std::setfill('0');
    
    if(abs(t_hours) > 0)
        ss << std::setw(2) << t_hours << ":";
    
    ss << std::setw(2) << t_mins << ":";
    ss << std::setw(2) << t_secs;
    
    return ss.str();
}

