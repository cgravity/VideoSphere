#ifndef NO_AUDIO

#include "audio.h"
#include <cstdio>
#include <cstring>
#include <cmath>
using namespace std;

#ifndef TURN
#define TURN (2*M_PI)
#endif

// Converts a direction in radians to a quad-binaural interpoloation value.
//
// Basically, quadraphonic binaural audio is made up of four stereo tracks
// spaced at each of four quarter turn positions (aka 0 degrees, 90 degrees,
// 180...). Each track contains the audio exactly as it should be played
// if the viewer were looking dead on in that direction. To play it back for
// other directions, we need to interpolate between the two nearest sources.
//
// This struct calculates the the interpolation fraction to use, and selects
// the indicies A and B for which tracks to play. (Note: the "tracks" here
// will probably be interleaved as a single *actual* audio track with 8 samples
// per frame for simplicity in the main audio routine -- at least initially --
// but were originally four different files.)
//
// The indicies produced assume that:
//      0 = facing directly forward
//      1 = facing left
//      2 = facing back
//      3 = facing right
//
// Likewise, the input angle value assumes 0 is straight forward, and that 
// the angle increases counter-clockwise. This convention probably won't 
// match all file sources, but I had to pick *something*.


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
    
    QuadBinaural qb;
    
    // only bother to compute these values if we're actually going to use them
    if(audio->mode == AM_QUAD_BINAURAL)
    {
        qb = QuadBinaural(audio->direction);
    }
    
    for(size_t i = 0; i < 2*frame_count; i++)
    {
        if(audio->paused)
        {
            *out++ = 0;
        }
        else if(audio->mode == AM_SIMPLE &&
            audio->now + i < audio->samples.size())
        {
            *out++ = audio->samples[audio->now + i];
        }
        else if(audio->mode == AM_QUAD_BINAURAL &&
            4*audio->now + i < audio->samples.size())
        {
            // This function was originally written assuming that we'd only have
            // stereo samples in the buffer -- since this variant needs 8 
            // channels we end up having to use 4*audio->now as the bounds 
            // check above as a result (audio->now is already multiplied by 2).
            // Similarly, we use 4 below instead of 8 since
            // we are alternating left/right output on each subsequent index i.
            //
            // TODO: rewrite this function to not make assumptions about 
            // the number of channels per frame and in the output.
        
            // order of samples is (audio channel - orientation):
            //  Left - Front 
            //  Left - Left
            //  Left - Back
            //  Left - Right
            //  Right - Front
            //  Right - Left
            //  Right - Back
            //  Right - Right
            
            float a = audio->samples[4*(audio->now+i) + qb.A];
            float b = audio->samples[4*(audio->now+i) + qb.B];
            *out++ = qb.fraction_a * a + qb.fraction_b * b;
            

// debug tool: cycle source file each second            
//                *out++ = audio->samples[4*(audio->now+i) + 
//                    (audio->now / 48000) % 4];

            // debug tool: left/right different behavior
            #if 0
            if((i&1) == 0)
            {
                *out++ = audio->samples[4*(audio->now+i) + 
                    (audio->now / 48000) % 4];
            }
            else
            {
                *out++ = audio->samples[4*(audio->now+i) + 
                    (audio->now / 48000) % 4];
            }
            #endif
        }
        else
        {
            *out++ = 0;
        }
    }
    
    if(!audio->paused)
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
    
    cerr << "AUDIO SIZE: " << samples.size()*sizeof(samples[0]) << " bytes\n";
    /*
    FILE* fp = fopen("debug.dat", "wb");
    fwrite(&samples[0], sizeof(samples[0]), samples.size(), fp);
    fclose(fp);
    */
    
    setup_state = AuSS_PLAYING;
}


#endif // ifndef NO_AUDIO

