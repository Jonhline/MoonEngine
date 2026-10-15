/* the display's car icon (RaceDisplay +0x2D14): func_003A6678 draws the
   car's 96x64 picture (func_004997B0 2, 0x60, 0x40), CarIconMaker's first
   per car (00393B70: 00393D20 mode 0, in perspective from the front
   three-quarter), not the results' 64x32 side view (mode 1) */
static void hud_icon(void *user, int car, float x, float y, float w, float h)
{
    race_game *g = user;
    race_view_draw_icon_front(g->view, car, x, y, w, h);
}

static void draw_icon(void *user, int car, float x, float y, float w, float h,
                      const uint8_t color[4])
{
    race_game *g = user;
    (void)color;
    race_view_draw_icon(g->view, car, x, y, w, h);
}

static void draw_license_screen(void *user, float x, float y, float w, float h)
{
    race_game *g = user;
    int32_t state;
    uint32_t time;
    if (!race_game_license_result(g, &state, &time) || state > 0 || !g->license_screen)
        return;
    /* 0033F2E8 consumes the previous callback's Scene parameters;
       003E0E38 then multiplies Result+568 by .98 and publishes for next
       frame. LUT is cached by the GPU; no readback or image allocation. */
    int options[4];
    float parameters[6];
    uint8_t mix[3][3];
    race_pause_display_options(options);
    gt4_license_image_options(options, parameters);
    if (!g->license_image_valid || memcmp(options, g->license_image_options, sizeof options)) {
        gt4_license_image_lut(parameters, g->license_image_lut);
        memcpy(g->license_image_options, options, sizeof options);
        g->license_image_valid = 1;
    }
    gt4_license_image_mix(parameters[5] * g->license_saturation, mix);
    if (g->license_image_active)
        video_license_image(g->license_screen, x, y, w, h, g->license_image_lut, mix);
    else {
        const uint8_t white[4] = {255, 255, 255, 255};
        plat_draw_quad(g->license_screen, x, y, w, h, 0, 1, 1, 0, white);
    }
    g->license_image_active = 1;
    g->license_saturation = gt4_license_image_decay(g->license_saturation);
}

#include "race_game_result_assets.inc"

/* the pause menu over the race or the replay */
static void draw_pause(race_game *g, double dt)
{
    if (!g->pause)
        return;
    race_pause_update(g->pause, dt);
    race_pause_draw(g->pause);
    int state = race_pause_state(g->pause);
    if (state == RACE_PAUSE_EXIT)
        g->quit = 1;
    if (state != RACE_PAUSE_OPEN) {
        race_pause_close(g->pause);
        g->pause = NULL;
        race_audio_pause(g->audio, 0);
    }
    if (state == RACE_PAUSE_RETRY) {
        g->retry_requested = 1; /* app owns the loading transition, as on Start */
    }
}

/* over the replay: the plate a while after a button, the hold */
static void replay_overlay(race_game *g)
{
    if (g->label && g->label_v > 0) {
        race_hud_draw_replay(g->hud, g->label, g->label_v, 0);
    }
    if (g->held)
        race_hud_draw_simple_pause(g->hud);
}

#include "race_game_results.inc"

