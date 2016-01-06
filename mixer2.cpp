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

/*
|-----------|-----------|--> Time [frames]
0           1           2
Process 0   Process 1   Process 2
            Display 0   Display 1

The game runs at a fixed frame rate, and needs to
provide a buffer of audio samples each frame, which
matches the game events that occurred.

At t=0 we update the game state and generate the
frame that will be drawn in the time period from
frame 1 to frame 2. During frame 0 we must fill
the mixing buffer with 44100/60 = 735 samples per
channel, that would ideally be played back together
with the displayed image for frame 0.

Unfortunately, we cannot ensure that these samples
will be played back exactly together with the
displayed image, because displays are poop.

SDL will consume the mixing buffer in buffers of
a prespecified size. I've set this size equal to
the number of samples we will provide each frame.
SDL automatically computes the necessary fillsize

    bytes_to_fill = frame_size x
                    num_channels x
                    bytes_per_sample

(Default: 4096 samples, or 16384
bytes for two-channel 16-bit audio).

    MIXING BUFFER

The mixing buffer is a ringbuffer that is written
to each frame by the game, and read from once in a
while by the SDL audio callback.
*/

/* Desired API
void audio_PlaySource(Source *src,
                      audio_Flags flags = audio_NoFlags)
{
    Assert(audio.count <= AUDIO_MAX_SOURCES);
    if (audio.count == AUDIO_MAX_SOURCES)
    {
        return;
    }
    if (flags & audio_Restart)
    {
        src->position = src->chunk;
        src->remaining = src->length;
    }
    if (flags & audio_Repeat)
    {
        src->repeat = 1;
    }
    src->playing = 1;
    audio.playing[audio.count] = src;
    audio.count++;
}

void audio_StopSource(Source *src,
                      audio_Flags flags = audio_NoFlags)
{
    src->playing = 0;
    audio.count--;
}

void audio_SetMasterGain(r32 gain)
{
    audio.master_gain = gain;
}

void audio_SetMasterPan(r32 pan)
{
    audio.master_pan = pan;
}

void audio_Toggle()
{
    if (audio.master_playing)
        audio.master_playing = 0;
    else
        audio.master_playing = 1;
}

Source bgm1;
Source sfx1;
Source sfx2;

void init()
{
    bgm1 = audio_LoadSource("...")
    printf("bgm1\n");
    printf("duration: %.2f seconds\n");

    sfx1 = audio_LoadSource("...")
    printf("sfx1\n");
    printf("duration: %.2f seconds\n",
           sfx1.samples_per_channel / audio_SamplesPerSecond);

    sfx2 = audio_LoadSource("...")
    printf("sfx2\n");
    printf("duration: %.2f seconds\n");

    bgm1.gain = 0.2f;
    bgm1.pan = 0.0f;
    audio_PlaySource(&bgm1, audio_Repeat);
}

void tick()
{
    audio_SetMasterGain(0.8f);
    audio_SetMasterPan(-0.3f);
    if (KEY_RELEASED(1))
    {
        sfx1.pan = -1.0f;
        audio_PlaySource(&sfx1, audio_Restart);
    }
    if (KEY_RELEASED(2))
    {
        sfx2.pan = +1.0f;
        audio_PlaySource(&sfx2, audio_Restart);
    }
    if (KEY_RELEASED(SPACE))
    {
        if (bgm1.playing)
            audio_StopSource(&bgm1);
        else
            audio_PlaySource(&bgm1); // resumes by default
    }

    if (KEY_RELEASED(ESCAPE))
    {
        audio_Toggle();
    }
}
*/
#define Game_Frame_Rate (60)
#define Source_Samples_Per_Frame (Source_Sample_Rate / (r32)Game_Frame_Rate)

#define Source_Sample_Rate 44100
#define Source_Audio_Fmt AUDIO_S16
#define Source_Bytes_Per_Sample (SDL_AUDIO_BITSIZE(Source_Audio_Fmt)/8)
#define Source_Channels 2
#define Source_Frame_Size 735
#define Source_BufLenInSamples(x) (x / (Source_Bytes_Per_Sample))
#define Source_BufLenInSamplesPerChannel(x) (x / (Source_Channels*Source_Bytes_Per_Sample))
#define Source_Value_Max ((1<<(SDL_AUDIO_BITSIZE(Source_Audio_Fmt)-1)) - 1)

struct Source
{
    s16 *buffer; // Pointer to start of audio
    s32 length; // Length of audio data in bytes
    u32 channels; // Number of interleaved channels
    u32 bytes_per_sample;

    u32 samples_per_channel;
    u32 samples_in_total; // channels x samples_per_channel

