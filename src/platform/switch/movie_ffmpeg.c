/* Linked libav movie backend (Switch, Linux, Windows, Android): bounded RGBA queue, in-process FFmpeg and the existing
   frontend music mixer. Only the render thread touches GPU textures. */
#include "movie.h"
#include "movie_decode.h"
#include "platform/platform.h"
#include "pss_audio.h"
#include <SDL.h>
#include <libavutil/log.h>
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__)
void moon_android_mark_frame(const char *where);
#define MOON_FRAME(where) moon_android_mark_frame(where)
#else
#define MOON_FRAME(where) ((void)0)
#endif

enum { MOVIE_QUEUE = 6 };
struct sdl_movie {
    uint8_t *data;
    gt4_movie_decoder *decoder;
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_cond *ready;
    SDL_atomic_t stop;
    int ended, head, count, rate;
    uint8_t *frames[MOVIE_QUEUE], *display;
    plat_texture *texture;
    uint64_t start, number, decode_ns, decode_max, decoded;
    int16_t *pcm;
    size_t audio_frames, audio_pos;
    int audio_enabled, audio_attempted, audio_playing;
};
static void movie_audio_render(int16_t *out, int frames, void *user)
{
    sdl_movie *m = user;
    size_t count = (size_t)frames, take = m->audio_frames - m->audio_pos;
    if (take > count)
        take = count;
    memset(out, 0, count * 4);
    memcpy(out, m->pcm + m->audio_pos * 2, take * 4);
    m->audio_pos += take;
}
void sdl_movie_audio(sdl_movie *m, int enabled)
{
    if (!m)
        return;
    m->audio_enabled = enabled;
    if (!enabled && m->audio_playing) {
        plat_audio_release_music(m);
        m->audio_playing = 0;
    }
}
static void start_audio(sdl_movie *m)
{
    if (!m->audio_enabled || !m->pcm || m->audio_attempted)
        return;
    m->audio_attempted = 1;
    m->audio_playing = plat_audio_set_music(movie_audio_render, m) == 0;
    fprintf(stderr, "movie: linked libav music output=%s\n",
            m->audio_playing ? "playing" : "unavailable");
}
static uint64_t playback_ns(sdl_movie *m)
{
    return m->audio_playing ? plat_audio_music_position() * 1000000000ull / 48000
           : m->start       ? plat_time_ns() - m->start
                            : 0;
}
static uint64_t frame_ns(const sdl_movie *m, uint64_t frame)
{
    return frame * 1001000000ull / (unsigned)m->rate;
}
static int decode(void *user)
{
    sdl_movie *m = user;
    /* FFmpeg's pixel/filter math uses its normal nearest rounding. This is
       a dedicated worker; never change the physics/render thread's FP mode. */
    fesetround(FE_TONEAREST);
    for (;;) {
        SDL_LockMutex(m->lock);
        while (m->count == MOVIE_QUEUE && !SDL_AtomicGet(&m->stop))
            SDL_CondWait(m->ready, m->lock);
        int slot = (m->head + m->count) % MOVIE_QUEUE;
        SDL_UnlockMutex(m->lock);
        if (SDL_AtomicGet(&m->stop))
            break;
        uint64_t before = plat_time_ns();
        int result = gt4_movie_decoder_read(m->decoder, m->frames[slot]);
        uint64_t elapsed = plat_time_ns() - before;
        if (result <= 0) {
            if (result < 0)
                fprintf(stderr, "movie: linked libav decoder ended with error %d\n", result);
            break;
        }
        m->decode_ns += elapsed;
        if (elapsed > m->decode_max)
            m->decode_max = elapsed;
        m->decoded++;
        SDL_LockMutex(m->lock);
        m->count++;
        SDL_UnlockMutex(m->lock);
    }
    SDL_LockMutex(m->lock);
    m->ended = 1;
    SDL_UnlockMutex(m->lock);
    return 0;
}
void sdl_movie_release_texture(sdl_movie *m)
{
    if (m) {
        plat_texture_destroy(m->texture);
        m->texture = NULL;
    }
}
int sdl_movie_ready(sdl_movie *m)
{
    if (!m)
        return 0;
    SDL_LockMutex(m->lock);
    int ready = m->count > 0;
    SDL_UnlockMutex(m->lock);
    return ready;
}
int sdl_movie_finished(sdl_movie *m)
{
    if (!m)
        return 0;
    SDL_LockMutex(m->lock);
    int done = m->ended && !m->count;
    SDL_UnlockMutex(m->lock);
    return done && (!m->number || playback_ns(m) >= frame_ns(m, m->number));
}
uint64_t sdl_movie_frame_count(const sdl_movie *m)
{
    return m ? m->number : 0;
}
void sdl_movie_close(sdl_movie *m)
{
    if (!m)
        return;
    sdl_movie_audio(m, 0);
    SDL_AtomicSet(&m->stop, 1);
    if (m->lock) {
        SDL_LockMutex(m->lock);
        if (m->ready)
            SDL_CondBroadcast(m->ready);
        SDL_UnlockMutex(m->lock);
    }
    if (m->thread)
        SDL_WaitThread(m->thread, NULL);
    gt4_movie_decoder_close(m->decoder);
    if (m->ready)
        SDL_DestroyCond(m->ready);
    if (m->lock)
        SDL_DestroyMutex(m->lock);
    for (unsigned i = 0; i < MOVIE_QUEUE; i++)
        free(m->frames[i]);
    if (m->decoded)
        fprintf(stderr, "movie: linked libav decoded=%llu mean_ms=%.3f max_ms=%.3f\n",
                (unsigned long long)m->decoded, (double)m->decode_ns / m->decoded / 1e6,
                (double)m->decode_max / 1e6);
    free(m->data);
    free(m->display);
    free(m->pcm);
    /* The frontend releases textures on the render thread before retirement.
       This fallback also handles an open failure, before any texture exists. */
    sdl_movie_release_texture(m);
    free(m);
}
static sdl_movie *open_owned(uint8_t *data, size_t size, int interlace, int repeat, int audio_track)
{
    sdl_movie *m = calloc(1, sizeof *m);
    if (!m) {
        free(data);
        return NULL;
    }
    m->data = data;
    m->rate = interlace ? 60 : 30;
    /* Equivalent to desktop ffmpeg -v error, with no per-frame logging. */
    av_log_set_level(AV_LOG_ERROR);
    MOON_FRAME("movie_ffmpeg.c: decoder open");
    m->decoder = gt4_movie_decoder_open(data, size, interlace, repeat);
    if (!m->decoder)
        goto fail;
    if (audio_track > 0 && !repeat) {
        if (gt4_pss_audio(data, size, (unsigned)audio_track, &m->pcm, &m->audio_frames))
            fprintf(stderr, "movie: linked libav audio track=%d frames=%zu stereo=48000\n", audio_track,
                    m->audio_frames);
        else
            fprintf(stderr, "movie: linked libav unsupported/missing audio track=%d\n", audio_track);
    }
    m->lock = SDL_CreateMutex();
    m->ready = SDL_CreateCond();
    if (!m->lock || !m->ready)
        goto fail;
    for (unsigned i = 0; i < MOVIE_QUEUE; i++) {
        m->frames[i] = malloc(GT4_MOVIE_BYTES);
        if (!m->frames[i])
            goto fail;
    }
    m->display = malloc(GT4_MOVIE_BYTES);
    if (!m->display)
        goto fail;
    m->thread = SDL_CreateThread(decode, "native MPEG-2 movie", m);
    if (!m->thread)
        goto fail;
    fprintf(stderr, "movie: linked libav decoder ready (in-process), fields=%d repeat=%d\n",
            interlace != 0, repeat != 0);
    return m;
fail:
    fprintf(stderr, "movie: linked libav decoder failed to initialize\n");
    sdl_movie_close(m);
    return NULL;
}
sdl_movie *sdl_movie_open_owned(uint8_t *data, size_t size, int interlace, int repeat,
                                int audio_track)
{
    fenv_t saved;
    int have_environment = fegetenv(&saved) == 0;
    if (!have_environment) {
        free(data);
        return NULL;
    }
    if (fesetround(FE_TONEAREST) != 0) {
        free(data);
        return NULL;
    }
    sdl_movie *m = open_owned(data, size, interlace, repeat, audio_track);
    fesetenv(&saved);
    return m;
}
sdl_movie *sdl_movie_open(const uint8_t *data, size_t size, int interlace, int repeat,
                          int audio_track)
{
    if (!data || !size || size > 512u * 1024 * 1024)
        return NULL;
    uint8_t *copy = malloc(size);
    if (!copy)
        return NULL;
    memcpy(copy, data, size);
    return sdl_movie_open_owned(copy, size, interlace, repeat, audio_track);
}
int sdl_movie_draw(sdl_movie *m, float x, float y, float w, float h, float opacity)
{
    if (!m)
        return 0;
    uint64_t now = plat_time_ns();
    int update = 0;
    if (!m->start && sdl_movie_ready(m))
        start_audio(m);
    SDL_LockMutex(m->lock);
    while (m->count && (!m->start || playback_ns(m) >= frame_ns(m, m->number))) {
        if (!m->start)
            m->start = now;
        memcpy(m->display, m->frames[m->head], GT4_MOVIE_BYTES);
        m->head = (m->head + 1) % MOVIE_QUEUE;
        m->count--;
        m->number++;
        update = 1;
        if (!m->audio_playing && now - m->start > frame_ns(m, m->number) + 100000000ull)
            m->start = now - frame_ns(m, m->number - 1);
        SDL_CondSignal(m->ready);
    }
    SDL_UnlockMutex(m->lock);
    if (update) {
        if (m->texture)
            plat_texture_update(m->texture, m->display);
        else
            m->texture = plat_texture_create(GT4_MOVIE_WIDTH, GT4_MOVIE_HEIGHT, m->display);
    }
    if (!m->texture)
        return 0;
    uint8_t tint[4] = {255, 255, 255, (uint8_t)(255 * opacity)};
    plat_draw_quad(m->texture, x, y, w, h, 0, 0, 1, 1, tint);
    return 1;
}

