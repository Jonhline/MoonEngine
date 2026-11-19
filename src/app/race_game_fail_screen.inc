/* Custom Modes: its own copy of the licence-failure presentation.
   This file is private to Custom Modes.  The licence, training and mission
   paths keep using src/core/race_result.c and src/app/race_game_hud.inc
   untouched; nothing here is called by them. */
#include "strobe_player.h"
#include "rtext.h"

#define CFAIL_NO_TIME 0x157529FFu

struct race_fail_movie {
    race_game *game;
    strb_player *movie;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok, state, over, retry, first;
    char time_text[24], difference[32], title[96];
    plat_texture *screen;
    int image_options[4], image_valid, image_active;
    uint8_t image_lut[256][3];
    float saturation;
    int pad_held[7];
    uint64_t opened_ns;
    int test_key_done;
    int pending_key;
    uint64_t pending_at;
    char course[32], car[96], others[MOON_MAX_CARS - 1][96];
    const char *other_ptr[MOON_MAX_CARS - 1];
    int nothers, color, field_size, thunder;
};

/* COPY of src/core/race_result.c:53 format_time — custom modes owns this
   copy; do not re-sync with the licence path. */
static void cfail_format_time(uint32_t ms, char *out, size_t len)
{
    unsigned s, m;
    if (ms == CFAIL_NO_TIME) {
        snprintf(out, len, "\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f");
        return;
    }
    s = ms / 1000;
    m = s / 60;
    if (m / 60)
        snprintf(out, len, "%u:%02u'%02u.%03u", m / 60, m % 60, s % 60, ms % 1000);
    else
        snprintf(out, len, "%u'%02u.%03u", m % 60, s % 60, ms % 1000);
}

/* COPY of src/core/race_result.c:93 call() — the licence-failure commands
   only.  Custom modes owns this copy; do not re-sync with the licence path. */
static int cfail_call(void *user, const char *name, const strb_value *args, int nargs,
                      strb_value *ret)
{
    struct race_fail_movie *r = user;
    char upper[64];
    size_t i;
    memset(ret, 0, sizeof *ret);
    for (i = 0; i < sizeof upper - 1 && name[i]; i++)
        upper[i] = name[i] >= 'a' && name[i] <= 'z' ? name[i] - 32 : name[i];
    upper[i] = 0;
    if (!strcmp(upper, "PRIZELEVEL")) {
        ret->t = STRB_NUM;
        ret->n = r->state == 1 || r->state == 2   ? 0
                 : r->state == 3                  ? 1
                 : r->state == 4 || r->state == 5 ? 2
                                                  : 3;
        return 1;
    }
    if (!strcmp(upper, "GETTIME")) {
        ret->t = STRB_STR;
        snprintf(ret->s, sizeof ret->s, "%s", r->time_text);
        return 1;
    }
    if (!strcmp(upper, "GETTIMEDIF")) {
        ret->t = STRB_STR;
        snprintf(ret->s, sizeof ret->s, "%s", r->difference);
        return 1;
    }
    if (!strcmp(upper, "GETTITLE")) {
        ret->t = STRB_STR;
        snprintf(ret->s, sizeof ret->s, "%s", r->title);
        return 1;
    }
    if (!strcmp(upper, "GETLECTURE")) {
        ret->t = STRB_STR;
        return 1;
    }
    if (!strcmp(upper, "EXIT_MENU")) {
        r->retry = nargs && args[0].t == STRB_STR && !strcmp(args[0].s, "retry");
        fprintf(stderr, "custom modes: fail screen button %s\n", r->retry ? "RETRY" : "EXIT");
        r->over = 1;
        return 1;
    }
    if (!strcmp(upper, "PAUSEOFF"))
        return 1;
    if (!strcmp(upper, "PLAY_SOUND"))
        return 1;
    return 1;
}

/* COPY of src/app/race_game_hud.inc:39 draw_license_screen — Custom Modes
   owns this copy; do not re-sync with the licence path. */
static void cfail_screen_draw(void *user, float x, float y, float w, float h)
{
    struct race_fail_movie *r = user;
    int options[4];
    float parameters[6];
    uint8_t mix[3][3];
    if (!r || !r->screen)
        return;
    race_pause_display_options(options);
    gt4_license_image_options(options, parameters);
    if (!r->image_valid || memcmp(options, r->image_options, sizeof options)) {
        gt4_license_image_lut(parameters, r->image_lut);
        memcpy(r->image_options, options, sizeof options);
        r->image_valid = 1;
    }
    gt4_license_image_mix(parameters[5] * r->saturation, mix);
    if (r->image_active)
        video_license_image(r->screen, x, y, w, h, r->image_lut, mix);
    else {
        const uint8_t white[4] = {255, 255, 255, 255};
        plat_draw_quad(r->screen, x, y, w, h, 0, 1, 1, 0, white);
    }
    r->image_active = 1;
    r->saturation = gt4_license_image_decay(r->saturation);
}

