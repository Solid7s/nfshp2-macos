#include "audio_out.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <vector>

extern FILE *g_hle_log;
static AudioPull g_pull;
static SDL_AudioStream *g_stream;

static void feed(void *, SDL_AudioStream *stream, int additional, int)
{
    // The core always supplies signed 16-bit stereo at AUDIO_RATE. SDL converts
    // to the device's format and calls us off the main/game threads.
    s16 samples[2048];
    while (additional > 0) {
        const int frames = std::min((additional + 3) / 4, 1024);
        g_pull(samples, frames);
        if (!SDL_PutAudioStreamData(stream, samples, frames * 4)) break;
        additional -= frames * 4;
    }
}

bool audio_out_start(AudioPull pull)
{
    g_pull = pull;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, AUDIO_RATE};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
    const bool ok = g_stream && SDL_ResumeAudioStreamDevice(g_stream);
    fprintf(g_hle_log, "audio: SDL %s (%s)\n", ok ? "open" : "unavailable", ok ? SDL_GetCurrentAudioDriver() : SDL_GetError());
    return ok;
}
