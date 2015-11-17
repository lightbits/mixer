// Linux: gcc main.cpp -o app -lGL `sdl2-config --cflags --libs`

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

#include <math.h>
#define TWO_PI (6.28318530718f)

#include "lib/stb_vorbis.c"

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

struct Source
{
    u08 *chunk;    // Pointer to start of audio
    s32 length;    // Length of audio data in bytes
    s32 remaining;
    u08 *position;
};

void fill_audio(void *userdata, u08 *stream, s32 len)
{
    // The callback must completely initialize the buffer; as of SDL 2.0, this buffer is not initialized before the callback is called. If there is nothing to play, the callback should fill the buffer with silence.

    Assert(userdata != 0);
    Source *src = (Source*)userdata;

    if (src->remaining <= 0)
    {
        SDL_memset(stream, 0, len);
    }
    else
    {
        if (len > src->remaining)
            len = src->remaining;

        SDL_memcpy(stream, src->position, len);
        src->position += len;
        src->remaining -= len;
    }

}

#include <stdio.h>
int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        Printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return -1;
    }

    // Callback API, load ogg
    #if 0
    int vb_channels, vb_sample_rate;
    s16 *vb_buffer;
    int vb_result = stb_vorbis_decode_filename("../music2.ogg",
                    &vb_channels, &vb_sample_rate, &vb_buffer);
    if (vb_result == -1)
    {
        Printf("Could not load\n");
    }

    Printf("Frequency: %d\n", vb_sample_rate);
    Printf("Channels: %d\n", vb_channels);
    Printf("Samples: %d samples/channel\n", vb_result);
    Printf("Total size: %d bytes\n", vb_result*sizeof(s16)*vb_channels);
    Printf("Duration: = %.2f seconds\n", (vb_result) / (r32)(vb_sample_rate));

    Source src = {};
    src.chunk = (u08*)vb_buffer;
    src.length = vb_result*sizeof(s16)*vb_channels;
    src.position = src.chunk;
    src.remaining = src.length;

    SDL_AudioSpec audio;
    audio.freq = vb_sample_rate;
    audio.format = AUDIO_S16;
    audio.channels = vb_channels;
    audio.samples = 4096;
    audio.callback = fill_audio;
    audio.userdata = &src;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
    }

    SDL_PauseAudio(0);
    while (src.remaining > 0)
    {
        SDL_Delay(1000);
    }
    SDL_CloseAudio();

    #endif

    // Callback API, load WAV
    #if 1
    SDL_AudioSpec wav_spec;
    u08 *wav_buffer;
    u32 wav_len;
    char *filename = "../bgm2.wav";

    if (!SDL_LoadWAV(filename, &wav_spec, &wav_buffer, &wav_len))
    {
        Printf("Failed to load WAV\n");
        return -1;
    }

    Printf("Loaded %s\n", filename);
    Printf("Frequency: %d\n", wav_spec.freq);
    Printf("Channels: %d\n", wav_spec.channels);
    Printf("Buffer: %d bytes per unit\n", wav_spec.samples);
    Printf("Bits/Sample: %d\n", SDL_AUDIO_BITSIZE(wav_spec.format));
    Printf("Signed: %d\n", SDL_AUDIO_ISSIGNED(wav_spec.format));
    Printf("LEndian: %d\n", SDL_AUDIO_ISLITTLEENDIAN(wav_spec.format));
    Printf("Float: %d\n", SDL_AUDIO_ISFLOAT(wav_spec.format));
    Printf("Format: 0x%x\n", wav_spec.format);
    Printf("Total size: %d bytes\n", wav_len);

    int channels = wav_spec.channels;
    int frequency = wav_spec.freq;
    int num_bytes = SDL_AUDIO_BITSIZE(wav_spec.format) / 8;
    Printf("Duration: = %.2f s\n",
           (wav_len) / (r32)(channels*frequency*num_bytes));

    Source src = {};
    src.chunk = wav_buffer;
    src.length = wav_len;
    src.position = src.chunk;
    src.remaining = src.length;

    SDL_AudioSpec audio;
    audio.freq = wav_spec.freq;
    audio.format = wav_spec.format;
    audio.channels = wav_spec.channels;
    audio.samples = wav_spec.samples;
    audio.callback = fill_audio;
    audio.userdata = &src;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
    }

    SDL_PauseAudio(0);
    // while (src.remaining > 0)
    // {
    //     printf("\r%d\t\t\t\t\t", src.remaining);
    //     SDL_Delay(5);
    // }
    printf("> ");
    char input[256];
    scanf("%s", &input);
    SDL_CloseAudio();
    #endif

    // Audio synthesis
    #if 0
    #define DSP_Freq (22050)
    #define Num_Samples ((DSP_Freq)*2)
    s16 samples[Num_Samples];
    for (u32 i = 0; i < Num_Samples; i++)
    {
        r32 t = i / (r32)DSP_Freq;
        r32 f = 440.0f;
        r32 x = -1.0f + 2.0f * sin(f*TWO_PI*t);
        samples[i] = (s16)(0.05f*65535.0f*x);
    }
    Source src = {};
    src.chunk = (u08*)samples;
    src.length = sizeof(samples);
    src.position = src.chunk;
    src.remaining = src.length;

    SDL_AudioSpec audio;
    audio.format = AUDIO_S16;
    audio.channels = 1;
    audio.callback = fill_audio;
    audio.userdata = &src;
    audio.freq = 22050;
    audio.samples = 4096;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
    }

    SDL_PauseAudio(0);
    while (src.remaining > 0)
    {
        SDL_Delay(1000);
    }
    SDL_CloseAudio();
    #endif

    // Floating point synthesis, AudioDevice
    #if 0
    #define DSP_Freq (48000)
    #define Num_Samples ((DSP_Freq)*1)
    r32 samples[Num_Samples];
    for (u32 i = 0; i < Num_Samples; i++)
    {
        r32 t = i / (r32)DSP_Freq;
        r32 x = -1.0f + 2.0f * sin(440.0f*TWO_PI*t);
        r32 y = -1.0f + 2.0f * sin(554.0f*TWO_PI*t);
        r32 z = -1.0f + 2.0f * sin(659.0f*TWO_PI*t);
        samples[i] = 0.33f*x + 0.33f*y + 0.33f*z;
    }
    Source src = {};
    src.chunk = (u08*)samples;
    src.length = sizeof(samples);
    src.position = src.chunk;
    src.remaining = src.length;

    SDL_AudioSpec desired, obtained;
    desired.freq = DSP_Freq;
    desired.format = AUDIO_F32;
    desired.channels = 1;
    desired.samples = 4096;
    desired.callback = fill_audio;
    desired.userdata = &src;
    SDL_AudioDeviceID audio_device =
        SDL_OpenAudioDevice(0, 0, &desired, &obtained,
        SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (!audio_device)
    {
        Printf("Failed to open audio device\n");
        return -1;
    }

    SDL_PauseAudioDevice(audio_device, 0);
    while (src.remaining > 0)
    {
        SDL_Delay(1000);
    }
    SDL_CloseAudioDevice(audio_device);
    #endif

    SDL_Quit();
    return 0;
}
