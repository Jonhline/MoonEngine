/* PC launcher ownership of the existing native GTSE/SQT decoders. */
#include "launcher_audio.h"
#include "music_override.h"
#include "music.h"
#include "sound.h"
#include "vol.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { int16_t *pcm; size_t frames; } effect;
typedef struct { int effect; size_t position; int active; } voice;
struct launcher_audio {
    char *disc;
    SDL_Thread *thread;
    SDL_AudioDeviceID device;
    SDL_atomic_t ready, stop, available, suspended;
    gt4_music *music;
    /* P: the launcher shuffles the game's menu music. The worker prepares the next track
       whole while one plays; the callback swaps at the end and hands the old one back. */
    gt4_music *next, *old;
    unsigned next_frames;
    /* the crossfade: frames of the current track played, and how far into the fade */
    size_t played, fade_pos;
    int fading;
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
    for (int k = 0; k < 4; k++)
    {
            int peak = 0;
            for (size_t i = 0; i < a->effects[k].frames * 2; i++)
                if (abs(a->effects[k].pcm[i]) > peak)
                    peak = abs(a->effects[k].pcm[i]);
            fprintf(stderr, "launcher audio: effect %s %zu frames peak %d\n", effect_name(k),
                    a->effects[k].frames, peak);
        }
}
static void publish(void *user, unsigned frames)
{
    SDL_AtomicSet(&((launcher_audio *)user)->available, (int)frames);
}
/* The menu music sets the launcher draws from, every sequence of each (the race and the
   licence/mission sets stay with their screens). */
static const char *const shuffle_sets[] = {"map",    "home",          "hall",  "arcade",
                                           "gtauto", "replaytheater", "bgmmid"};