    s32 position; // Position in buffer of next sample to play
    s32 remaining; // Number of samples remaining to be played

    // Reserved for audio api state
    bool playing;
    bool repeat;
    r32 gain_l;
    r32 gain_r;
    u32 audio_index;
};

void free_source(Source *source)
{
    Assert(false); // not yet implemented
    free(source->buffer);
    // etc...
}

Source load_source(char *filename)
{
    SDL_AudioSpec spec;
    u08 *buffer;
    u32 length;

    if (!SDL_LoadWAV(filename, &spec, &buffer, &length))
    {
        Printf("Failed to load WAV\n");
        Assert(false);
    }

    Assert(spec.freq == Source_Sample_Rate);
    Assert(SDL_AUDIO_BITSIZE(spec.format) / 8 == Source_Bytes_Per_Sample);
    Assert(spec.channels == Source_Channels);

    Source result = {};
    result.buffer = (s16*)buffer;
    result.length = length;
    result.channels = Source_Channels;
    result.bytes_per_sample = Source_Bytes_Per_Sample;
    result.samples_in_total =
        Source_BufLenInSamples(result.length);
    result.samples_per_channel =
        Source_BufLenInSamplesPerChannel(result.length);
    result.position = 0;
    result.remaining = result.samples_in_total;

    r32 duration = result.length /
        (r32)(result.channels*spec.freq*result.bytes_per_sample);

    Printf("Loaded %s\n", filename);
    Printf("Frequency: %d\n", spec.freq);
    Printf("Channels: %d\n", spec.channels);
    Printf("Buffer: %d bytes per unit\n", spec.samples);
    Printf("Bits/Sample: %d\n", SDL_AUDIO_BITSIZE(spec.format));
    Printf("Signed: %d\n", SDL_AUDIO_ISSIGNED(spec.format));
    Printf("LEndian: %d\n", SDL_AUDIO_ISLITTLEENDIAN(spec.format));
    Printf("Float: %d\n", SDL_AUDIO_ISFLOAT(spec.format));
    Printf("Format: 0x%x\n", spec.format);
    Printf("Total size: %d bytes\n", length);
    Printf("Duration: = %.2f s\n", duration);

    result.playing = 0;
    result.repeat = 0;
    result.gain_l = 1.0f;
    result.gain_r = 1.0f;
    result.audio_index = 0;

    return result;
}

// The input data must
//  - be sampled at Source_Sample_Rate
//  - have Source_Channels interleaved channel samples (LRLRLR...)
// The returned struct does not make a copy of the data, so the
// user must ensure that it is preserved and freed properly.
Source make_source(s16 *data, u32 total_num_samples)
{
    Source result = {};
    result.buffer = data;
    result.length = total_num_samples * sizeof(s16);
    result.channels = Source_Channels;
    result.bytes_per_sample = Source_Bytes_Per_Sample;
    result.samples_in_total =
        Source_BufLenInSamples(result.length);
    result.samples_per_channel =
        Source_BufLenInSamplesPerChannel(result.length);
    result.position = 0;
    result.remaining = result.samples_in_total;
    result.playing = 0;
    result.repeat = 0;
    result.gain_l = 1.0f;
    result.gain_r = 1.0f;
    result.audio_index = 0;
    return result;
}

s16 source_r32_to_s16(r32 x)
{
    s32 result = (s32)(Source_Value_Max*x);
    if (result < -Source_Value_Max) result = -Source_Value_Max;
    else if (result > Source_Value_Max) result = Source_Value_Max;
    return (s16)(result);
}

r32 source_s16_to_r32(s16 x)
{
    r32 result = (r32)(x) / (r32)Source_Value_Max;
    return result;
}

#define AUDIO_MAX_PLAYING 16
struct Audio
{
    Source *sources[AUDIO_MAX_PLAYING];
    int num_playing;
};

enum audio_Flags
{
    AUDIO_NOFLAG = 0, // Play once and stop
    AUDIO_REPEAT,     // Repeat forever
    AUDIO_RESTART     // Restart if not already playing
};

void audio_StartSource(Audio *audio,
                       Source *source,
                       audio_Flags flags = AUDIO_NOFLAG)
{
    SDL_LockAudio();
    if (!source->playing &&
        audio->num_playing < AUDIO_MAX_PLAYING)
    {
        // find first available slot
        for (int i = 0; i < AUDIO_MAX_PLAYING; i++)
        {
            if (!audio->sources[i])
            {
                if (flags & AUDIO_REPEAT)
                {
                    source->repeat = 1;
                }
                if (flags & AUDIO_RESTART)
                {
                    source->position = 0;
                    source->remaining = source->samples_in_total;
                }
                source->playing = 1;
                source->audio_index = i;
                audio->sources[i] = source;
                break;
            }
        }
        audio->num_playing++;
    }
    SDL_UnlockAudio();
}

