/* sound.c — WAV playback.
 * Original: 38c2 loads resources 432/433/434 (tick/win/boom) via
 * LoadResource+sndPlaySound(SND_SYNC); 38ed plays only when sound==3;
 * 38d7 purges (sndPlaySound(-1,SND_ASYNC)) only when sound==3. */
#include "winmine.h"
#include <stdio.h>
#include <string.h>

static SDL_AudioDeviceID dev = 0;
static SDL_AudioSpec spec;
static SDL_AudioSpec tick_spec;
static Uint8 *tick_buf = NULL; static Uint32 tick_len = 0;
static Uint8 *win_buf = NULL;  static Uint32 win_len = 0;
static Uint8 *boom_buf = NULL; static Uint32 boom_len = 0;

static int load_wav(const char *dir, const char *name,
                    Uint8 **buf, Uint32 *len, SDL_AudioSpec *sp)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!SDL_LoadWAV(path, sp, buf, len)) {
        SDL_Log("sound: cannot load %s: %s", path, SDL_GetError());
        return 0;
    }
    return 1;
}

/* 38c2: returns success; caller sets sound = 2 + success */
int sound_init(void)
{
    const char *dir = render_asset_dir();
    if (!load_wav(dir, "tick.wav", &tick_buf, &tick_len, &tick_spec))
        return 0;
    spec = tick_spec;
    dev = SDL_OpenAudioDevice(0, 0, &spec, NULL, 0);
    if (!dev) {
        SDL_Log("sound: no audio device: %s", SDL_GetError());
        SDL_FreeWAV(tick_buf);
        tick_buf = NULL;
        return 0;
    }
    SDL_AudioSpec tmp;
    load_wav(dir, "win.wav", &win_buf, &win_len, &tmp);
    load_wav(dir, "boom.wav", &boom_buf, &boom_len, &tmp);
    return 1;
}

void sound_play(int id)
{
    /* 38ed: plays only when DAT_010056b8 == 3 */
    if (G.sound != 3 || !dev) return;
    Uint8 *buf; Uint32 len;
    switch (id) {
    case 1: buf = tick_buf; len = tick_len; break;   /* tick */
    case 2: buf = win_buf;  len = win_len;  break;   /* win  */
    case 3: buf = boom_buf; len = boom_len; break;   /* boom */
    default: return;
    }
    if (!buf) return;
    SDL_QueueAudio(dev, buf, len);
}

/* 38d7: purge queued audio; original gates on sound==3 as well */
void sound_stop(void)
{
    if (G.sound != 3 || !dev) return;
    SDL_ClearQueuedAudio(dev);
}

void sound_exit(void)
{
    if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
    if (tick_buf) { SDL_FreeWAV(tick_buf); tick_buf = NULL; }
    if (win_buf)  { SDL_FreeWAV(win_buf);  win_buf = NULL; }
    if (boom_buf) { SDL_FreeWAV(boom_buf); boom_buf = NULL; }
}