#define SHUFFLE_SETS (int)(sizeof shuffle_sets / sizeof *shuffle_sets)
static gt4_music *shuffle_open(gt4_vol *v, const unsigned counts[], unsigned total, int *last)
{
    for (int tries = 0; tries < 8 && total; tries++) {
        int pick = (int)((unsigned)rand() % total), set = 0;
        while (set < SHUFFLE_SETS && pick >= (int)counts[set])
            pick -= (int)counts[set++];
        int id = set * 256 + pick;
        if (set >= SHUFFLE_SETS || (id == *last && total > 1))
            continue;
        gt4_music *m = gt4_music_open_sequence(v, shuffle_sets[set], (unsigned)pick);
        if (m && gt4_music_prepare_begin(m, 1)) {
            *last = id;
            fprintf(stderr, "launcher audio: playing %s %d\n", shuffle_sets[set], pick);
            return m;
        }
        gt4_music_close(m);
    }
    return NULL;
}
static void publish_count(void *user, unsigned frames)
{
    *(unsigned *)user = frames;
}
static int load(void *user)
{
    launcher_audio *a = user;
    const char *error = NULL;
    gt4_vol *v = gt4_vol_open(a->disc, &error);
    unsigned counts[SHUFFLE_SETS] = {0}, total = 0;
    int last = -1;
    if (v) {
        decode_effects(a, v);
        for (int i = 0; i < SHUFFLE_SETS; i++)
            total += counts[i] = gt4_music_sequence_count(v, shuffle_sets[i]);
        srand((unsigned)SDL_GetTicks() ^ (unsigned)(uintptr_t)a);
        /* MOON_LAUNCHER_FIRST_TRACK=<set>: a fixed first track (tests compare its PCM) */
        const char *first = getenv("MOON_LAUNCHER_FIRST_TRACK");
        if (first && *first) {
            a->music = gt4_music_open_sequence(v, first, 0);
            if (a->music && !gt4_music_prepare_begin(a->music, 1)) {
                gt4_music_close(a->music);
                a->music = NULL;
            }
        }
        if (!a->music)
            a->music = shuffle_open(v, counts, total, &last);
    }
    SDL_AtomicSet(&a->ready, 1);
    while (!SDL_AtomicGet(&a->stop) && a->music && gt4_music_prepare_step(a->music, publish, a)) {}
    while (v && !SDL_AtomicGet(&a->stop)) {
        gt4_music *m = shuffle_open(v, counts, total, &last);
        unsigned frames = 0;
        while (m && !SDL_AtomicGet(&a->stop) && gt4_music_prepare_step(m, publish_count, &frames)) {}
        if (!m || SDL_AtomicGet(&a->stop)) {
            gt4_music_close(m);
            break;
        }
        if (a->device) SDL_LockAudioDevice(a->device);
        a->next = m;
        a->next_frames = frames;
        if (a->device) SDL_UnlockAudioDevice(a->device);
        /* wait for the callback to take it at the current track's end */
        for (;;) {
            if (a->device) SDL_LockAudioDevice(a->device);
            int taken = a->next == NULL;
            gt4_music *old = a->old;
            a->old = NULL;
            if (a->device) SDL_UnlockAudioDevice(a->device);
            gt4_music_close(old);
            if (taken || SDL_AtomicGet(&a->stop) || !a->device)
                break;
            SDL_Delay(50);
        }
    }
    if (v) gt4_vol_close(v);
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
    if(!SDL_AtomicGet(&a->suspended)) {
    if(!moon_music_override_render(4,out,frames,a->music_volume*128/120,0)) {
    gt4_music_render_available(out, frames, a->music, (unsigned)SDL_AtomicGet(&a->available));
    /* P: the next track comes in over the last three seconds of this one (a crossfade);
       the current one keeps playing through its loop point while it fades. Its length is
       final here: the worker prepares the next only after this one is complete. */
    enum { XFADE = 48000 * 3, MAXCHUNK = 4096 };
    a->played += (size_t)frames;
    static long cap = -1; /* dev (MOON_LAUNCHER_TRACK_SECONDS): shorter tracks, to hear fades */
    if (cap < 0)
        cap = getenv("MOON_LAUNCHER_TRACK_SECONDS") ? atol(getenv("MOON_LAUNCHER_TRACK_SECONDS")) * 48000 : 0;
    size_t length = (size_t)SDL_AtomicGet(&a->available);
    if (cap > 0 && (size_t)cap < length)
        length = (size_t)cap;
    if (!a->fading && a->next && a->music && a->played + XFADE >= length) {
        a->fading = 1;
        a->fade_pos = 0;
        gt4_music_rewind(a->next);
    }
    if (a->fading && a->next) {
        static int16_t in[MAXCHUNK * 2];
        for (int done = 0; done < frames;) {
            int take = frames - done > MAXCHUNK ? MAXCHUNK : frames - done;
            gt4_music_render_available(in, take, a->next, a->next_frames);
            for (int i = 0; i < take; i++) {
                float g = (float)(a->fade_pos + (size_t)i) / XFADE;
                if (g > 1) g = 1;
                g = g * g * (3 - 2 * g);
                for (int c = 0; c < 2; c++) {
                    int16_t *o = out + (done + i) * 2 + c;
                    *o = (int16_t)clamp((int)(*o * (1 - g) + in[i * 2 + c] * g));
                }
            }
            a->fade_pos += (size_t)take;
            done += take;
        }
        if (a->fade_pos >= XFADE) { /* the next track is the track now */
            a->old = a->music;
            a->music = a->next;
            a->next = NULL;
            a->fading = 0;
            a->played = a->fade_pos;
            SDL_AtomicSet(&a->available, (int)a->next_frames);
        }
    }
    for (int i = 0; i < frames * 2; i++) out[i] = (int16_t)((int)out[i] * a->music_volume / 120);
    } /* Original music or custom music, then the same menu effects. */
    }
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
void launcher_audio_suspend_music(launcher_audio *a, int suspended)
{
    if (a) SDL_AtomicSet(&a->suspended, !!suspended);
}
static void callback(void *user, Uint8 *stream, int bytes)
{
    launcher_audio_render(user, (int16_t *)stream, bytes / 4);
    /* dev (MOON_LAUNCHER_AUDIO_PROBE): the loudest sample sent, once a second, before the
       volume is applied by the device - proves sound leaves without listening to it */
    static int probe = -1, peak, blocks;
    if (probe < 0)
        probe = getenv("MOON_LAUNCHER_AUDIO_PROBE") != NULL;
    if (probe) {
        launcher_audio *a = user;
        const int16_t *s = (const int16_t *)stream;
        for (int i = 0; i < bytes / 2; i++)
            if (abs(s[i]) > peak)
                peak = abs(s[i]);
        if (++blocks * 1024 >= 48000) {
            fprintf(stderr, "launcher audio: peak %d (music volume %d)\n", peak, a->music_volume);
            peak = blocks = 0;
        }
        if (getenv("MOON_LAUNCHER_AUDIO_PROBE")[0] == 'm')
            memset(stream, 0, bytes); /* "mute": measure, then send silence */
    }
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
        if (!a->device)
            fprintf(stderr, "launcher audio: no device (%s)\n", SDL_GetError());
    } else if (device)
        fprintf(stderr, "launcher audio: SDL audio unavailable (%s)\n", SDL_GetError());
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
    gt4_music_close(a->next);
    gt4_music_close(a->old);
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
        if (getenv("MOON_LAUNCHER_AUDIO_PROBE"))
            fprintf(stderr, "launcher audio: play %s\n", name);
    }
    if (a->device) SDL_UnlockAudioDevice(a->device);
}
int launcher_audio_ready(launcher_audio *a)
{
    return a && SDL_AtomicGet(&a->ready) && SDL_AtomicGet(&a->available) >= 4096;
}
