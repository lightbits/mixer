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

#define Source_Sample_Rate 44100
#define Source_Audio_Fmt AUDIO_S16
#define Source_Bytes_Per_Sample (SDL_AUDIO_BITSIZE(Source_Audio_Fmt)/8)
#define Source_Channels 2
#define Source_Frame_Size 4096
#define Source_BufLenInSamples(x) (x / (Source_Bytes_Per_Sample))
#define Source_BufLenInSamplesPerChannel(x) (x / (Source_Channels*Source_Bytes_Per_Sample))
#define Source_Value_Max ((1<<(SDL_AUDIO_BITSIZE(Source_Audio_Fmt)-1)) - 1)
struct Source
{
    u08 *chunk;    // Pointer to start of audio
    s32 length;    // Length of audio data in bytes
    u32 channels;  // Number of interleaved channels
    u32 bytes_per_sample;

    u32 samples_per_channel;
    u32 samples_in_total;

    u08 *position; // Pointer to next data sample to be processed
    s32 remaining; // In bytes
};

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
    result.chunk = buffer;
    result.length = length;
    result.channels = Source_Channels;
    result.bytes_per_sample = Source_Bytes_Per_Sample;
    result.remaining = length;
    result.position = result.chunk;
    result.samples_in_total =
        Source_BufLenInSamples(result.length);
    result.samples_per_channel =
        Source_BufLenInSamplesPerChannel(result.length);

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

    return result;
}

