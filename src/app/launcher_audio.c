/* PC launcher ownership of the existing native GTSE/SQT decoders. */
#include "launcher_audio.h"
#include "music.h"
#include "sound.h"
#include "vol.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
typedef struct { int16_t *pcm; size_t frames; } effect;
typedef struct { int effect; size_t position; int active; } voice;
struct launcher_audio {
    char *disc;
    SDL_Thread *thread;
    SDL_AudioDeviceID device;
    SDL_atomic_t ready, stop, available;
    gt4_music *music;
    effect effects[4];
    voice voices[8];
    int music_volume, effects_volume;
};
static const char *effect_name(int i)
{
    const char *names[] = {"cursor", "select", "ok", "cancel"};
    return i >= 0 && i < 4 ? names[i] : NULL;
}
static unsigned word(const uint8_t *p)
{
    return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
static void decode_effects(launcher_audio *a, gt4_vol *v)
{
    size_t bn = 0, nn = 0;
    uint8_t *bank = gt4_vol_read(v, "/sound/gt4sys.ins", &bn);
    uint8_t *names = gt4_vol_read(v, "/sound/gt4se.inf", &nn);
    /* Original: 00463098 installs bank 0; GTSE entries name its sequences. */
    if (bank && names && nn >= 16 && !memcmp(names, "GTSE", 4) && word(names + 12) <= (nn - 16) / 8)
        for (unsigned i = 0; i < word(names + 12); i++) {
            const uint8_t *e = names + 16 + i * 8;
            unsigned off = word(e);
            if (off >= nn || !memchr(names + off, 0, nn - off) || e[7]) continue;
            for (int k = 0; k < 4; k++) if (!strcmp((char *)names + off, effect_name(k)))
                gt4_sound_render(bank, bn, e[5], &a->effects[k].pcm, &a->effects[k].frames);
        }
    free(bank); free(names);
}
static void publish(void *user, unsigned frames)
{
    SDL_AtomicSet(&((launcher_audio *)user)->available, (int)frames);
}
static int load(void *user)
{
    launcher_audio *a = user;
    const char *error = NULL;
    gt4_vol *v = gt4_vol_open(a->disc, &error);
    if (v) {
        decode_effects(a, v);
        /* P: Options inherits its parent's BGM. The standalone launcher
           selects GT Mode's map sequence 0 through the native SQT player. */
        a->music = gt4_music_open(v, "map");
        if (a->music && !gt4_music_prepare_begin(a->music, 1)) {
            gt4_music_close(a->music); a->music = NULL;
        }
        gt4_vol_close(v);
    }
    SDL_AtomicSet(&a->ready, 1);
    while (!SDL_AtomicGet(&a->stop) && a->music && gt4_music_prepare_step(a->music, publish, a)) {}
    return 0;
}
static int clamp(int value)
{
    return value < -32768 ? -32768 : value > 32767 ? 32767 : value;
}
void launcher_audio_render(launcher_audio *a, int16_t *out, int frames)
{
    memset(out, 0, (size_t)frames * 4);
    if (!a || !SDL_AtomicGet(&a->ready)) return;
    gt4_music_render_available(out, frames, a->music, (unsigned)SDL_AtomicGet(&a->available));
    for (int i = 0; i < frames * 2; i++) out[i] = (int16_t)((int)out[i] * a->music_volume / 120);
    for (int k = 0; k < 8; k++) {
        voice *v = &a->voices[k];
        if (!v->active) continue;
        effect *e = &a->effects[v->effect];
        size_t take = e->frames - v->position;
        if (take > (size_t)frames) take = frames;
        for (size_t i = 0; i < take * 2; i++)
            out[i] = (int16_t)clamp(out[i] + (int)e->pcm[v->position * 2 + i] * a->effects_volume / 128);
        v->position += take;
        if (v->position == e->frames) v->active = 0;
    }
}
static void callback(void *user, Uint8 *stream, int bytes)
{
    launcher_audio_render(user, (int16_t *)stream, bytes / 4);
}
launcher_audio *launcher_audio_open(const char *disc, int device)
{
    if (!disc || !*disc) return NULL;
    launcher_audio *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->disc = strdup(disc); a->music_volume = a->effects_volume = 120;
    if (!a->disc) { free(a); return NULL; }
    if (device && SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec spec = {0}; spec.freq = 48000; spec.format = AUDIO_S16SYS;
        spec.channels = 2; spec.samples = 1024; spec.callback = callback; spec.userdata = a;
        a->device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    }
    a->thread = SDL_CreateThread(load, "launcher-audio", a);
    if (!a->thread) { launcher_audio_close(a); return NULL; }
    if (a->device) SDL_PauseAudioDevice(a->device, 0);
    return a;
}
void launcher_audio_close(launcher_audio *a)
{
    if (!a) return;
    if (a->device) SDL_CloseAudioDevice(a->device);
    SDL_AtomicSet(&a->stop, 1);
    if (a->thread) SDL_WaitThread(a->thread, NULL);
    gt4_music_close(a->music);
    for (int i = 0; i < 4; i++) free(a->effects[i].pcm);
    free(a->disc); free(a);
}
void launcher_audio_volume(launcher_audio *a, int music, int effects)
{
    if (!a) return;
    if (a->device) SDL_LockAudioDevice(a->device);
    a->music_volume = music < 0 ? 0 : music > 128 ? 128 : music;
    a->effects_volume = effects < 0 ? 0 : effects > 128 ? 128 : effects;
    if (a->device) SDL_UnlockAudioDevice(a->device);
}
void launcher_audio_play(launcher_audio *a, const char *name)
{
    if (!a || !name || !SDL_AtomicGet(&a->ready)) return;
    if (a->device) SDL_LockAudioDevice(a->device);
    for (int e = 0; e < 4; e++) if (!strcmp(name, effect_name(e)) && a->effects[e].pcm) {
        int slot = 0;
        for (int i = 0; i < 8; i++) if (!a->voices[i].active) { slot = i; break; }
        a->voices[slot] = (voice){e, 0, 1};
    }
    if (a->device) SDL_UnlockAudioDevice(a->device);
}
int launcher_audio_ready(launcher_audio *a)
{
    return a && SDL_AtomicGet(&a->ready) && SDL_AtomicGet(&a->available) >= 4096;
}
