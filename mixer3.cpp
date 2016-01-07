
#if 0

bgm_src = audio_load("../bgm.wav")
sfx_src = audio_load("../sfx.wav")
bgm = audio_stream(bgm_src, AUDIO_PAUSED)

sfx_streams[16] = {
    audio_stream(sfx_src, AUDIO_PAUSED),
    audio_stream(sfx_src, AUDIO_PAUSED),
    ...
    audio_stream(sfx_src, AUDIO_PAUSED)
};
audio_master(0.2f, 0.2f);

r32 audio_seconds(int samples)
{
    return samples / (r32)Audio_Sample_Rate;
}

int audio_position(audio_Stream stream)
{

}

game_update()
{
    printf("bgm: %.2f/%.2f seconds\n",
        audio_seconds(audio_position(bgm)),
        audio_seconds(audio_duration(bgm)));
    if (KEY_RELEASED(1))
    {
        audio_gain(bgm, 1.0f, 0.5f);
        audio_play(bgm, AUDIO_RESTART)
    }
    if (KEY_RELEASED(SPACE))
    {
        audio_stop(bgm);
    }
    if (KEY_RELEASED(ESCAPE))
    {
        if (audio_is_paused())
            audio_pause();
        else
            audio_resume();
        audio_master_stop();
    }
}
#endif

#define SDL_ASSERT_LEVEL 2
#define Assert SDL_assert
#define Printf SDL_Log
#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_assert.h"
#include <stdint.h>
typedef float       r32;
typedef uint64_t    u64;
typedef uint32_t    u32;
typedef uint16_t    u16;
typedef uint8_t     u08;
typedef int32_t     s32;
typedef int16_t     s16;
typedef int8_t      s08;

#define ArrayCount(x) (sizeof(x)/sizeof(x[0]))

#include "lib/stb_vorbis.c"

#define Game_Frame_Rate (60)
#define Audio_Samples_Per_Frame (Audio_Sample_Rate / (r32)Game_Frame_Rate)

#define Audio_Sample_Rate 44100
#define Audio_Format AUDIO_S16
#define Audio_Bytes_Per_Sample (SDL_AUDIO_BITSIZE(Audio_Format)/8)
#define Audio_Channels 2
#define Audio_Frame_Size 1024
#define Audio_BufLenInSamples(x) (x / (Audio_Bytes_Per_Sample))
#define Audio_BufLenInSamplesPerChannel(x) (x / (Audio_Channels*Audio_Bytes_Per_Sample))
#define Audio_BufLenInSeconds(x) (x / (r32)(Audio_Sample_Rate*Audio_Bytes_Per_Sample*Audio_Channels))
#define Audio_SamplesInSeconds(x) (x / (r32)(Audio_Sample_Rate*Audio_Channels))
#define Audio_Value_Max ((1<<(SDL_AUDIO_BITSIZE(Audio_Format)-1)) - 1)

struct audio_Source
{
    s16 *buffer; // Pointer to original interleaved audio data
                 // allocated when the source was loaded or made.
    int length;  // Number of interleaved samples in buffer
};

struct audio_Stream
{
    audio_Source source;
    int position; // Position in source buffer in samples
    int remaining; // Number of samples remaining to be played
    bool active;  // When true the stream is currently in use
    bool paused;
    bool repeat;
    r32 gain_l; // Left channel gain in range 0 to 1
    r32 gain_r; // Right channel gain in range 0 to 1
};

typedef int audio_id;

#define Audio_Max_Streams 16
struct Audio
{
    audio_Stream streams[Audio_Max_Streams];
    int num_streams;
    r32 gain_l;
    r32 gain_r;
} audio;

typedef int audio_id;
#define Audio_Invalid_Stream -1

// Returns a handle that can be used to refer
// to the new stream in subsequence calls.
// Returns -1 if the number of active streams
// is maxed out. The handle is valid until
// a call to audio_close with the given handle.
// The stream is originally paused, and must
// be started by a call to audio_play.
audio_id audio_stream(audio_Source source)
{
    SDL_LockAudio();
    audio_id result = Audio_Invalid_Stream;
    // find first available stream
    for (int id = 0; id < Audio_Max_Streams; id++)
    {
        if (!audio.streams[id].active)
        {
            result = id;
            audio.streams[id].source = source;
            audio.streams[id].position = 0;
            audio.streams[id].paused = 1;
            audio.streams[id].active = 1;
            audio.streams[id].repeat = 0;
            audio.streams[id].gain_l = 1.0f;
            audio.streams[id].gain_r = 1.0f;
            audio.streams[id].remaining = source.length;
            audio.num_streams++;
            break;
        }
    }
    SDL_UnlockAudio();
    return result;
}

