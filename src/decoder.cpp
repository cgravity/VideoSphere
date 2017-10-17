#include "decoder.h"
#include <iostream>
using namespace std;

static void* decoder_thread_main(void* arg)
{
    Decoder* decoder = (Decoder*)arg;
    decoder->loop();    
    
    return NULL;
}

#ifndef NO_AUDIO
/*
void load_audio_mono_s16(Audio* audio, AVFrame* frame)
{
    // FIXME
    for(int i = 0; i < frame->nb_samples; i++)
    {
        int16_t sample = frame->data[0][i];
        
        audio->samples.push_back(sample);
        audio->samples.push_back(sample);
    }
}

void load_audio_stereo_s16_packed(Audio* audio, AVFrame* frame)
{
    // FIXME
    for(int i = 0; i < 2*frame->nb_samples; i++)
    {
        int16_t sample = frame->data[0][i];
        audio->samples.push_back(sample);
    }
}
*/

void load_audio_stereo_fltp(Audio* audio, AVFrame* frame)
{    
    float* a = (float*)frame->data[0];
    float* b = (float*)frame->data[1];
    
    static float theta = 0;
    for(int i = 0; i < frame->nb_samples; i++)
    {
        audio->samples.push_back(*a); a++;
        audio->samples.push_back(*b); b++;
    }
}

