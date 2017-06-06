#include "decoder.h"
#include <iostream>
using namespace std;

static void* decoder_thread_main(void* arg)
{
    Decoder* decoder = (Decoder*)arg;
    decoder->loop();    
    
    return NULL;
}


bool Decoder::open(const std::string& path)
{
    if(avformat_open_input(&format_context, path.c_str(), NULL, NULL) != 0)
    {
        perror("avformat_open_input");
        cerr << "PATH " << path << '\n';
        cerr << "Failed to open file\n";
        return false;
    }
    
    if(avformat_find_stream_info(format_context, NULL) < 0)
    {
        cerr << "Failed to determine stream info\n";
        return false;
    }
    
    // debug
    av_dump_format(format_context, 0, path.c_str(), 0);
    
    
    video_stream_index = -1;
    // FIXME: Audio stream
    
    for(int i = 0;i < format_context->nb_streams; i++)
    {
        if(format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }
    
    if(video_stream_index == -1)
    {
        cerr << "Failed to find video stream\n";
        return false;
    }
    
    codec = avcodec_find_decoder(
        format_context->streams[video_stream_index]->codecpar->codec_id);
    
    if(codec == NULL)
    {
        cerr << "Unsupported codec\n";
        return false;
    }
    
    //cout << "CODEC IS: " << codec->name << '\n';
    
    codec_context = avcodec_alloc_context3(codec);
    
    if(!codec_context)
    {
        cerr << "Failed to allocate codec context!\n";
        return false;
    }
    
    avcodec_parameters_to_context(codec_context,
        format_context->streams[video_stream_index]->codecpar);
    
    if(avcodec_open2(codec_context, codec, NULL) < 0)
    {
        cerr << "Couldn't open codec\n";
        return false;
    }
    
    time_base = format_context->streams[video_stream_index]->time_base;
    duration = format_context->streams[video_stream_index]->duration;
    number_of_frames = format_context->streams[video_stream_index]->nb_frames;
    
    return true;
} // Decoder::open

void Decoder::add_fillable_frames(size_t count)
{
    lock();
    
    for(size_t i = 0; i < count; i++)
    {
        AVFrame* frame = av_frame_alloc();
            
        uint8_t* buffer = NULL;
        int num_bytes;
        
        num_bytes = avpicture_get_size(AV_PIX_FMT_RGB24, 
            codec_context->width, codec_context->height);
        
        buffer = (uint8_t*)av_malloc(num_bytes*sizeof(uint8_t));
        
        avpicture_fill((AVPicture*)frame, buffer, AV_PIX_FMT_RGB24,
            codec_context->width, codec_context->height);
        
        fillable_frames.push_back(frame);
    }
    
    unlock();
}

void Decoder::start_thread()
{
    pthread_create(&decoder_thread, NULL, decoder_thread_main, this);
}

void Decoder::loop()
{
    AVFrame* yuv_frame = av_frame_alloc();
    AVFrame* rgb_frame = NULL;
    
    struct SwsContext* sws_context = NULL;
    int frame_finished;
    AVPacket packet;
    
    sws_context = sws_getContext(
        codec_context->width,
        codec_context->height,
        
        codec_context->pix_fmt,
        
        codec_context->width,
        codec_context->height,
        AV_PIX_FMT_RGB24,
        
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL);

    // --------------------------------------------------

    lock();

continue_point:         // loop start
    if(exit_flag)
    {
        unlock();
        pthread_exit(NULL);
    }
    
    rgb_frame = NULL;
    
    if(fillable_frames.size())
    {
        rgb_frame = fillable_frames.front();
        fillable_frames.pop_front();
    }
    
    if(!rgb_frame)
    {
        // buffer is totally full; wait until we need more data
        // or ~10ms max so we can check for exit flag
        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 10000000;
        
        // note: mutex already locked here
        wait(&timeout); // mutex unlocked while waiting
        // note: mutex automatically locked when returning from wait
        goto continue_point;
    }
    else
    {
        unlock();   // don't need to hold lock while spending time decoding
        
        while(av_read_frame(format_context, &packet) >= 0)
        {
            if(packet.stream_index == video_stream_index)
            {
                avcodec_decode_video2(codec_context, yuv_frame, 
                    &frame_finished, &packet);
                    
                if(frame_finished)
                {
                    sws_scale(
                        sws_context, 
                        (uint8_t const* const*)yuv_frame->data,
                        yuv_frame->linesize,
                        0,
                        codec_context->height,
                        rgb_frame->data,
                        rgb_frame->linesize);
                    
                    rgb_frame->pts = yuv_frame->pts;
                    rgb_frame->pkt_duration = yuv_frame->pkt_duration;
                    
                    goto finished_frame;
                }
            } // if packet.stream_index == video_stream_index
        } // while av_read_frame
        
        lock();
        decoded_all_flag = true; // reached end of data
        unlock();
        return;
        
finished_frame:
        lock(); // finished decoding a frame, so store it...
        showable_frames.push_back(rgb_frame);
        goto continue_point;
    }
    
} // void Decoder::loop()