// The input data must
//  * be sampled at Source_Sample_Rate
//  * have Source_Channels interleaved channel samples (LRLRLR...)
// The returned struct does not make a copy of the data, so the
// user must ensure that it is preserved and freed properly.
Source make_source(s16 *data, u32 total_num_samples)
{
    Source result = {};
    result.chunk = (u08*)data;
    result.length = total_num_samples * sizeof(s16);
    result.channels = Source_Channels;
    result.bytes_per_sample = Source_Bytes_Per_Sample;
    result.remaining = result.length;
    result.position = result.chunk;
    result.samples_in_total =
        Source_BufLenInSamples(result.length);
    result.samples_per_channel =
        Source_BufLenInSamplesPerChannel(result.length);
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

void audio_lowpass_mono(s16 *input,
                        s16 *result,
                        u32 samples)
{
    // working buffers
    r32 x[3];
    r32 y[3];

    r32 a1 = -1.9f;
    r32 a2 = 0.91f;
    r32 k = 0.00102f;
    r32 b1 = 2.0f;
    r32 b2 = 1.0f;
    y[0] = 0.0f;
    y[1] = 0.0f;
    x[0] = source_s16_to_r32(input[0]);
    x[1] = source_s16_to_r32(input[Source_Channels]);
    result[0] = input[0];
    result[Source_Channels] = input[Source_Channels];
    u32 n = 0;
    for (u32 i = 2; i < samples; i++)
    {
        x[2] = source_s16_to_r32(input[n]);
        y[2] = -a1*y[1] - a2*y[0] + k*(x[2] + b1*x[1] + b2*x[0]);
        x[0] = x[1];
        x[1] = x[2];
        y[0] = y[1];
        y[1] = y[2];
        result[n] = source_r32_to_s16(y[2]);

        n += Source_Channels;
    }
}

// input, result - 16-bit signed interleaved channels audio
// samples       - Number of samples per channel
void audio_lowpass(s16 *input, s16 *result, u32 samples)
{
    audio_lowpass_mono(input, result, samples);
    audio_lowpass_mono(input+1, result, samples);
}

#define DFT_Samples 64
#define DFT_Sample_To_Hz(k) (Source_Sample_Rate*k/(r32)(DFT_Samples*2))
// Note: The DFT is symmetric, so we only compute half of the spectrum
void audio_dft_mono(s16 *mono, r32 *real, r32 *complex, u32 samples)
{
    u32 K = DFT_Samples*2;
    for (u32 k = 0; k < DFT_Samples; k++)
    {
        real[k] = 0.0f;
        complex[k] = 0.0f;
        for (u32 n = 0; n < samples; n++)
        {
            real[k] += source_s16_to_r32(mono[2*n])*cos(TWO_PI*k*n/K);
            complex[k] -= source_s16_to_r32(mono[2*n])*sin(TWO_PI*k*n/K);
        }
        real[k] /= sqrt((r32)K);
        complex[k] /= sqrt((r32)K);
    }
}

// Compute the _samples_-DFT of stereo signal
// and store real and complex output in given
// buffers.
void audio_dft(s16 *stereo,
               r32 *real_left,
               r32 *real_right,
               r32 *complex_left,
               r32 *complex_right,
               u32 samples)
{
    audio_dft_mono(stereo, real_left, complex_left, samples);
    audio_dft_mono(stereo+1, real_right, complex_right, samples);
}

void audio_callback(void *userdata, u08 *stream, s32 bytes_to_fill)
{
    // The callback must completely initialize the buffer; as of SDL 2.0, this buffer is not initialized before the callback is called. If there is nothing to play, the callback should fill the buffer with silence.

    // The number of samples had better be an even multiple of the
    // number of channels!
    Assert(bytes_to_fill % (Source_Channels*Source_Bytes_Per_Sample) == 0);

    Assert(userdata != 0);
    Source *src = (Source*)userdata;

    if (src->remaining <= 0)
    {
        SDL_memset(stream, 0, bytes_to_fill);
    }
    else
    {
        if (bytes_to_fill > src->remaining)
            bytes_to_fill = src->remaining;

        #if 1
        SDL_memcpy(stream, src->position, bytes_to_fill);
        #else
        u32 samples = Source_BufLenInSamplesPerChannel(bytes_to_fill);
        audio_lowpass((s16*)src->position, (s16*)stream, samples);
        #endif
        src->position += bytes_to_fill;
        src->remaining -= bytes_to_fill;
    }
}

#include <stdio.h>
int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        Printf("Failed to initialize SDL: %s\n", SDL_GetError());
        Assert(false);
    }

    #if 0
    Source src1 = load_source("../bgm2.wav");
    s16 *filtered = (s16*)malloc(src1.length);
    audio_lowpass((s16*)src1.chunk, filtered, src1.samples_per_channel);
    Source src = make_source(filtered, src1.samples_in_total);
    #else

    s16 square_wave[Source_Sample_Rate*Source_Channels*2];
    // u32 duty = 1;
    // u32 duty_len = (u32)((r32)Source_Sample_Rate / (2.0f*220.0f));
    // s16 sign = Source_Value_Max;
    s16 *output = square_wave;
    for (u32 n = 0; n < ArrayCount(square_wave)/2; n++)
    {
        // duty++;
        // if (duty == duty_len)
        // {
        //     sign *= -1;
        //     duty = 1;
        //     if (duty_len > 20)
        //         duty_len--;
        // }
        // output[0] = sign;
        // output[1] = sign;
        // output += 2;

        r32 t = (r32)n / Source_Sample_Rate;
        output[0] = source_r32_to_s16(sin(TWO_PI*440.0f*t));
        output[1] = source_r32_to_s16(sin(TWO_PI*440.0f*t));
        output += 2;
    }

    s16 filtered_wave[ArrayCount(square_wave)];
    audio_lowpass(square_wave, filtered_wave, ArrayCount(square_wave)/2);

    Source src = make_source(square_wave, ArrayCount(square_wave));

    static r32 real_left[DFT_Samples];
    static r32 real_right[DFT_Samples];
    static r32 complex_left[DFT_Samples];
    static r32 complex_right[DFT_Samples];
    audio_dft(square_wave, real_left, real_right,
              complex_left, complex_right, ArrayCount(square_wave)/2);

    for (u32 k = 0; k < 16; k++)
    {
        r32 magnitude = sqrt(real_left[k]*real_left[k] +
                        complex_left[k]*complex_left[k]);
        Printf("%.2f Hz -> %.2f\n", DFT_Sample_To_Hz(k), magnitude);
    }

    #endif

    SDL_AudioSpec audio;
    audio.freq = Source_Sample_Rate;
    audio.format = Source_Audio_Fmt;
    audio.channels = Source_Channels;
    audio.samples = Source_Frame_Size;
    audio.callback = audio_callback;
    audio.userdata = &src;

    if (SDL_OpenAudio(&audio, 0) != 0)
    {
        Printf("Failed to open audio device: %s\n", SDL_GetError());
        Assert(false);
    }

    SDL_PauseAudio(0);
    while (src.remaining > 0)
    {
        printf("\r%d\t\t\t\t\t", src.remaining);
        SDL_Delay(5);
    }
    // printf("> ");
    // char input[256];
    // scanf("%s", &input);
    SDL_CloseAudio();

    SDL_Quit();
    return 0;
}
