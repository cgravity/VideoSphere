#pragma once

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/time.h>
}

#include <pthread.h>
#include <list>
#include <string>

#include <iostream>

struct Decoder
{
    pthread_mutex_t mutex;     // lock for exclusive access to rest of struct
    pthread_cond_t  condition; // signaled when decoding should continue
    
    pthread_t decoder_thread;

    bool exit_flag;
    bool decoded_all_flag;
    std::list<AVFrame*> showable_frames;
    std::list<AVFrame*> fillable_frames;
    
    AVFormatContext* format_context;
    AVCodecContext*  codec_context;
    AVCodec*         codec;
    
    AVRational time_base;
    int64_t duration; // of the whole stream in time_base units
    int64_t number_of_frames; // in the whole stream, or 0 if unknown
    
    int video_stream_index;
    int audio_stream_index; // FIXME: unused
    
    Decoder()
    {
        exit_flag = false;
        decoded_all_flag = false;
        
        mutex = PTHREAD_MUTEX_INITIALIZER;
        condition = PTHREAD_COND_INITIALIZER;
        
        format_context = NULL;
        codec_context  = NULL;
        codec          = NULL;
        
        video_stream_index = -1;
        audio_stream_index = -1;
    }
    
    // opens a video file, returning true if successful
    bool open(const std::string& file_path);
    
    void seek(int64_t seek_to)
    {
        pthread_mutex_lock(&mutex);
        av_seek_frame(format_context, video_stream_index, 
            seek_to, AVSEEK_FLAG_ANY);
        
//        if(avformat_seek_file(format_context, video_stream_index, 0,
//            seek_to, seek_to, 0) < 0)
//        {
//            std::cerr << "Seek failed\n";
//        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    void lock()
    {
        pthread_mutex_lock(&mutex);
    }
    
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }
    
    void signal()
    {
        pthread_cond_signal(&condition);
    }
    
    // must lock() before wait
    // mutex will be locked after returning from wait (regardless of why)
    void wait(struct timespec* max_wait = NULL)
    {
        if(max_wait == NULL)
        {
            pthread_cond_wait(&condition, &mutex);
        }
        else
        {
            pthread_cond_timedwait(&condition, &mutex, max_wait);
        }
    }
    
    // call this before starting the thread so that some work can buffer up
    // count indicates how many frames should be passed back and forth
    // between the queues
    void add_fillable_frames(size_t count);
    
    // gets the next frame available from the decoder
    // returns NULL if no frame is available (e.g. end of video, slow decode...)
    AVFrame* get_frame()
    {
        AVFrame* result = NULL;
        
        lock();
        
        if(showable_frames.size())
        {
            result = showable_frames.front();
            showable_frames.pop_front();
        }
        
        unlock();
        
        return result;
    }
    
    // returns a frame to the queue so it can be reused
    // this should be called when the clock passes the PTS + duration
    void return_frame(AVFrame* frame)
    {
        lock();
        fillable_frames.push_back(frame);
        unlock();
    }
    
    // starts the decoder thread and returns immediately
    void start_thread();
    
    // sets the flag to indicate to the decoder thread that it should quit
    void set_quit()
    {
        lock();
        exit_flag = true;
        unlock();
        
        signal();
    }
    
    // wait until the decoder thread finishes (call set_quit first!)
    void join()
    {
        void* return_value; // unused
        pthread_join(decoder_thread, &return_value);
    }
    
    // main loop for decoder thread
    // do not call this directly; it will be run indirectly by start_thread()
    void loop();
    
    // FIXME: Add support for seeking
    // FIXME: Add support for audio
};


