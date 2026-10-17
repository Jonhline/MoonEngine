/* Actual menu transport with deterministic page/loader/device boundaries.
   MIDI data and PCM copying come from the user's disc. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "menu.h"
#include "music.h"
#include "pregrid_audio.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "menu music line %d: %s\n", __LINE__, #x);                             \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
struct menu {
    const char *request;
};
static int loader, fading, music_faded, starts, stops;
static plat_audio_cb callback;
static void *owner;
static const char *page = "GameRoot";
int loading_screen_active(void)
{
    return loader;
}
int plat_sdl_music_fading(void)
{
    return fading;
}
int plat_audio_set_music(plat_audio_cb cb, void *user)
{
    callback = cb;
    owner = user;
    if (cb)
        starts++;
    else
        stops++;
    return 0;
}
void plat_audio_release_music(void *user)
{
    if (owner == user)
        plat_audio_set_music(NULL, NULL);
}
int plat_audio_play(const int16_t *p, size_t n, int r, int c)
{
    return 1;
}
uint64_t plat_time_ns(void)
{
    return 0;
}
const char *menu_music_name(menu *m)
{
    return m->request;
}
ui_project *menu_ui(menu *m)
{
    return NULL;
}
int menu_active_root(const menu *m)
{
    return 0;
}
const char *ui_root_name(const ui_project *ui, int root)
{
    return page;
}
double menu_option_number(menu *m, const char *key, double fallback)
{
    return fallback;
}
const char *menu_pcm_request(menu *m, int *volume)
{
    *volume = 120;
    return NULL;
}
void menu_pcm_parameters(menu *m, int *sequence, int *repeat, unsigned *serial)
{
    *sequence = -1;
    *repeat = 1;
    *serial = 0;
}
int menu_pcm_paused(menu *m)
{
    return 0;
}
void menu_pcm_state(menu *m, int playing)
{
}
void pregrid_audio_update_gated(pregrid_audio *p, const char *g, int volume, uint32_t clock,
                                int sequence, int repeat, unsigned serial, int publish)
{
}
void pregrid_audio_pause(pregrid_audio *p, int paused)
{
}
int pregrid_audio_playing(pregrid_audio *p)
{
    return 0;
}
#include "../../src/app/main_audio.inc"
static void ready(music_job *job, gt4_vol *vol, const char *name)
{
    snprintf(job->name, sizeof job->name, "%s", name);
    job->count = gt4_music_sequence_count(vol, name);
    job->music = gt4_music_open_sequence(vol, name, 0);
    CHECK(job->music && gt4_music_prepare(job->music,1));
    SDL_AtomicSet(&job->frames, 48000 * 12);
    SDL_AtomicSet(&job->ready, 1);
    SDL_AtomicSet(&job->volume, 120);
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    music_job startup = {0};
    music_prefix prefix = {&startup, 0, 1};
    publish_music_prefix(&prefix, 4096);
    if (argc > 2 && !strcmp(argv[2], "--short-prefix"))
        SDL_AtomicSet(&startup.ready, 1);
    CHECK(!SDL_AtomicGet(&startup.ready));
    publish_music_prefix(&prefix, PLAT_SDL_MUSIC_PREROLL);
    CHECK(SDL_AtomicGet(&startup.ready));
    music_job playlist_startup = {0};
    gt4_music *track[1] = {NULL};
    SDL_atomic_t track_ready[1] = {0}, track_frames[1] = {0};
    playlist_startup.playlist = track;
    playlist_startup.track_ready = track_ready;
    playlist_startup.track_frames = track_frames;
    prefix.job = &playlist_startup;
    publish_music_prefix(&prefix, 4096);
    CHECK(!SDL_AtomicGet(track_ready) && !SDL_AtomicGet(&playlist_startup.ready));
    publish_music_prefix(&prefix, PLAT_SDL_MUSIC_PREROLL);
    CHECK(SDL_AtomicGet(track_ready) && SDL_AtomicGet(&playlist_startup.ready));
    SDL_SetMainReady();
    const char *error = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol);
    music_job jobs[32] = {0};
    int count = 2;
    ready(&jobs[0], vol, "arcade");
    ready(&jobs[1], vol, "map");
    menu m = {"arcade"};
    menu_audio_context state = {0};
    state.jobs = jobs;
    state.count = &count;
    state.enabled = state.allowed = 1;
    state.owner = &m;
    preparing_audio = &state;
    /* No outer UI update occurs in this fixture: only the real Sync pump. */
    update_present_pcm(&m);
    CHECK(starts==1 && owner==&jobs[0]);
    int16_t first[1024], expected[1024], next[1024];
    gt4_music *reference = gt4_music_open_sequence(vol, "arcade", 0);
    CHECK(reference && gt4_music_prepare(reference,1));
    callback(first, 512, owner);
    gt4_music_render(expected, 512, reference);
    CHECK(!memcmp(first,expected,sizeof first));
    for (int i = 0; i < 12; i++)
        update_present_pcm(&m);
    loader = 1;
    update_present_pcm(&m);
    loader = 0;
    CHECK(starts==1 && !stops && owner==&jobs[0]);
    callback(next, 512, owner);
    gt4_music_render(expected, 512, reference);
    CHECK(!memcmp(next,expected,sizeof next));
    /* The script asks for a different group before panIn/Sync returns. */
    m.request = "map";
    if (argc > 2 && !strcmp(argv[2], "--defer-midi"))
        state.enabled = 0; /* Deliberately restore outer-loop-only publication. */
    update_present_pcm(&m);
    CHECK(starts==2 && owner==&jobs[1] && state.playing==&jobs[1]);
    /* An authored fade retains its producer until the device finishes it. */
    m.request = "";
    music_faded = 1;
    fading = 1;
    update_present_pcm(&m);
    CHECK(owner==&jobs[1] && music_faded);
    fading = 0;
    update_present_pcm(&m);
    CHECK(!owner && !music_faded && !state.playing);
    /* Revisit and keep must not retrigger a source in repeated Sync calls. */
    m.request = "arcade";
    jobs[0].played = 0;
    loader = 1;
    update_present_pcm(&m);
    CHECK(!owner);
    loader = 0;
    update_present_pcm(&m);
    int prior = starts;
    CHECK(owner==&jobs[0]);
    m.request = NULL;
    for (int i = 0; i < 20; i++)
        update_present_pcm(&m);
    CHECK(starts==prior && owner==&jobs[0]);
    state.allowed = 0;
    update_present_pcm(&m);
    CHECK(!owner);
    gt4_music_close(reference);
    for (int i = 0; i < 2; i++)
        gt4_music_close(jobs[i].music);
    gt4_vol_close(vol);
    preparing_audio = NULL;
    puts("Menu music: Sync publication, keep, sample continuity, loading and authored fade passed");
    return 0;
}