void audio_StopSource(Audio *audio,
                      Source *source)
{
    // TODO: Paused sources should remain in the playlist
    // So that we can guaranteed unpause them.
    SDL_LockAudio();
    if (source->playing)
    {
        int index = source->audio_index;
        source->playing = 0;
        audio->sources[index] =0;
        audio->num_playing--;
    }
    SDL_UnlockAudio();
}

void audio_SetGain(Source *source, r32 gain_l, r32 gain_r)
{
    source->gain_l = gain_l;
    source->gain_r = gain_r;
}

// The callback must completely initialize the buffer; as of SDL 2.0, this
// buffer is not initialized before the callback is called. If there is
// nothing to play, the callback should fill the buffer with silence.
void audio_callback(void *userdata, u08 *stream, s32 bytes_to_fill)
{
    Assert(userdata != 0);

    // The number of samples had better be an even multiple of the
    // number of channels!
    Assert(bytes_to_fill % (Source_Channels*Source_Bytes_Per_Sample) == 0);
    s32 samples_to_fill = bytes_to_fill / Source_Bytes_Per_Sample;

    #define MIX_BUFFER_SAMPLES (1024*Source_Channels)
    Assert(MIX_BUFFER_SAMPLES >= samples_to_fill);

    Audio *audio = (Audio*)userdata;

    // mix sources
    static r32 mix_buffer[MIX_BUFFER_SAMPLES];
    SDL_memset(mix_buffer, 0, sizeof(mix_buffer));
    for (int source_index = 0;
         source_index < AUDIO_MAX_PLAYING;
         source_index++)
    {
        Source *source = audio->sources[source_index];
        if (!source)
            continue;

        r32 gain_l = source->gain_l;
        r32 gain_r = source->gain_r;

        for (int sample_index = 0;
             sample_index < samples_to_fill;
             sample_index += 2)
        {
            if (source->remaining > 0)
            {
                s16 xs16_l, xs16_r;
                r32 xr32_l, xr32_r;

                xs16_l = source->buffer[source->position];
                xs16_r = source->buffer[source->position+1];

                xr32_l = source_s16_to_r32(xs16_l);
                xr32_r = source_s16_to_r32(xs16_r);

                mix_buffer[sample_index] += gain_l * xr32_l;
                mix_buffer[sample_index+1] += gain_r * xr32_r;

                source->position += 2;
                source->remaining -= 2;
            }
            else if (source->repeat)
            {
                source->position = 0;
                source->remaining = source->samples_in_total;
            }
            else
            {
                source->playing = 0;
                audio->sources[source_index] = 0;
                audio->num_playing--;
                break;
            }
        }
    }

    // write result to output stream
    s16 *out = (s16*)stream;
    for (s32 s = 0; s < samples_to_fill; s++)
    {
        r32 xr = mix_buffer[s];
        s16 xs = source_r32_to_s16(xr);
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

    Source bgm2 = load_source("../bgm2.wav");
    Source sfx1 = load_source("../fx1.wav");

    Audio mixer = {};
    mixer.num_playing = 0;
    for (u32 s = 0; s < AUDIO_MAX_PLAYING; s++)
        mixer.sources[s] = 0;
    audio_SetGain(&sfx1, 1.0f, 1.0f);
    audio_SetGain(&bgm2, 1.0f, 1.0f);
    audio_StartSource(&mixer, &sfx1, AUDIO_REPEAT);
    audio_StartSource(&mixer, &bgm2);

    SDL_AudioSpec audio;
    audio.freq = Source_Sample_Rate;
    audio.format = Source_Audio_Fmt;
    audio.channels = Source_Channels;
    audio.samples = Source_Frame_Size;
    audio.callback = audio_callback;
    audio.userdata = &mixer;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
        Assert(false);
    }

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
                r32 gain_l = 0.5f + 0.5f * sin(t);
                r32 gain_r = 0.5f + 0.5f * cos(t);
                audio_SetGain(&bgm2, gain_l, gain_r);
                audio_SetGain(&sfx1, gain_r, gain_l);

                // artificial delay, to save battery
                SDL_Delay(14);

                if (time_since(start_tick) > 4.0f)
                {
                    audio_StartSource(&mixer, &bgm2, AUDIO_RESTART);
                }
                else if (time_since(start_tick) > 2.0f)
                {
                    audio_StopSource(&mixer, &bgm2);
                }
            }

            Printf("update %.2f %d\n",
                   1000.0f * time_since(last_update),
                   bgm2.remaining);
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
