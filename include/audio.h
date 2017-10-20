#pragma once
#ifndef NO_AUDIO

#include "portaudio.h"
#include <pthread.h>
#include <vector>
#include <iostream>
#include <stdint.h>

enum AudioSetupState
{
    // one of these two states will be set by the time
    // the decoder thread starts
    AuSS_NO_AUDIO,
    AuSS_START_DECODING,
    
    // if START_DECODING was set, state will be changed to DECODING
    // in the decoder thread and samples[] will be filled with the
    // entire audio contents of the file
    AuSS_DECODING,
    
    // once decoding is done, READY_TO_PLAY will be set by the decoder
    // thread to communicate to the main thread that playing can start
    AuSS_READY_TO_PLAY,
    
    // set by the main thread after calling open_stream() and start_stream()
    AuSS_PLAYING
};

struct Audio
{
    pthread_mutex_t mutex;
    
    std::vector<float> samples;
    int sample_rate; // samples per second
    int now; // in samples
    
    bool paused;
    
    PaStream* stream;
    AudioSetupState setup_state;
    
    Audio()
    {    
        mutex = PTHREAD_MUTEX_INITIALIZER;
        sample_rate = 44100;
        now = 0;
        stream = NULL;
        setup_state = AuSS_NO_AUDIO;
        
        paused = false;
        
        PaError err = Pa_Initialize();
        
        if(err != paNoError)
        {
            std::cerr << "PortAudio Error: " << Pa_GetErrorText(err) << '\n';
            return;
        }
    }
    
    ~Audio()
    {
        Pa_Terminate();
    }
    
    void lock()
    {
        pthread_mutex_lock(&mutex);
    }
    
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }
    
    void seek(int64_t us)
    {
        lock();
            double when = us / 1000000.0;
            when = 2 * sample_rate * when;
            now = when;
            std::cout << "Audio seek to: " << now << '\n';
        unlock();
    }
    
    void start();
};

#endif // ifndef NO_AUDIO