void audio_close(audio_id id)
{
    SDL_LockAudio();
    if (id >= 0 && audio.streams[id].active)
    {
        audio.streams[id].active = 0;
        audio.num_streams--;
    }
    SDL_UnlockAudio();
}

enum audio_Flags
{
    Audio_NoFlag = 0,
    Audio_Restart,
    Audio_Repeat
};

void audio_play(audio_id id, audio_Flags flags = Audio_NoFlag)
{
    SDL_LockAudio();
    if (id >= 0 && audio.streams[id].active)
    {
        if (flags & Audio_Restart)
        {
            audio.streams[id].position = 0;
        }
        if (flags & Audio_Repeat)
        {
            audio.streams[id].repeat = 1;
        }
        audio.streams[id].paused = 0;
    }
    SDL_UnlockAudio();
}

void audio_stop(audio_id id)
{
    SDL_LockAudio();
    if (id >= 0 && audio.streams[id].active)
    {
        audio.streams[id].paused = 1;
    }
    SDL_UnlockAudio();
}

// Returns the position along the stream for
// one channel, in samples.
int audio_time(audio_id id)
{
    SDL_LockAudio();
    int result = 0;
    if (id >= 0 && audio.streams[id].active)
    {
        result = audio.streams[id].position / Audio_Channels;
    }
    SDL_UnlockAudio();
    return result;
}

// Converts the result from a call to audio_time
// to seconds.
r32 audio_time_in_seconds(int samples_per_channel)
{
    return samples_per_channel / (r32)(Audio_Sample_Rate);
}

void audio_master_gain(r32 left, r32 right)
{
    audio.gain_l = left;
    audio.gain_r = right;
}

void audio_gain(audio_id id, r32 left, r32 right)
{
    SDL_LockAudio();
    if (id >= 0 && audio.streams[id].active)
    {
        audio.streams[id].gain_l = left;
        audio.streams[id].gain_r = right;
    }
    SDL_UnlockAudio();
}

audio_Source audio_load(char *filename)
{
    SDL_AudioSpec spec;
    u08 *buffer;
    u32 length_in_bytes;

    if (!SDL_LoadWAV(filename, &spec, &buffer, &length_in_bytes))
    {
        Printf("Failed to load WAV\n");
        Assert(false);
    }

    Assert(spec.freq == Audio_Sample_Rate);
    Assert(SDL_AUDIO_BITSIZE(spec.format) / 8 == Audio_Bytes_Per_Sample);
    Assert(spec.channels == Audio_Channels);

    audio_Source result = {};
    result.buffer = (s16*)buffer;
    result.length = Audio_BufLenInSamples(length_in_bytes);
    r32 duration = Audio_BufLenInSeconds(length_in_bytes);

    Printf("Loaded %s\n", filename);
    Printf("Frequency: %d\n", spec.freq);
    Printf("Channels: %d\n", spec.channels);
    Printf("Buffer: %d bytes per unit\n", spec.samples);
    Printf("Bits/Sample: %d\n", SDL_AUDIO_BITSIZE(spec.format));
    Printf("Signed: %d\n", SDL_AUDIO_ISSIGNED(spec.format));
    Printf("LEndian: %d\n", SDL_AUDIO_ISLITTLEENDIAN(spec.format));
    Printf("Float: %d\n", SDL_AUDIO_ISFLOAT(spec.format));
    Printf("Format: 0x%x\n", spec.format);
    Printf("Total size: %d bytes\n", length_in_bytes);
    Printf("Duration: = %.2f s\n", duration);

    return result;
}

// The input data must
//  - be sampled at Audio_Sample_Rate
//  - have Audio_Channels interleaved channel samples (LRLRLR...)
// The returned struct does not make a copy of the data, so the
// user must ensure that it is preserved and freed properly.
audio_Source make_source(s16 *data, u32 total_num_samples)
{
    audio_Source result = {};
    result.buffer = data;
    result.length = total_num_samples;
    return result;
}

s16 audio_r32_to_s16(r32 x)
{
    s32 result = (s32)(Audio_Value_Max*x);
    if (result < -Audio_Value_Max) result = -Audio_Value_Max;
    else if (result > Audio_Value_Max) result = Audio_Value_Max;
    return (s16)(result);
}

r32 audio_s16_to_r32(s16 x)
{
    r32 result = (r32)(x) / (r32)Audio_Value_Max;
    return result;
}