/* Original IPU archive pictures share the decoder but have no playback clock. */
uint8_t *sdl_movie_ipic_decode(const uint8_t *stream, size_t size, unsigned count)
{
    if (!stream || size < 16 || memcmp(stream, "ipum", 4) || !count ||
        count > SIZE_MAX / GT4_MOVIE_BYTES)
        return NULL;
    fenv_t saved;
    if (fegetenv(&saved) || fesetround(FE_TONEAREST))
        return NULL;
    MOON_FRAME("movie_ffmpeg.c: IPU decoder open");
    gt4_movie_decoder *decoder = gt4_movie_decoder_open(stream, size, 0, 0);
    uint8_t *pixels = decoder ? malloc((size_t)count * GT4_MOVIE_BYTES) : NULL;
    unsigned copied = 0;
    if (pixels)
        while (copied < count) {
            int result = gt4_movie_decoder_read(decoder, pixels + (size_t)copied * GT4_MOVIE_BYTES);
            if (result != 1) {
                fprintf(stderr, "movie: linked libav IPU: picture=%u/%u decode=%d\n", copied, count, result);
                break;
            }
            copied++;
        }
    else
        fprintf(stderr, "movie: linked libav IPU: decoder=%s allocation=%s\n", decoder ? "ready" : "failed",
                pixels ? "ready" : "failed");
    gt4_movie_decoder_close(decoder);
    fesetenv(&saved);
    if (copied != count) {
        free(pixels);
        return NULL;
    }
    return pixels;
}