static void split_hud(race_game *g, double dt)
{
    for (int i = 0; i < 2; i++) {
        race_hud **h = i ? &g->hud_2p : &g->hud;
        if (!*h) {
            *h = race_hud_open(g->vol);
            race_hud_set_course(*h, g->last_course);
        }
        gt4_race_hud r;
        race_hud_state s = {0};
        if (!*h || !gt4_race_run_hud(g->run, i, &r))
            continue;
        s.rank = r.rank;
        s.cars = 2;
        s.lap = r.lap;
        s.laps = r.laps;
        s.lap_time = r.lap_time;
        s.best_lap = r.best_lap;
        s.last_lap = r.last_lap;
        s.speed = r.speed;
        s.rpm = r.rpm;
        s.gear = r.gear;
        s.throttle = r.throttle;
        s.brake = r.brake;
        s.replay = g->replaying;
        s.side_g = r.side_g;
        s.shift_lamp = r.shift_lamp;
        s.fuel = r.fuel;
        s.fuel_cap = r.fuel_cap;
        s.rev_limit = r.rev_limit;
        s.aid = r.aid;
        s.brake_lock = r.brake_lock;
        s.driving = r.driving;
        s.start_frames = r.start_frames;
        s.count_frames = r.count_frames;
        s.rolling = r.rolling;
        s.start_seconds = r.start_seconds;
        s.intro_elapsed = r.intro_elapsed;
        s.finished = r.finished;
        s.speed_scale = (float)(r.speed_steps * (g->unit == 1 ? 50 : 80));
        s.tacho_scale = r.tacho_scale;
        s.odometer = r.odometer;
        s.unit = g->unit;
        s.signal = r.signal;
        /* 003A2360 mode 8: dedicated horizontal Battle2P layout. */
        s.split = 1;
        s.player = i;
        s.panel = 0;
        for (int j = 0; j < 2; j++)
            s.map_show[j] = gt4_race_run_xy(g->run, j, &s.map_x[j], &s.map_y[j]);
        plat_video_set_view(640, 448);
        video_ui_panel(i * 224.f, 224);
        race_hud_draw(*h, &s, g->pause ? 0.f : (float)dt);
        video_ui_panel(0, 0);
    }
    plat_video_set_view(640, 448);
    /* Keep each driver's view until both original finish flags are set.
       A: Battle2P result activation ordering still needs a world oracle. */
    if (!g->replaying && gt4_race_run_finished(g->run) && GT4_U8(player_2p + 0x5b6))
        draw_result(g, 2, dt);
    draw_pause(g, dt);
}