// The callback must completely initialize the buffer; as of SDL 2.0, this
// buffer is not initialized before the callback is called. If there is
// nothing to play, the callback should fill the buffer with silence.
void audio_callback(void *userdata,
                    u08 *sdl_buffer,
                    s32 bytes_to_fill)
{
    // The number of samples had better be an even multiple of the
    // number of channels!
    Assert(bytes_to_fill % (Audio_Channels*Audio_Bytes_Per_Sample) == 0);
    s32 samples_to_fill = bytes_to_fill / Audio_Bytes_Per_Sample;

    #define MIX_BUFFER_SAMPLES (2048*Audio_Channels)
    Assert(MIX_BUFFER_SAMPLES >= samples_to_fill);

    // mix sources
    static r32 mix_buffer[MIX_BUFFER_SAMPLES];
    SDL_memset(mix_buffer, 0, sizeof(mix_buffer));
    for (int stream_index = 0;
         stream_index < Audio_Max_Streams;
         stream_index++)
    {
        audio_Stream *stream = audio.streams + stream_index;
        if (!stream->active)
            continue;
        if (stream->paused)
            continue;

        audio_Source source = stream->source;

        r32 gain_l = audio.gain_l * stream->gain_l;
        r32 gain_r = audio.gain_r * stream->gain_r;

        for (int sample_index = 0;
             sample_index < samples_to_fill;
             sample_index += 2)
        {
            if (stream->remaining > 0)
            {
                s16 xs16_l, xs16_r;
                r32 xr32_l, xr32_r;

                xs16_l = source.buffer[stream->position];
                xs16_r = source.buffer[stream->position+1];

                xr32_l = audio_s16_to_r32(xs16_l);
                xr32_r = audio_s16_to_r32(xs16_r);

                mix_buffer[sample_index] += gain_l * xr32_l;
                mix_buffer[sample_index+1] += gain_r * xr32_r;

                stream->position += 2;
                stream->remaining -= 2;
            }
            else if (stream->repeat)
            {
                stream->position = 0;
                stream->remaining = source.length;
            }
            else
            {
                stream->paused = 1;
                break;
            }
        }
    }

    // write result to output stream
    s16 *out = (s16*)sdl_buffer;
    for (s32 s = 0; s < samples_to_fill; s++)
    {
        r32 xr = mix_buffer[s];
        s16 xs = audio_r32_to_s16(xr);
        out[s] = xs;
    }
}

u64 get_tick()
{
    return SDL_GetPerformanceCounter();
}

float get_elapsed_time(u64 begin, u64 end)
{
    u64 frequency = SDL_GetPerformanceFrequency();
    return (float)(end - begin) / (float)frequency;
}

float time_since(u64 then)
{
    u64 now = get_tick();
    return get_elapsed_time(then, now);
}

#include <stdio.h>
#include <math.h>
int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        Printf("Failed to initialize SDL: %s\n", SDL_GetError());
        Assert(false);
    }

    audio_Source bgm2_src = audio_load("../bgm2.wav");
    audio_Source sfx1_src = audio_load("../fx1.wav");

    audio.num_streams = 0;

    SDL_AudioSpec audio;
    audio.freq = Audio_Sample_Rate;
    audio.format = Audio_Format;
    audio.channels = Audio_Channels;
    audio.samples = Audio_Frame_Size;
    audio.callback = audio_callback;
    audio.userdata = 0;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
        Assert(false);
    }

    audio_id bgm1 = audio_stream(bgm2_src);
    audio_id bgm2 = audio_stream(bgm2_src);
    audio_id sfx1 = audio_stream(sfx1_src);
    audio_play(bgm1);
    audio_master_gain(1.0f, 1.0f);

    u64 start_tick = get_tick();
    r32 frame_time = 1.0f / 60.0f;
    r32 tick_timer = 0.0f;
    u64 frame_tick = start_tick;
    u64 last_update = start_tick;

    SDL_PauseAudio(0);
    bool running = 1;
    while (running)
    {
        if (tick_timer <= 0.0f)
        {
            // game update
            {
                r32 t = time_since(start_tick);
                r32 gain_l = 0.2f * (0.5f + 0.5f * sin(t));
                r32 gain_r = 0.2f * (0.5f + 0.5f * cos(t));
                audio_gain(bgm1, gain_l, gain_r);

                // artificial delay, to save battery
                SDL_Delay(14);

                if (time_since(start_tick) > 2.0f)
                {
                    audio_play(bgm2);
                    audio_gain(bgm2, gain_r, gain_l);
                }

                if (time_since(start_tick) > 4.0f)
                {
                    audio_stop(bgm1);
                    audio_play(sfx1, Audio_Repeat);
                }
            }

            Printf("update %.2f %.2f\n",
                   1000.0f * time_since(last_update),
                   audio_time_in_seconds(audio_time(bgm2)));
            last_update = get_tick();
            tick_timer += frame_time;
        }
        u64 now = get_tick();
        tick_timer -= get_elapsed_time(frame_tick, now);
        frame_tick = now;
    }
    SDL_CloseAudio();

    SDL_Quit();
    return 0;
}
