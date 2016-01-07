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
            audio.streams[id].remaining = audio.streams[id].source.length;
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

struct GameInput
{
    struct Key
    {
        bool down[SDL_NUM_SCANCODES];
        bool released[SDL_NUM_SCANCODES];
    } key;
    int window_width;
    int window_height;
    r32 t;
    r32 dt;
};

#define KEY_RELEASED(code) input.key.released[SDL_SCANCODE_##code]
#define KEY_DOWN(code) input.key.down[SDL_SCANCODE_##code]
void game_update(GameInput input)
{
    static bool loaded = 0;
    static audio_Source sfx1_src;
    static audio_Source sfx2_src;
    static audio_Source sfx3_src;
    static audio_Source sfx4_src;
    static audio_Source sfx5_src;
    static audio_Source sfx6_src;
    static audio_Source bgm1_src;
    static audio_Source bgm2_src;

    static audio_id sfx1;
    static audio_id sfx2;
    static audio_id sfx3;
    static audio_id sfx4;
    static audio_id sfx5;
    static audio_id sfx6;
    static audio_id bgm1;
    static audio_id bgm2;
    if (!loaded)
    {
        bgm1_src = audio_load("../bgm1.wav");
        bgm2_src = audio_load("../bgm2.wav");
        sfx1_src = audio_load("../fx1.wav");
        sfx2_src = audio_load("../fx2.wav");
        sfx3_src = audio_load("../fx3.wav");
        sfx4_src = audio_load("../fx4.wav");
        sfx5_src = audio_load("../fx5.wav");
        sfx6_src = audio_load("../fx6.wav");
        sfx1 = audio_stream(sfx1_src);
        sfx2 = audio_stream(sfx2_src);
        sfx3 = audio_stream(sfx3_src);
        sfx4 = audio_stream(sfx4_src);
        sfx5 = audio_stream(sfx5_src);
        sfx6 = audio_stream(sfx6_src);
        bgm1 = audio_stream(bgm2_src);
        bgm2 = audio_stream(bgm2_src);
        audio_master_gain(0.01f, 0.01f);
        loaded = 1;
    }

    #define PLAY_ON_KEY(key, sound) { if (KEY_RELEASED(key)) { audio_play(sound, Audio_Restart); audio_gain(sound, 1.0f, 1.0f); } }

    r32 t = input.t;
    PLAY_ON_KEY(1, sfx1);
    PLAY_ON_KEY(2, sfx2);
    PLAY_ON_KEY(3, sfx3);
    PLAY_ON_KEY(4, sfx4);
    PLAY_ON_KEY(5, sfx5);
    PLAY_ON_KEY(6, sfx6);

    if (KEY_RELEASED(SPACE))
    {
        audio_play(bgm2);
    }

    audio_gain(bgm2, 0.5f+0.5f*sin(t), 0.5f+0.5f*cos(t));

    glViewport(0, 0, input.window_width, input.window_height);
    r32 r = 0.2f * sin(0.3f * t + 0.11f) + 0.6f;
    r32 g = 0.1f * sin(0.4f * t + 0.55f) + 0.3f;
    r32 b = 0.3f * sin(0.5f * t + 1.44f) + 0.3f;
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_LINES);
    {
        u32 n = 8;
        r32 pi = 3.1415926f;
        for (u32 i = 0; i < n; i++)
        {
            r32 a = i / (r32)(n-1);
            r32 w = a + 0.3f;
            r32 p = a * pi / 8.0f;
            r32 x0 = 0.5f*cos(w*t + p)*input.window_height/input.window_width;
            r32 y0 = 0.5f*sin(w*t + p);
            r32 x1 = -x0;
            r32 y1 = -y0;
            glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x0, y0 + 0.3f*sin(0.3f*t));
            glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x1, y1 + 0.3f*sin(0.3f*t));
        }

        u32 wn = 4;
        for (u32 wave = 0; wave < wn; wave++)
        {
            r32 p = wave / (r32)(wn);
            u32 ln = 64;
            for (u32 i = 0; i < ln; i++)
            {
                r32 a = i / (r32)(ln-1);
                r32 b = (i+1) / (r32)(ln-1);
                r32 x0 = -1.0f + 2.0f*a;
                r32 x1 = -1.0f + 2.0f*b;
                r32 y = -0.75f;
                r32 y0 = y + (0.03f+0.03f*p)*sin(0.3f*t + (a+4.0f*p)*1.2f*pi)+0.02f*cos((2.0f+p)*t+a);
                r32 y1 = y + (0.03f+0.03f*p)*sin(0.3f*t + (b+4.0f*p)*1.2f*pi)+0.02f*cos((2.0f+p)*t+b);
                glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x0, y0);
                glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(x1, y1);
            }
        }
    }
    glEnd();
}

#include <stdio.h>
#include <math.h>
int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        Printf("Failed to initialize SDL: %s\n", SDL_GetError());
        Assert(false);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    int window_width = 640;
    int window_height = 480;
    SDL_Window *window = SDL_CreateWindow(
        "Hello world!",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        window_width, window_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        Printf("Failed to create a window: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(0);

    // init audio
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

    SDL_PauseAudio(0);

    GameInput input = {};
    input.window_width = window_width;
    input.window_height = window_height;

    u64 start_tick = get_tick();
    r32 frame_time = 1.0f / 60.0f;
    r32 tick_timer = 0.0f;
    u64 frame_tick = start_tick;
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_WINDOWEVENT:
                {
                    switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                            Printf("Window %d size changed to %dx%d\n",
                                    event.window.windowID,
                                    event.window.data1,
                                    event.window.data2);
                            window_width = event.window.data1;
                            window_height = event.window.data2;
                            input.window_width = window_width;
                            input.window_height = window_height;
                        } break;
                    }
                } break;

                case SDL_KEYDOWN:
                {
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    input.key.down[event.key.keysym.scancode] = 1;
                } break;

                case SDL_KEYUP:
                {
                    if (input.key.down[event.key.keysym.scancode])
                    {
                        input.key.down[event.key.keysym.scancode] = 0;
                        input.key.released[event.key.keysym.scancode] = 1;
                    }
                } break;

                case SDL_QUIT:
                {
                    running = false;
                } break;
            }
        }

        if (tick_timer <= 0.0f)
        {
            r32 elapsed_time = time_since(start_tick);
            input.t = elapsed_time;
            input.dt = frame_time;
            game_update(input);
            tick_timer += frame_time;
            SDL_GL_SwapWindow(window);

            // We have now processed these events!
            for (int i = 0; i < SDL_NUM_SCANCODES; i++)
                input.key.released[i] = 0;

            SDL_Delay(10);
        }

        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            Printf("An OGL error occurred\n");
            running = false;
        }

        u64 now = get_tick();
        tick_timer -= get_elapsed_time(frame_tick, now);
        frame_tick = now;
    }

    SDL_CloseAudio();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
