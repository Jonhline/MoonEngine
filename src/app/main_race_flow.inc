/* References to main's live race/menu state; this does not own resources. */
typedef struct {
    race_game *game;
    race_view *race;
    menu **mn;
    menu **cache;
    int *proj;
    int *driving;
    int *race_start_pending;
    int *last_root;
    int *race_loading;
    int *race_active;
    int *race_return;
    int *theater;
    double *attract_seconds;
    int *attract_cancel;
    const char *race_course;
    const char *race_car;
    const char *shot;
    int *exit_code;
} app_race_flow;

static int handle_race_retry_and_replay_return(app_race_flow *flow)
{
    if ((*flow->driving) == 2 && race_game_take_retry(flow->game)) {
        char error[256];
        if (!race_game_restart(flow->game, error, sizeof error)) {
            fprintf(stderr, "race retry: %s\n", error);
            (*flow->exit_code) = 1;
            return 0;
        }
        career_license.result_applied = 0;
        career_race.result_applied = 0;
        loading_screen_begin();
        (*flow->driving) = 1;
        (*flow->race_start_pending) = 1;
    }
    /* out of the race when the pause menu chose EXIT: back to the
           quick menu (its grid, race again or leave) */
    if ((*flow->driving) == 2 && race_game_demonstrating(flow->game) &&
        (race_game_quit(flow->game) || race_game_over(flow->game))) {
        /* 0011C518 / 0011D020: teacher playback never enters the
               attempt-result/award path. Keep the visit's best and icons. */
        char msg[256];
        loading_screen_begin();
        if (!race_game_end_demo(flow->game, msg, sizeof msg)) {
            fprintf(stderr, "demonstration return: %s\n", msg);
            (*flow->exit_code) = 1;
            return 0;
        }
        (*flow->driving) = 1;
        (*flow->race_start_pending) = 0;
        (*flow->last_root) = -1;
        fprintf(stderr, "demonstration: returned to examination menu\n");
        menu_enter_quick_start((*flow->mn));
        menu_tick((*flow->mn));
        presentation_clock = plat_time_ns();
    }
    if ((*flow->theater) && (*flow->driving) == 2 &&
        (race_game_quit(flow->game) || race_game_over(flow->game) ||
         ((*flow->theater) == 2 && (*flow->attract_seconds) >= 90))) {
        /* the saved replay over or left: back to the Replay Theater (a
               playlist goes on to its next one unless the replay was left) */
        if ((*flow->theater) == 1 && race_game_quit(flow->game))
            menu_stop_replay_queue();
        if ((*flow->theater) == 2)
            menu_attract_finish((*flow->mn), (*flow->attract_cancel) || race_game_quit(flow->game));
        loading_screen_begin();
        present_wait((*flow->mn), menu_loading_duration((*flow->mn)), NULL);
        race_game_stop(flow->game);
        (*flow->driving) = 0;
        (*flow->theater) = 0;
        (*flow->last_root) = -1;
        if ((*flow->mn)) {
            menu_enter((*flow->mn),
                       menu_active_root((*flow->mn)) < 0 ? 0 : menu_active_root((*flow->mn)));
            menu_tick((*flow->mn));
            ui_fade(menu_ui((*flow->mn)), 1, 0, .2f);
        }
        loading_screen_end();
        plat_video_set_view(640, 480);
        presentation_clock = plat_time_ns();
    }

    return 1;
}

