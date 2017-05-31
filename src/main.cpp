extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <iostream>
#include <cstdio>
#include <cstdlib>
using namespace std;

#include <GLFW/glfw3.h>

// USE FFMPEG 3.3.1


// see http://dranger.com/ffmpeg/tutorial01.html for reference

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;
  
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

void on_window_resize(GLFWwindow* window, int w, int h)
{
    glViewport(0,0,w,h);
}

int main(int argc, char* argv[]) 
{
    if(argc != 2)
    {
        cerr << "USAGE: ./video_sphere <path-to-video>\n";
        return EXIT_FAILURE;
    }

    av_register_all();
    avcodec_register_all();
    
    
    GLFWwindow* window;
    
    if(!glfwInit())
    {
        glfwTerminate();
        cerr << "Failed to init GLFW3\n";
        return EXIT_FAILURE;
    }
    
    window = glfwCreateWindow(640, 360, "Video Sphere", NULL, NULL);
    if(!window)
    {
        glfwTerminate();
        cerr << "Failed to open window\n";
        return EXIT_FAILURE;
    }
    
    glfwMakeContextCurrent(window);
    
    glfwSetWindowSizeCallback(window, on_window_resize);
    
    AVFormatContext* format_context = NULL;
    
    if(avformat_open_input(&format_context, argv[1], NULL, NULL) != 0)
    {
        cerr << "Failed to open file\n";
        return EXIT_FAILURE;
    }
    
    if(avformat_find_stream_info(format_context, NULL) < 0)
    {
        cerr << "Failed to determine stream info\n";
        return EXIT_FAILURE;
    }
    
    // debug
    av_dump_format(format_context, 0, argv[1], 0);
    
    AVCodecContext* codec_context = NULL;
    
    int video_stream = -1;
    
    for(int i = 0;i < format_context->nb_streams; i++)
    {
        if(format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream = i;
            break;
        }
    }
    
    if(video_stream == -1)
    {
        cerr << "Failed to find video stream\n";
        return EXIT_FAILURE;
    }
    
    AVCodec* codec = NULL;
    codec = avcodec_find_decoder(
        format_context->streams[video_stream]->codecpar->codec_id);
    
    if(codec == NULL)
    {
        cerr << "Unsupported codec\n";
        return EXIT_FAILURE;
    }
    
    cout << "CODEC IS: " << codec->name << '\n';
    
    codec_context = avcodec_alloc_context3(codec);
    
    if(!codec_context)
    {
        cerr << "Failed to allocate codec context!\n";
        return EXIT_FAILURE;
    }
    
    avcodec_parameters_to_context(codec_context,
        format_context->streams[video_stream]->codecpar);
    
    if(avcodec_open2(codec_context, codec, NULL) < 0)
    {
        cerr << "Couldn't open codec\n";
        return EXIT_FAILURE;
    }
    
    AVFrame* frame = NULL;
    AVFrame* frame_rgb = NULL;
    
    frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    
    uint8_t* buffer = NULL;
    int num_bytes;
    
    num_bytes = avpicture_get_size(AV_PIX_FMT_RGB24, 
        codec_context->width, codec_context->height);
    
    buffer = (uint8_t*)av_malloc(num_bytes*sizeof(uint8_t));
    
    avpicture_fill((AVPicture*)frame_rgb, buffer, AV_PIX_FMT_RGB24,
        codec_context->width, codec_context->height);
    
    struct SwsContext* sws_context = NULL;
    int frame_finished;
    AVPacket packet;
    
    sws_context = sws_getContext(
        codec_context->width,
        codec_context->height,
        
        // only use one of two following lines
        //AV_PIX_FMT_YUV420P,
        codec_context->pix_fmt, // -- WTF, why does this break?!
        
        codec_context->width,
        codec_context->height,
        AV_PIX_FMT_RGB24,
        
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);
    
    glEnable(GL_TEXTURE_2D);
    GLuint tex;
    glGenTextures(1, &tex);
    
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // FIXME
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int i = 0;
    while(av_read_frame(format_context, &packet) >= 0)
    {
        if(glfwGetKey(window, GLFW_KEY_ESCAPE))
            break;
        
        if(packet.stream_index == video_stream)
        {
            avcodec_decode_video2(codec_context, frame, &frame_finished, 
                &packet);
            
            if(frame_finished)
            {
                sws_scale(
                    sws_context, 
                    (uint8_t const* const*)frame->data,
                    frame->linesize,
                    0,
                    codec_context->height,
                    frame_rgb->data,
                    frame_rgb->linesize);
                
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                    codec_context->width, codec_context->height,
                    0, GL_RGB, GL_UNSIGNED_BYTE, frame_rgb->data[0]);
                
//                if(++i <= 5)
//                    SaveFrame(frame_rgb, codec_context->width,
//                        codec_context->height, i);
                
                glBegin(GL_QUADS);
                {
                    glTexCoord2f(0,1);
                    glVertex2f(-1,-1);
                    
                    glTexCoord2f(0,0);
                    glVertex2f(-1,1);
                    
                    glTexCoord2f(1,0);
                    glVertex2f(1,1);
                    
                    glTexCoord2f(1,1);
                    glVertex2f(1,-1);
                }
                glEnd();
                
                glfwSwapBuffers(window);
                glfwPollEvents();
            }
        }
        
        // free packet allocated by av_read_frame
        av_free_packet(&packet);
    }
    
    av_free(buffer);
    av_free(frame_rgb);
    av_free(frame);
    
    avcodec_close(codec_context);
    
    avformat_close_input(&format_context);
    
    glfwTerminate();
}