/* COPY of src/core/race_result.c:199 ext() — licence "screen" only.  Custom
   modes owns this copy; do not re-sync with the licence path. */
static void cfail_ext(void *user, const char *name, float x, float y, float w, float h,
                      const uint8_t color[4])
{
    struct race_fail_movie *r = user;
    (void)color;
    if (r && !strcmp(name, "screen"))
        cfail_screen_draw(r, x, y, w, h);
}

static void cfail_close(struct race_fail_movie *r)
{
    if (!r)
        return;
    strb_close(r->movie);
    free(r->rt_data);
    plat_texture_destroy(r->screen);
    free(r);
}

/* COPY of src/core/race_result.c:305 race_result_license_open — failure
   (state 0) with the custom race time.  Custom modes owns this copy; do not
   re-sync with the licence path. */
static struct race_fail_movie *cfail_open(race_game *g, uint32_t time)
{
    struct race_fail_movie *r = calloc(1, sizeof *r);
    strb_host host = {0};
    size_t n = 0;
    if (!r)
        return NULL;
    r->game = g;
    r->state = 0;
    r->first = 1;
    r->saturation = 1.f;
    r->opened_ns = plat_time_ns();
    snprintf(r->course, sizeof r->course, "%s", g->last_course);
    snprintf(r->car, sizeof r->car, "%s", g->last_car);
    r->color = g->last_color;
    r->nothers = g->last_nothers;
    r->field_size = field_size_request ? field_size_request : g->last_nothers + 1;
    r->thunder = g->elim_thunder;
    for (int i = 0; i < g->last_nothers && i < MOON_MAX_CARS - 1; i++) {
        snprintf(r->others[i], sizeof r->others[i], "%s", g->last_others[i]);
        r->other_ptr[i] = r->others[i];
    }
    cfail_format_time(time, r->time_text, sizeof r->time_text);
    r->rt_data = gt4_vol_read(g->vol, "/rtext/raceUS.rt2", &n);
    r->rt_ok = r->rt_data && gt4_rtext_open(&r->rt, r->rt_data, n);
    host.call = cfail_call;
    host.ext = cfail_ext;
    host.user = r;
    r->movie = strb_open(g->vol, "/menu/US/result_license.strb", r->rt_ok ? &r->rt : NULL,
                         "RaceDisplay", &host);
    if (!r->movie) {
        cfail_close(r);
        return NULL;
    }
    strb_navigate(r->movie, 0);
    return r;
}

int race_game_fail_active(const race_game *g)
{
    return g && g->cfail ? 1 : 0;
}

void race_game_fail_open(race_game *g)
{
    if (!g || g->cfail)
        return;
    g->cfail = cfail_open(g, g->player_fail_time);
    if (g->cfail) {
        fprintf(stderr, "custom modes: fail screen open state=0 time=%u\n", g->player_fail_time);
    } else
        fprintf(stderr, "custom modes: fail screen unavailable\\n");
}

/* The licence movie's own navigation: Up then Enter is Retry, Down then
   Enter is Exit (the same pairs tests/gtmode/license_app.py uses).  The
   Enter follows one update later, as the movie's focus needs a tick. */
static void cfail_activate(struct race_fail_movie *r, int key)
{
    if (!r || !r->movie)
        return;
    if (key == 13) {
        strb_key(r->movie, 38);
        r->pending_key = 13;
        r->pending_at = plat_time_ns() + 120000000ull;
    } else if (key == 27) {
        strb_key(r->movie, 40);
        r->pending_key = 13;
        r->pending_at = plat_time_ns() + 120000000ull;
    } else
        strb_key(r->movie, key);
}