static int return_race_to_menu(app_race_flow *flow)
{
    if ((*flow->driving) == 2 && (race_game_quit(flow->game) ||
                                  (race_game_split(flow->game) && race_game_over(flow->game)))) {
        if (!career_race_result(flow->game, flow->cache[1], (*flow->mn))) {
            fprintf(stderr, "career result could not be recorded\n");
            (*flow->exit_code) = 1;
            return 0;
        }
        loading_screen_begin();
        present_wait((*flow->mn), menu_loading_duration((*flow->mn)), NULL);
        if (race_quick_project((*flow->proj))) {
            session_best_update(flow->game, (*flow->mn), flow->race_course, flow->race_car);
            menu_set_quick_completed((*flow->mn), 1);
            race_game_return_pregrid(flow->game);
            (*flow->driving) = 4;
            pregrid_clock = 0;
        } else {
            int split = race_game_split(flow->game);
            if (split && !race_game_replaying(flow->game) && !race_game_quit(flow->game) &&
                race_game_can_replay(flow->game)) {
                /* Battle2P uses both command recorders; common result -> replay.
                       A: activation ordering still requires an original world capture. */
                char msg[256];
                if (race_game_replay(flow->game, msg, sizeof msg)) {
                    (*flow->driving) = 1;
                    (*flow->race_start_pending) = 1;
                } else {
                    fprintf(stderr, "2P replay: %s\n", msg);
                    race_game_stop(flow->game);
                    (*flow->driving) = 0;
                }
            } else {
                race_game_stop(flow->game);
                (*flow->driving) = 0;
            }
            if (split && (*flow->mn) && !(*flow->driving)) {
                menu_enter((*flow->mn),
                           menu_active_root((*flow->mn)) < 0 ? 0 : menu_active_root((*flow->mn)));
                menu_tick((*flow->mn));
                ui_fade(menu_ui((*flow->mn)), 1, 0, .2f);
            }
        }
        (*flow->last_root) = -1;
        if ((*flow->mn) && race_quick_project((*flow->proj))) {
            menu_enter_quick_start((*flow->mn));
            menu_tick((*flow->mn));
            ui_fade(menu_ui((*flow->mn)), 1, 0, .2f);
        }
        loading_screen_end();
        plat_video_set_view(640, 480);
        presentation_clock = plat_time_ns();
    }
    return 1;
}

/* Custom Modes: the player's elimination is shown as one thunder strike —
   the game's own thunder plays out on screen and in the ear, then the picture
   and the sound cut to black at once, hold, and fade back in. */
#define STRIKE_THUNDER_S 2.000
#define STRIKE_HOLD_S .700
#define STRIKE_FADE_IN_S 1.400
#define STRIKE_TOTAL_S (STRIKE_THUNDER_S + STRIKE_HOLD_S + STRIKE_FADE_IN_S)
static struct {
    int active, cut_done, thunder;
    unsigned serial;
    double t;
} app_strike_state;

static int app_strike_cut(race_game *game, menu **mn, int *driving, int *race_start_pending,
                          int *last_root)
{
    if (!game || *driving != 2)
        return 0;
    race_game_stop(game);
    *driving = 0;
    *race_start_pending = 0;
    *last_root = -1;
    plat_video_set_view(640, 480);
    /* The mode's own entry faded the page to black; bring it back so the
       fade-in reveals the same Custom Modes page, not a dark one. */
    if (mn && *mn)
        ui_fade(menu_ui(*mn), 1, 0, 0);
    presentation_clock = plat_time_ns();
    return 1;
}

static void app_strike_poll(race_game *game, menu **mn, double dt, int *driving,
                            int *race_start_pending, int *last_root)
{
    if (!app_strike_state.active) {
        int thunder = 0;
        unsigned serial = *driving == 2 ? race_game_take_strike(game, &thunder) : 0;
        if (!serial)
            return;
        app_strike_state.serial = serial;
        app_strike_state.thunder = thunder;
        if (!thunder) {
            /* flag off: no flash, no bolt, no sound; the cut still happens. */
            app_strike_cut(game, mn, driving, race_start_pending, last_root);
            if (game)
                race_game_set_strike_visual(game, -1, 0);
            fprintf(stderr, "custom modes: strike serial=%u cut\n", serial);
            return;
        }
        app_strike_state.active = 1;
        app_strike_state.cut_done = 0;
        app_strike_state.t = 0;
        race_game_mute_music(game, 1);
        fprintf(stderr, "custom modes: strike serial=%u start\n", serial);
    }
    if (game)
        race_game_set_strike_visual(game, app_strike_state.active ? app_strike_state.t : -1.0,
                                    app_strike_state.serial);
    /* A frame stall must not skip the presentation: the sequence advances
       at most one normal display step per frame. */
    if (dt > .05)
        dt = .05;
    app_strike_state.t += dt;
    {
        double t = app_strike_state.t;
        if (!app_strike_state.cut_done && t >= STRIKE_THUNDER_S) {
            if (app_strike_cut(game, mn, driving, race_start_pending, last_root))
                fprintf(stderr, "custom modes: strike serial=%u black\n",
                        app_strike_state.serial);
            app_strike_state.cut_done = 1;
        }
        if (t >= STRIKE_TOTAL_S) {
            app_strike_state.active = 0;
            if (game)
                race_game_set_strike_visual(game, -1, 0);
            fprintf(stderr, "custom modes: strike serial=%u menu\n", app_strike_state.serial);
        }
    }
}

