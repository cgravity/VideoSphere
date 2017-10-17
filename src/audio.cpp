#ifndef NO_AUDIO

#include "audio.h"
#include <cstdio>
#include <cstring>
#include <cmath>
using namespace std;

static int audio_callback(
    const void* input, // ignore
    void* vp_out,
    unsigned long frame_count,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags statusFlags,
    void* user_data)
{
    // translate arguments into useful types
    float* out = (float*)vp_out;
    Audio* audio = (Audio*)user_data;
    
    audio->lock();
    
    for(size_t i = 0; i < 2*frame_count; i++)
    {
        if(audio->now + i < audio->samples.size())
        {
            *out++ = audio->samples[audio->now + i];
        }
        else
        {
            *out++ = 0;
        }
    }
    
    audio->now += 2*frame_count;
    
    audio->unlock();
    return paContinue;
}

void Audio::start()
{
    PaError err = Pa_OpenDefaultStream(
        &stream,
        0, // no input
        2, // stereo output
        paFloat32,
        sample_rate,
        paFramesPerBufferUnspecified,
        audio_callback,
        this);
    
    if(err != paNoError)
    {
        std::cerr << "PortAudio Error: " << Pa_GetErrorText(err) << '\n';
        return;
    }
    
    err = Pa_StartStream(stream);
    if(err != paNoError)
    {
        std::cerr << "PortAudio Error: " << Pa_GetErrorText(err) << '\n';
        return;
    }
    
    /*
    cerr << "SIZE: " << samples.size() << '\n';
    FILE* fp = fopen("debug.dat", "wb");
    fwrite(&samples[0], sizeof(samples[0]), samples.size(), fp);
    fclose(fp);
    */
}


#endif // ifndef NO_AUDIO

