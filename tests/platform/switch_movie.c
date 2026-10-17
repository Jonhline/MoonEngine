/* Exercise the actual Switch decoder/queue on the host. Reference RGBA comes
   from a separate ffmpeg CLI invocation with movie.c's desktop filter chain. */
#include "../../src/platform/switch/movie_ffmpeg.c"
#include <math.h>

struct plat_texture {
    uint8_t rgba[GT4_MOVIE_BYTES];
};
static SDL_atomic_t fake_ms;
static SDL_threadID render_thread;
static int wrong_thread;
static plat_texture *last_texture;
static plat_audio_cb music_source;
static void *music_user;
static uint64_t music_position;
uint64_t plat_time_ns(void)
{
    return 1000000000ull + (uint64_t)SDL_AtomicGet(&fake_ms) * 1000000;
}
plat_texture *plat_texture_create(int w, int h, const uint8_t *rgba)
{
    if (SDL_ThreadID() != render_thread || w != 640 || h != 448)
        wrong_thread = 1;
    plat_texture *t = malloc(sizeof *t);
    if (t)
        memcpy(t->rgba, rgba, GT4_MOVIE_BYTES);
    last_texture = t;
    return t;
}
void plat_texture_update(plat_texture *t, const uint8_t *rgba)
{
    if (SDL_ThreadID() != render_thread)
        wrong_thread = 1;
    memcpy(t->rgba, rgba, GT4_MOVIE_BYTES);
    last_texture = t;
}
void plat_texture_destroy(plat_texture *t)
{
    if (t && SDL_ThreadID() != render_thread)
        wrong_thread = 1;
    if (last_texture == t)
        last_texture = NULL;
    free(t);
}
void plat_draw_quad(const plat_texture *t, float x, float y, float w, float h, float u0, float v0,
                    float u1, float v1, const uint8_t c[4])
{
    (void)t;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    (void)c;
    if (SDL_ThreadID() != render_thread)
        wrong_thread = 1;
}
int plat_audio_set_music(plat_audio_cb source, void *user)
{
    music_source = source;
    music_user = user;
    music_position = 0;
    return 0;
}
void plat_audio_release_music(void *owner)
{
    if (music_user == owner) {
        music_source = NULL;
        music_user = NULL;
    }
}
uint64_t plat_audio_music_position(void)
{
    return music_position;
}

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "movie test %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t *read_file(const char *path, size_t *bytes)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    uint8_t *data = n > 0 ? malloc((size_t)n) : NULL;
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *bytes = n > 0 ? (size_t)n : 0;
    return data;
}
static int pixels_equal(const uint8_t *a, const uint8_t *b)
{
    for (size_t i = 0; i < GT4_MOVIE_BYTES; i++)
        if (abs((int)a[i] - b[i]) > 1) {
            fprintf(stderr, "movie pixel byte=%zu actual=%u expected=%u\n", i, a[i], b[i]);
            return 0;
        }
    return 1;
}
static int buffered(sdl_movie *m)
{
    SDL_LockMutex(m->lock);
    int count = m->count;
    SDL_UnlockMutex(m->lock);
    return count;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 4);
    av_log_set_level(AV_LOG_ERROR);
    size_t bytes, ref_bytes;
    uint8_t *data = read_file(argv[1], &bytes), *reference = read_file(argv[3], &ref_bytes);
    CHECK(data && reference && ref_bytes % GT4_MOVIE_BYTES == 0);
    int interlace = atoi(argv[2]), negative = argc > 4 && !strcmp(argv[4], "--negative");
    if (argc > 4 && !strcmp(argv[4], "--field-negative"))
        interlace = !interlace;
    int loop = argc > 4 && !strcmp(argv[4], "--loop");
    int audio = argc > 4 && !strcmp(argv[4], "--audio");
    if (negative)
        reference[0] ^= 64;
    CHECK(!gt4_movie_decoder_open((const uint8_t *)"invalid", 7, 0, 0));
    gt4_movie_decoder *d = gt4_movie_decoder_open(data, bytes, interlace, 0);
    CHECK(d);
    size_t frames = ref_bytes / GT4_MOVIE_BYTES;
    uint8_t *frame = malloc(GT4_MOVIE_BYTES);
    CHECK(frame);
    for (size_t i = 0; i < frames; i++) {
        int result = gt4_movie_decoder_read(d, frame);
        if (result != 1)
            fprintf(stderr, "movie ended at frame=%zu expected=%zu result=%d\n", i, frames, result);
        CHECK(result == 1);
        CHECK(pixels_equal(frame, reference + i * GT4_MOVIE_BYTES));
    }
    if (loop)
        CHECK(gt4_movie_decoder_read(d, frame) == 0);
    gt4_movie_decoder_close(d);
    CHECK(!SDL_Init(SDL_INIT_TIMER));
    render_thread = SDL_ThreadID();
    int original_rounding = fegetround();
    CHECK(fesetround(FE_TOWARDZERO) == 0);
    sdl_movie *m = sdl_movie_open(data, bytes, interlace, 0, audio ? 1 : 0);
    CHECK(m && fegetround() == FE_TOWARDZERO);
    CHECK(fesetround(original_rounding) == 0);
    uint32_t deadline = SDL_GetTicks() + 10000;
    while (!sdl_movie_ready(m) && SDL_GetTicks() < deadline)
        SDL_Delay(1);
    CHECK(sdl_movie_ready(m));
    if (audio) {
        CHECK(m->pcm && m->audio_frames > 1024);
        sdl_movie_audio(m, 1);
        CHECK(sdl_movie_draw(m, 0, 0, 640, 480, 1));
        CHECK(music_source && music_user == m && m->audio_playing);
        int16_t samples[2048];
        music_source(samples, 1024, music_user);
        CHECK(memcmp(samples, m->pcm, sizeof samples) == 0 && m->audio_pos == 1024);
        sdl_movie_audio(m, 0);
        CHECK(!music_source && !m->audio_playing);
    } else {
        while (sdl_movie_frame_count(m) < frames && SDL_GetTicks() < deadline) {
            sdl_movie_draw(m, 0, 0, 640, 480, 1);
            uint64_t count = sdl_movie_frame_count(m);
            if (count && count <= frames)
                CHECK(last_texture && pixels_equal(last_texture->rgba,
                                                   reference + (count - 1) * GT4_MOVIE_BYTES));
            SDL_AtomicAdd(&fake_ms, interlace ? 17 : 34);
            SDL_Delay(1);
        }
        CHECK(sdl_movie_frame_count(m) >= frames);
        if (loop) {
            SDL_AtomicAdd(&fake_ms, 100);
            while (!sdl_movie_finished(m) && SDL_GetTicks() < deadline)
                SDL_Delay(1);
            CHECK(sdl_movie_finished(m));
        }
    }
    sdl_movie_release_texture(m);
    sdl_movie_close(m);
    CHECK(!wrong_thread);
    /* Repeating movie: queue can fill while the render thread is idle. Cancel
       must wake the producer, join it, and release without a GPU call there. */
    m = sdl_movie_open(data, bytes, interlace, 1, 0);
    CHECK(m);
    deadline = SDL_GetTicks() + 10000;
    while (buffered(m) < MOVIE_QUEUE && SDL_GetTicks() < deadline)
        SDL_Delay(1);
    CHECK(buffered(m) == MOVIE_QUEUE && !sdl_movie_finished(m));
    uint32_t before = SDL_GetTicks();
    sdl_movie_close(m);
    CHECK(SDL_GetTicks() - before < 2000 && !wrong_thread);
    if (loop) {
        d = gt4_movie_decoder_open(data, bytes, interlace, 1);
        CHECK(d);
        for (size_t i = 0; i < frames * 2 + 1; i++) {
            CHECK(gt4_movie_decoder_read(d, frame) == 1);
            CHECK(pixels_equal(frame, reference + (i % frames) * GT4_MOVIE_BYTES));
        }
        gt4_movie_decoder_close(d);
    }
    free(data);
    free(reference);
    free(frame);
    SDL_Quit();
    printf(
        "Switch movie: %zu reference frames, fields=%d; queue/cancel/GPU ownership/FPU/audio passed\n",
        frames, interlace);
    return 0;
}