#endif // NO_AUDIO

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
    audio_stream_index = -1;
    
    for(int i = 0;i < format_context->nb_streams; i++)
    {
        if(format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
            && video_stream_index == -1)
        {
            video_stream_index = i;
        }
        
        if(format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
            && audio_stream_index == -1)
        {
            audio_stream_index = i;
        }
    }
    
    if(video_stream_index == -1)
    {
        cerr << "Failed to find video stream\n";
        return false;
    }
    
    #ifndef NO_AUDIO
    if(audio_stream_index == -1 && audio->setup_state != AuSS_NO_AUDIO)
    {
        cerr << "Warning: Failed to find audio stream\n";
        audio->setup_state = AuSS_NO_AUDIO;
    }
    #endif
    
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
    
    codec_context->thread_count = 8;
    
    // FIXME: Should this be 'refcounted_frames'???
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "recounted_frames", "1", 0);
    
    if(avcodec_open2(codec_context, codec, &opts) < 0)
    {
        cerr << "Couldn't open codec\n";
        return false;
    }
    
    time_base = format_context->streams[video_stream_index]->time_base;
    duration = format_context->streams[video_stream_index]->duration;
    number_of_frames = format_context->streams[video_stream_index]->nb_frames;
    
    
    #ifndef NO_AUDIO
    if(audio_stream_index != -1 && audio->setup_state != AuSS_NO_AUDIO)
    {
        audio_codec = avcodec_find_decoder(
            format_context->streams[audio_stream_index]->codecpar->codec_id);
        
        if(audio_codec == NULL)
        {
            cerr << "Unsupported audio codec!\n";
            audio->setup_state = AuSS_NO_AUDIO;
            goto end_audio_setup;
        }
        
        audio_codec_context = avcodec_alloc_context3(audio_codec);
        
        if(!audio_codec_context)
        {
            cerr << "Failed to allocate audio codec context!\n";
            audio->setup_state = AuSS_NO_AUDIO;
            goto end_audio_setup;
        }
    
        avcodec_parameters_to_context(audio_codec_context,
            format_context->streams[audio_stream_index]->codecpar);
            
        // FIXME: Should this be 'refcounted_frames'???
        AVDictionary* audio_opts = NULL;        
        av_dict_set(&audio_opts, "recounted_frames", "1", 0);
        
        if(avcodec_open2(audio_codec_context, audio_codec, &audio_opts) < 0)
        {
            cerr << "Couldn't open audio codec\n";
            audio->setup_state = AuSS_NO_AUDIO;
            goto end_audio_setup;
        }
        
        // decode all audio and stick it in the audio buffer
        // usually this will be a couple hundred megs of data -- no problem
        // on our systems in 2017 -- and greatly simplifies other logic
        
        if(audio->setup_state == AuSS_START_DECODING)
        {
            cerr << "Decoding audio...\n";
            
            audio->setup_state = AuSS_DECODING;
    
            int frame_finished = 0;
            AVPacket packet;
            AVFrame* frame = av_frame_alloc();
            
            packet.data = NULL;
            packet.size = 0;
            
            bool got_audio_details = false;
            void (*audio_loader)(Audio*,AVFrame*);
            
            int status = 0;
            
            // helpful format details in post by user "cornstalks" here:
            // https://www.gamedev.net/forums/topic/624876-how-to-read-an-audio-file-with-ffmpeg-in-c/?PageSpeed=noscript
            while(av_read_frame(format_context, &packet) >= 0)
            {
                if(packet.stream_index == audio_stream_index)
                {
                    status = avcodec_decode_audio4(audio_codec_context, frame, 
                        &frame_finished, &packet);
                    
                    if(frame_finished != 0)
                    {
                        // first time through, check info on sample rate,
                        // number of channels, etc
                        if(!got_audio_details)
                        {
                            got_audio_details = true;
                            audio->sample_rate = frame->sample_rate;
                            
                            /*if(frame->channels == 1 && 
                                audio_codec_context->sample_fmt ==
                                    AV_SAMPLE_FMT_S16)
                            {
                                audio_loader = load_audio_mono_s16;
                            }
                            else if(frame->channels == 2 &&
                                audio_codec_context->sample_fmt == 
                                    AV_SAMPLE_FMT_S16 &&
                                !av_sample_fmt_is_planar(
                                    audio_codec_context->sample_fmt))
                            {
                                audio_loader = load_audio_stereo_s16_packed;
                            }
                            else*/ if(frame->channels = 2 &&
                                audio_codec_context->sample_fmt ==
                                    AV_SAMPLE_FMT_FLTP)
                            {
                                audio_loader = load_audio_stereo_fltp;
                            }
                            else
                            {
                                cerr << "ERROR: Unsupported audio format!\n";
                                cerr << "Channels: " << frame->channels << '\n';
                                cerr << "FMT: " << av_get_sample_fmt_name(audio_codec_context->sample_fmt) << '\n';
                                cerr << "Planar: " << av_sample_fmt_is_planar(audio_codec_context->sample_fmt) << '\n';
                                break;
                            }
                        }
                        
                        // copy audio data into buffer based on format details
                        audio_loader(audio, frame);
                        av_frame_unref(frame);
                    }

                    char buf[256];

                    if(status < 0)
                    {
                        av_strerror(status, buf, sizeof(buf));
                        cerr << "ERROR " << buf << '\n';
                    }


                    av_free_packet(&packet);
                } // if packet.stream_index == audio_stream_index
                else
                {
                    av_free_packet(&packet);
                }
            } // while av_read_frame
            
            cerr << "Finished decoding audio\n";
            
            // indicate that we successfully loaded audio so that the
            // audio callback can be initialized
            audio->setup_state = AuSS_READY_TO_PLAY;
            seek(0);
        }
    }
  end_audio_setup:
    #endif
    
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
    
    DecoderFrame show_frame;
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
        if(seek_flag)
        {
            seek_flag = false;        
            
            av_seek_frame(format_context, video_stream_index, 
                seek_to, AVSEEK_FLAG_BACKWARD);
            
            avcodec_flush_buffers(codec_context);
            
            while(showable_frames.size())
            {
                fillable_frames.push_back(showable_frames.front().frame);
                showable_frames.pop_front();
            }
            
//            while(av_read_frame(format_context, &packet) >= 0)
//            {
//                if(packet.stream_index == video_stream_index)
//                {
//                    int64_t pts = packet.pts;                    
//                    av_free_packet(&packet);
//                    
//                    if(packet.pts >= seek_to)
//                    {
//                        break;
//                    }                    
//                }
//                else
//                {
//                    av_free_packet(&packet);
//                }
//            }
//            
            // next frame decoded may have weird timestamp because of seeking
            // so, playback thread should adjust time when it sees this frame!
            show_frame.seek_result = true;
        }
        
        unlock();   // don't need to hold lock while spending time decoding
        
        
        while(av_read_frame(format_context, &packet) >= 0)
        {
            if(packet.stream_index == video_stream_index)
            {
                avcodec_decode_video2(codec_context, yuv_frame, 
                    &frame_finished, &packet);
                
                av_free_packet(&packet);
                
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
                    
                    av_frame_unref(yuv_frame);
                    goto finished_frame;
                }
            } // if packet.stream_index == video_stream_index
            else
            {
                av_free_packet(&packet);
            }
        } // while av_read_frame
        
        if(looping)
        {
            seek(0);
            goto continue_point;
        }
        
        lock();
        decoded_all_flag = true; // reached end of data
        unlock();
        return;
        
finished_frame:
        lock(); // finished decoding a frame, so store it...
        show_frame.frame = rgb_frame;
        showable_frames.push_back(show_frame);

        // clear for next result
        show_frame.seek_result = false;
        show_frame.frame = NULL;
        
        goto continue_point;
    }
    
} // void Decoder::loop()