void race_game_draw_hud(race_game *g, double dt)
{
    gt4_race_hud r;
    race_hud_state s;
    if (!g || !g->ready || !g->run || !g->vol)
        return;
    /* Original: func_0033F2E8 +0x458..0x49C: scene correction precedes HUD.
       ResultLicense supplies its own correction/saturation through screen;
       do not correct its capture twice. */
    int32_t license_state;
    uint32_t license_time;
    int license_failure = !g->replaying && g->finished >= 0 &&
                          race_game_license_result(g, &license_state, &license_time) &&
                          license_state <= 0;
    if (!license_failure) {
        int options[4];
        race_pause_display_options_for(g->replaying, options);
        screen_setting_draw(&g->screen_setting, options);
    }
    if (g->cinema.on && g->cinema.clean) {
        if (g->pause)
            draw_pause(g, dt);
        return;
    }
    if (g->split) {
        split_hud(g, dt);
        return;
    }
    g->pad = plat_sdl_pad(0);
    if (getenv("OPENGT4_RACE_SNAPSHOT"))
        return; /* tests: the captured scene alone */
    if (getenv("OPENGT4_RACE_FILM"))
        return; /* P: clean filming frame */
    /* the replay: the display off as replay_display_enable's default
       (P: what it shows when on is not read) */
    if (!g->hud) {
        g->hud = race_hud_open(g->vol);
        race_hud_set_icon(g->hud, hud_icon, g);
    }
    {
        char title[96], artist[96];
        if (race_audio_music_title(g->audio, title, sizeof title, artist, sizeof artist)) {
            static char last[96];
            race_hud_music(g->hud, title, artist);
            if (getenv("OPENGT4_SOUND_TRACE") && strcmp(last, title)) {
                fprintf(stderr, "race music title: %s / %s\n", title, artist);
                snprintf(last, sizeof last, "%s", title);
            }
        } else
            race_hud_music(g->hud, "", ""); /* no racing track yet in this race */
    }
    if (g->hud_course[0]) { /* the course's map and logo (the replay's opening shows the logo) */
        race_hud_set_course(g->hud, g->hud_course);
        g->hud_course[0] = 0;
    }
    if (g->replaying && g->rmode != 2) { /* Normal and Dive: no display, the logo */
        race_hud_draw_replay(g->hud, NULL, 0, 1);
        race_hud_draw_music(g->hud, g->held ? 0.f : (float)dt);
        replay_opening(g);
        replay_overlay(g);
        plat_video_set_view(
            640, 448); /* the frame's 448 lines, as race_hud_draw leaves it: no black band below */
        return;
    }
    if (!g->hud) {
        g->hud = race_hud_open(g->vol);
        race_hud_set_icon(g->hud, hud_icon, g);
    }
    if (g->hud_course[0]) {
        race_hud_set_course(g->hud, g->hud_course);
        g->hud_course[0] = 0;
    }
    if (!gt4_race_run_hud(g->run, watched_car(g), &r))
        return;
    memset(&s, 0, sizeof s);
    s.rank = r.rank;
    s.cars = gt4_race_run_cars(g->run);
    s.lap = r.lap;
    s.laps = r.laps;
    s.lap_time = r.lap_time;
    s.best_lap = r.best_lap;
    s.last_lap = r.last_lap;
    s.speed = r.speed;
    s.rpm = r.rpm;
    s.gear = r.gear;
    s.throttle = r.throttle;
    s.brake = r.brake;
    s.replay = g->replaying;
    s.side_g = r.side_g;
    s.shift_lamp = r.shift_lamp;
    s.fuel = r.fuel;
    s.fuel_cap = r.fuel_cap;
    { /* 0039E790..0039E7C8: meter follows service state, not camera. */
        uint32_t car = gt4_race_run_car(g->run, watched_car(g));
        s.refueling = gt4_pit_refueling(GT4_U16(car + CAR_FIELD_UNK_5BC), GT4_U16(car + CAR_5BE));
    }
    s.rev_limit = r.rev_limit;
    s.aid = r.aid;
    s.brake_lock = r.brake_lock;
    s.driving = r.driving;
    s.start_frames = r.start_frames;
    s.count_frames = r.count_frames;
    s.rolling = r.rolling;
    s.start_seconds = r.start_seconds;
    s.intro_elapsed = r.intro_elapsed;
    s.aspec_points = r.aspec_points;
    s.finished = r.finished;
    if (g->last_prepared && license_exam(g)) {
        /* RaceDisplayLicense 003BD150/003BD4F0: reference times and
           contact count belong to the examination conductor. */
        uint32_t cond = gt4_race_run_conductor(g->run);
        s.license = 1;
        for (int i = 0; i < 3; i++)
            s.license_times[i] = gt4_career_license_time(g->last_event_row, i);
        s.license_pylon_limit = GT4_U8(cond + CONDUCTOR_FIELD_UNK_F85E);
        s.license_pylons = (int16_t)GT4_U16(cond + CONDUCTOR_FIELD_UNK_1003C);
        s.license_wait = gt4_license_wait(cond, 0);
        int shown = 0;
        for (int i = 0; i < s.cars && i < 6; i++) {
            float x, y;
            if (gt4_race_run_xy(g->run, i, &x, &y) == 3)
                shown++;
        }
        s.license_rank = shown >= 2;
    }
    s.speed_scale = (float)(r.speed_steps * (g->unit == 1 ? 50 : 80));
    s.tacho_scale = r.tacho_scale;
    s.odometer = r.odometer;
    for (int i = 0; i < s.cars && i < 6; i++)
        s.map_show[i] = gt4_race_run_xy(g->run, i, &s.map_x[i], &s.map_y[i]);
    s.unit = g->unit;
    s.signal = r.signal;
    { /* func_0035FC80 reads the car's +0x712 while it has not finished */
        uint32_t car = gt4_race_run_car(g->run, 0);
        s.penalty = car && !r.finished ? (int)GT4_U16(car + CAR_FIELD_UNK_712) : 0;
    }
    s.panel = g->camera != 1 || g->start_cam_on ||
              g->tv_on; /* the onboard panel with the views from the car (and the start's) */
    if (g->replaying)
        s.panel =
            g->car_cam; /* the replay: the simple panel under the TV cameras, the gauges on the car (rv_02, rv_05) */
    if (g->bspec_ui) { /* B-Spec's display (003A2360 cases 2/3, the panel's +0x18): no gauges, the race's time, the tyres */
        uint32_t cond = gt4_race_run_conductor(g->run),
                 car = gt4_race_run_car(g->run, watched_car(g));
        s.panel = 0;
        s.bspec = 1;
        s.total_time = GT4_U32(cond + CONDUCTOR_FIELD_RACE_CLOCK_U) / 3; /* func_00358440 */
        /* 003D80A8: each tyre's colour by the car's wheel layout (00359538:
           D_00620478 + params+0x30 * 0x54, kind 0..3) from its wheel's
           heat (003446D8: car + wheel * 0xEC + 0x215) through 003677F8 */
        int kind = GT4_U8(0x00620478u + GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x30) * 0x54u);
        /* 003D80A8 +0x10..0x58: black unless the race wears its tyres
           (00367D40: cond+4 +0x68 or +0x69) and the player's car is watched */
        int shown = (GT4_U8(cond + CONDUCTOR_FIELD_TYRE_STATE + 0x68) ||
                     GT4_U8(cond + CONDUCTOR_FIELD_TYRE_STATE + 0x69)) &&
                    watched_car(g) == 0;
        for (int i = 0; i < 4; i++) {
            if (!shown) {
                memset(s.tire_rgb[i], 0, 3);
                continue;
            }
            int k = kind == 1   ? (i < 2 ? 0 : i - 1)
                    : kind == 2 ? (i < 3 ? i : 2)
                    : kind == 3 ? i >> 1
                                : i;
            int t = (int8_t)GT4_U8(car + (uint32_t)k * 0xec + 0x215);
            uint8_t r = 0, gg = 0, b = 0;
            if (t >= 0x40) {
                r = 0xfc;
                gg = (uint8_t)((0x7f - t) * 2);
            } else if (t >= 0x20) {
                r = 0xfc;
                gg = (uint8_t)((0x5f - t) * 4);
            } else if (t >= 0) {
                r = (uint8_t)(t * 8);
                gg = 0xfc;
            } else if (t >= -0x3f) {
                b = (uint8_t)(-t * 4);
                gg = (uint8_t)(0xfc + t * 4);
            }
            s.tire_rgb[i][0] = r;
            s.tire_rgb[i][1] = gg;
            s.tire_rgb[i][2] = b;
        }
    }
    int license_failed = 0;
    if (s.license && r.finished) {
        int32_t state;
        uint32_t time;
        license_failed = race_game_license_result(g, &state, &time) && state <= 0;
    }
    if (license_failed) {
        /* 003E0E38 "screen" owns the race viewport during failure.
           Preserve this frame before the HUD; other result primitives
           then draw over a black canvas, as the slot 03 reference. */
        g->license_screen = video_grab(g->license_screen, 0, 0, 640, 448);
        const uint8_t black[4] = {0, 0, 0, 255};
        plat_video_set_view(640, 480);
        plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
        plat_video_set_view(640, 448);
    } else if (g->bspec_ui && race_bspec_mode(g->bspec_ui) == 1) {
        /* B-Spec's Race Monitor over the whole screen (003DA150: the race
           not drawn under it, +0x38) */
        const uint8_t black[4] = {0, 0, 0, 255};
        plat_video_set_view(640, 480);
        plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
        g->bspec_hud = &s;
        race_bspec_draw(g->bspec_ui);
        g->bspec_hud = NULL;
        race_hud_draw_tooltip(g->hud, race_bspec_tooltip(g->bspec_ui),
                              race_bspec_light(g->bspec_ui));
        plat_video_set_view(640, 448);
    } else {
        race_hud_draw(g->hud, &s, g->pause ? 0.f : (float)dt); /* held with the race */
        if (g->bspec_ui) {
            g->bspec_hud = &s;
            race_bspec_draw(g->bspec_ui);
            g->bspec_hud = NULL;
            race_hud_draw_tooltip(g->hud, race_bspec_tooltip(g->bspec_ui),
                                  race_bspec_light(g->bspec_ui));
            plat_video_set_view(640, 448);
        }
    }
    /* the results, from the player's finish (ResultArcade's movie) */
    if (r.finished && !g->replaying && g->mission_presentation < GT4_MISSION_RETURN)
        draw_result(g, s.cars, dt);
    if (g->replaying) {
        replay_opening(g);
        replay_overlay(g);
        plat_video_set_view(640, 448);
    } else {
        race_pitmenu_draw(g->pitmenu); /* 003DD690, under the pause */
        draw_pause(g, dt);
    }
}