static void app_fail_poll(race_game *game, menu **mn, int *driving, int *race_start_pending,
                          int *last_root)
{
    int retry;
    if (!game || *driving != 2)
        return;
    race_game_take_player_fail(game);
    if (!race_game_fail_active(game) || !race_game_fail_over(game))
        return;
    retry = race_game_fail_retry(game);
    fprintf(stderr, "custom modes: fail screen close retry=%d\n", retry);
    if (retry) {
        char msg[256];
        if (race_game_fail_retry_race(game, msg, sizeof msg)) {
            loading_screen_begin();
            *driving = 1;
            *race_start_pending = 1;
            *last_root = -1;
        } else
            fprintf(stderr, "custom modes: fail retry: %s\n", msg);
    } else if (app_strike_cut(game, mn, driving, race_start_pending, last_root))
        fprintf(stderr, "custom modes: fail exit to custom modes\n");
}

static void app_strike_draw(void)
{
    uint8_t v[4] = {0, 0, 0, 255};
    float h;
    double t;
    if (!app_strike_state.active)
        return;
    t = app_strike_state.t;
    /* The bolt itself is world geometry in the race pass. Here only the
       picture after it: nothing over the strike, then black at once (no
       ramp), hold, and one clean fade back to the page. */
    if (t < STRIKE_THUNDER_S)
        return;
    h = video_view_height();
    if (t < STRIKE_THUNDER_S + STRIKE_HOLD_S) {
        plat_draw_quad(NULL, 0, 0, 640, h, 0, 0, 1, 1, v);
        return;
    }
    {
        float u = (float)((t - STRIKE_THUNDER_S - STRIKE_HOLD_S) / STRIKE_FADE_IN_S);
        if (u < 0.f)
            u = 0.f;
        if (u > 1.f)
            u = 1.f;
        u = u * u * (3.f - 2.f * u); /* smoothstep: the page comes up */
        plat_draw_quad_blend(NULL, 0, 0, 640, h, 0, 0, 1, 1, v, 1.f - u);
    }
}

