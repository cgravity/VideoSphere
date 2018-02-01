#pragma once
#ifndef NO_AUDIO

#include "portaudio.h"
#include <pthread.h>
#include <vector>
#include <iostream>
#include <stdint.h>

#include <cmath>
#ifndef TURN
#define TURN (2*M_PI)
#endif


struct QuadBinaural
{
    float fraction_a;
    float fraction_b;
    int A;
    int B;
    
    QuadBinaural() : fraction_a(0.0), fraction_b(0.0), A(0), B(0) {}
    
    QuadBinaural(float direction)
    {
        float dir = fmod(direction, TURN);
        
        if(dir < 0)
            dir += TURN;
        
        float theta = fmod(dir, TURN/4.0);
        fraction_b = 1.0 - theta / (TURN/4.0);
        fraction_a = 1.0 - fraction_b;
        
        B = int(floor(dir / (TURN/4.0))) % 4;
        A = (B+1) % 4;
    }
};

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
    
    // set once Audio::start() has been called
    AuSS_PLAYING
};

enum AudioMode
{
    // basic, traditional output (e.g. stereo @ 44.1k or 48k)
    // this is the default mode.
    AM_SIMPLE,  
    
    // samples represent four stereo recordings (one for each of
    // 0, 90, 180, and 270 degree orientations). Audio playback is interpolated
    // based on the "direction" angle specified in the audio struct.
    AM_QUAD_BINAURAL
};

struct Audio
{
    pthread_mutex_t mutex;
    
    std::vector<float> samples;
    int sample_rate; // samples per second
    int now; // in samples -- assuming stereo samples.
    int samples_per_frame; // 2 for stereo, 8 for QB
    
    bool paused;
    
    PaStream* stream;
    AudioSetupState setup_state;
    
    AudioMode mode;
    
    float direction; // angle, in radians, for AM_QUAD_BINAURAL
    
    Audio()
    {    
        mutex = PTHREAD_MUTEX_INITIALIZER;
        sample_rate = 44100;
        now = 0;
        stream = NULL;
        setup_state = AuSS_NO_AUDIO;
        
        mode = AM_SIMPLE;
        direction = 0.0;
        samples_per_frame = 2;
        
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