void race_game_fail_update(race_game *g, double dt)
{
    struct race_fail_movie *r;
    (void)dt;
    if (!g || !(r = g->cfail))
        return;
    (void)gt4_license_result_tick(NULL);
    {
        float movie_dt = gt4_license_result_tick(&r->first);
        if (movie_dt > 0)
            strb_tick(r->movie, movie_dt);
    }
    if (r->pending_key && plat_time_ns() >= r->pending_at) {
        strb_key(r->movie, r->pending_key);
        r->pending_key = 0;
    }
    { /* tests: MOON_FAIL_TEST_KEY=<ms>:<13|27> */
        const char *spec = getenv("MOON_FAIL_TEST_KEY");
        if (spec && !r->test_key_done) {
            long ms = 0;
            int key = 0;
            if (sscanf(spec, "%ld:%d", &ms, &key) == 2 && (key == 13 || key == 27) &&
                plat_time_ns() - r->opened_ns >= (uint64_t)ms * 1000000ull) {
                cfail_activate(r, key);
                fprintf(stderr, "custom modes: fail screen key %d\n", key);
                r->test_key_done = 1;
            }
        }
    }
}

void race_game_fail_input(race_game *g)
{
    struct race_fail_movie *r;
    SDL_GameController *pad;
    static const SDL_GameControllerButton buttons[6] = {
        SDL_CONTROLLER_BUTTON_DPAD_UP,   SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_A,         SDL_CONTROLLER_BUTTON_B};
    static const int codes[6] = {38, 40, 37, 39, 13, 13};
    if (!g || !(r = g->cfail))
        return;
    pad = plat_sdl_pad(0);
    for (int i = 0; i < 6; i++) {
        int down = pad && SDL_GameControllerGetButton(pad, buttons[i]);
        if (down && !r->pad_held[i]) {
            if (i == 4)
                cfail_activate(r, 13);
            else if (i == 5)
                cfail_activate(r, 27);
            else
                strb_key(r->movie, codes[i]);
        }
        r->pad_held[i] = down;
    }
}

void race_game_draw_fail(race_game *g, double dt)
{
    struct race_fail_movie *r;
    const uint8_t black[4] = {0, 0, 0, 255};
    (void)dt;
    if (!g || !(r = g->cfail))
        return;
    r->screen = video_grab(r->screen, 0, 0, 640, 448);
    plat_video_set_view(640, 480);
    plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
    strb_draw(r->movie);
}

int race_game_fail_key(race_game *g, int key)
{
    struct race_fail_movie *r;
    int code;
    if (!g || !(r = g->cfail))
        return 0;
    code = key == SDLK_LEFT                         ? 37
           : key == SDLK_UP                         ? 38
           : key == SDLK_RIGHT                      ? 39
           : key == SDLK_DOWN                       ? 40
           : key == SDLK_RETURN || key == SDLK_KP_ENTER ? 13
           : key == SDLK_ESCAPE                     ? 27
                                                    : 0;
    if (!code)
        return 0;
    cfail_activate(r, code);
    return 1;
}

int race_game_fail_press(race_game *g)
{
    if (!g || !g->cfail)
        return 0;
    strb_key(g->cfail->movie, 13);
    return 1;
}

int race_game_fail_over(const race_game *g)
{
    return g && g->cfail && g->cfail->over;
}

int race_game_fail_retry(const race_game *g)
{
    return g && g->cfail && g->cfail->over && g->cfail->retry;
}

int race_game_fail_retry_race(race_game *g, char *msg, size_t len)
{
    struct race_fail_movie *r = g ? g->cfail : NULL;
    char course[32], car[96], others[MOON_MAX_CARS - 1][96];
    const char *other_ptr[MOON_MAX_CARS - 1];
    int nothers, color, field, thunder, i;
    if (!r) {
        snprintf(msg, len, "no failed custom race");
        return 0;
    }
    snprintf(course, sizeof course, "%s", r->course);
    snprintf(car, sizeof car, "%s", r->car);
    nothers = r->nothers;
    color = r->color;
    field = r->field_size;
    thunder = r->thunder;
    if (!course[0]) {
        fprintf(stderr, "custom modes: fail retry: course unavailable\n");
        snprintf(msg, len, "custom race course unavailable");
        return 0;
    }
    for (i = 0; i < nothers && i < MOON_MAX_CARS - 1; i++) {
        snprintf(others[i], sizeof others[i], "%s", r->others[i]);
        other_ptr[i] = others[i];
    }
    race_game_set_field_size(field);
    if (!race_game_start(g, course, car, color, other_ptr, NULL, nothers, msg, len))
        return 0;
    race_game_set_elimination(g, 1, thunder);
    fprintf(stderr, "custom modes: fail retry race start course=%s field=%d\n", course, field);
    return 1;
}

void race_game_fail_close(race_game *g)
{
    if (!g || !g->cfail)
        return;
    cfail_close(g->cfail);
    g->cfail = NULL;
}