static int poll_race_loading(app_race_flow *flow, double *elapsed)
{
    if ((*flow->driving) == 1 && (!loading_screen_active() || loading_screen_ready())) {
        int ready = race_game_poll(flow->game);
        if (ready > 0) {
            if ((*flow->race_start_pending)) {
                (*flow->driving) = 2;
                (*flow->race_start_pending) = 0;
                plat_video_set_view(640, 448);
            } else
                (*flow->driving) = 3;
        } else if (ready < 0) {
            if ((*flow->theater) == 2) {
                menu_attract_finish((*flow->mn), 1);
                (*flow->theater) = 0;
            }
            (*flow->driving) = 0;
            fprintf(stderr, "cannot load course %s\n", flow->race_course);
            present_dialog((*flow->mn), "Info", "Nao foi possivel carregar esta pista. Veja o log.",
                           0, NULL, NULL);
        }
        if (ready) {
            loading_screen_end();
            plat_video_set_view(640, (*flow->driving) == 2 ? 448 : 480);
            (*flow->last_root) = -1;
            (*elapsed) = 0;
            presentation_clock = plat_time_ns();
        }
    }
    if ((*flow->race_loading) && (!loading_screen_active() || loading_screen_ready())) {
        int ready = race_view_poll(flow->race, (*flow->race_loading) < 0);
        if (ready) {
            loading_screen_end();
            plat_video_set_view(640, ready > 0 && (*flow->race_loading) > 0 ? 448 : 480);
            if (ready > 0 && (*flow->race_loading) > 0) {
                (*flow->race_active) = 1;
                plat_video_set_view(640, 448);
            } else if (ready < 0 && (*flow->race_loading) > 0) {
                fprintf(stderr, "cannot load course %s\n", flow->race_course);
                if (flow->shot) {
                    (*flow->exit_code) = 1;
                    return 0;
                }
                present_dialog((*flow->mn), "Info",
                               "Nao foi possivel carregar esta pista. Veja o log.", 0, NULL, NULL);
            }
            (*flow->race_loading) = 0;
            (*flow->last_root) = -1;
            (*elapsed) = 0;
            presentation_clock = plat_time_ns();
        }
    }

    return 1;
}

static int update_and_draw_race(app_race_flow *flow, double elapsed, int *car_ready)
{

    { /* the replay has its own volumes (replay_bgm/se_volume_raw) */
        int rp = race_game_replaying(flow->game);
        race_game_set_audio(
            flow->game,
            menu_option_number((*flow->mn), rp ? "replay_bgm_volume_raw" : "race_bgm_volume_raw",
                               120),
            menu_option_number((*flow->mn), rp ? "replay_bgm" : "race_bgm", 1),
            menu_option_number((*flow->mn), rp ? "replay_se_volume_raw" : "race_se_volume_raw",
                               120));
    }
    if (!bench_freeze)
        race_game_update(flow->game, elapsed);
    (*car_ready) = race_game_draw(flow->game);
    if ((*flow->theater) == 2)
        (*flow->attract_seconds) += elapsed; /* 00434590 +0x10: 90-second demo budget. */
    {
        static int told;
        if (race_game_finished(flow->game) >= 0 && !told) {
            told = 1;
            printf("race: the player finished\n");
        }
    }
    { /* tests: MOON_RACE_REPLAY_AFTER=<seconds> replays the race from then
                   (with MOON_REPLAY_SAVE, saved and read back first) */
        static double raced;
        const char *after = getenv("MOON_RACE_REPLAY_AFTER");
        if (after && !race_game_replaying(flow->game) && (raced += elapsed) >= atof(after)) {
            char msg[256];
            raced = -1e9;
            if (getenv("MOON_REPLAY_SAVE")) {
                gt4_replay_file r, back;
                char name[64];
                if (race_game_replay_export(flow->game, &r, GT4_REPLAY_FULL) &&
                    gt4_replay_store(&r, name, sizeof name) && gt4_replay_load(name, &back)) {
                    fprintf(stderr, "replay: saved %s, %zu steps read back\n", name, back.steps);
                    race_game_replay_import(flow->game, &back);
                    gt4_replay_free(&back);
                } else
                    fprintf(stderr, "replay: save failed\n");
            }
            if (race_game_replay(flow->game, msg, sizeof msg)) {
                loading_screen_begin();
                (*flow->driving) = 1;
                (*flow->race_start_pending) = 1;
            } else
                fprintf(stderr, "replay: %s\n", msg);
        }
    }
    if ((*flow->driving) == 2 && !race_game_demonstrating(flow->game) &&
        race_game_over(flow->game) && race_quick_project((*flow->proj))) {
        if (!career_race_result(flow->game, flow->cache[1], (*flow->mn))) {
            fprintf(stderr, "career result could not be recorded\n");
            (*flow->exit_code) = 1;
            return 0;
        }
        /* the results over (cb_raceexit): back to the quick menu */
        session_best_update(flow->game, (*flow->mn), flow->race_course, flow->race_car);
        (*flow->race_return) = 1;
    }

    return 1;
}
