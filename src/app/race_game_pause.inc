/* the pause menu (RacePause): Escape or the pad's Start holds the race;
   the arrows and Enter (the pad's D-pad and A) go to its movie. A: the
   game's own pause button handling (Start again, Triangle) is not read;
   Escape / Start in the menu continues. */
static void pause_sound(void *user, const char *name)
{
    race_game *g = user;
    race_audio_menu_sound(g->audio, name, g->se_volume);
}
static void pause_toggle(race_game *g)
{
    if (g->replaying) {
        g->held = !g->held;
        return;
    } /* the replay: its own hold */
    if (g->pause) {
        race_pause_close(g->pause);
        g->pause = NULL;
        race_audio_pause(g->audio, 0);
        return;
    }
    if ((!g->split && g->finished >= 0) || g->result)
        return;
    /* the Arcade's RacePause as every capture holds it: +0x28 retry 0,
       +0x30 screen setting 1, +0x2C confirm 0. The replay's the same
       (A: no capture of a replay's; func_003C2D80 makes every pause with
       retry 0, and only the race worlds set it) */
    /* 003BBF90 +0x110 sets License RacePause +0x28 to 1. The movie
       remains common/pause.strb (003C31C8, 006A2A60), not pause_license. */
    /* 003EDE50 / 003EF498 (the GT Mode event and championship worlds) set
       +0x2C to 1 unless D_00620208 (the demonstration, 0010CA50) is set:
       Exit asks to confirm. */
    int confirm =
        (!g->split && g->last_prepared && g->last_desc.event_row && !g->last_desc.license);
    g->pause = race_pause_open(g->vol, g->split || (g->last_prepared && license_exam(g)), !g->split,
                               confirm, pause_sound, g);
    if (g->pause) {
        race_audio_pause(g->audio, 1); /* 0038B428: the world's vt+0x2D0 */
        plat_input_set_ffb(0, 0);
        plat_input_set_ffb(1, 0);
    }
}

/* the replay's buttons: 0 Square, 1 Circle, 2 Cross, 3 L3, 4 up, 5 down, 6 Triangle */
static void replay_button(race_game *g, int b)
{
    static const char *const modes[3] = {"ReplayModeNormal", "ReplayModeDive",
                                         "ReplayModeNormalDisplay"};
    int cars = gt4_race_run_cars(g->run);
    switch (b) {
    case 0:
        g->rmode = (g->rmode + 1) % 3;
        g->label = modes[g->rmode];
        break;
    case 1:
        g->bgm_off = !g->bgm_off;
        g->label = g->bgm_off ? "ReplayModeBGMOff" : "ReplayModeBGMOn";
        break;
    case 2:
        g->car_cam = !g->car_cam;
        if (g->tvcam)
            gt4_tvcam_reset(g->tvcam);
        return;
    case 3:
        g->fast = !g->fast;
        g->label = g->fast ? "ReplayModeAccelerate" : "ReplayModeNormalSpeed";
        break;
    case 6:
        g->zoom_on = !g->zoom_on;
        return;
    case 4:
    case 5:
        if (cars > 0)
            g->focus = (g->focus + (b == 4 ? cars - 1 : 1)) % cars;
        if (g->tvcam)
            gt4_tvcam_reset(g->tvcam);
        return;
    default:
        return;
    }
    g->label_t = 2.75; /* func_003A98E8: at once, 2.75 s, in and out in a quarter (rv_01..06) */
    g->label_out = 0;
}

void race_game_key(race_game *g, int key)
{
    if (!g || !g->ready)
        return;
    if (g->result && g->last_prepared && license_exam(g)) {
        int code = key == SDLK_LEFT     ? 37
                   : key == SDLK_UP     ? 38
                   : key == SDLK_RIGHT  ? 39
                   : key == SDLK_DOWN   ? 40
                   : key == SDLK_ESCAPE ? 27
                                        : 0;
        if (code)
            race_result_key(g->result, code);
        return;
    }
    if (g->replaying) { /* the replay: Select (Backspace, P) holds it, Start (Escape) leaves */
        if (key == SDLK_BACKSPACE || key == SDLK_p)
            g->held = !g->held;
        else if (key == SDLK_ESCAPE)
            g->quit = 1;
        else
            replay_button(g, key == SDLK_d                                ? 0
                             : key == SDLK_m                              ? 1
                             : key == SDLK_RETURN || key == SDLK_KP_ENTER ? 2
                             : key == SDLK_f                              ? 3
                             : key == SDLK_UP                             ? 4
                             : key == SDLK_DOWN                           ? 5
                             : key == SDLK_z                              ? 6
                                                                          : -1);
        return;
    }
    if (key == SDLK_ESCAPE) {
        pause_toggle(g);
        return;
    }
    if (!g->pause && race_pitmenu_up(g->pitmenu)) {
        int c = key == SDLK_LEFT                             ? 37
                : key == SDLK_UP                             ? 38
                : key == SDLK_RIGHT                          ? 39
                : key == SDLK_DOWN                           ? 40
                : key == SDLK_RETURN || key == SDLK_KP_ENTER ? 13
                                                             : 0;
        if (c) {
            race_pitmenu_key(g->pitmenu, c);
            g->pit_touched |= 1;
        }
        return;
    }
    if (!g->pause)
        return;
    int code = key == SDLK_LEFT                             ? 37
               : key == SDLK_UP                             ? 38
               : key == SDLK_RIGHT                          ? 39
               : key == SDLK_DOWN                           ? 40
               : key == SDLK_RETURN || key == SDLK_KP_ENTER ? 13
                                                            : 0;
    if (code)
        race_pause_key(g->pause, code);
}
static void pause_input(race_game *g)
{
    if (g->split) {
        plat_pad_state p;
        int connected = plat_input_poll(1, &p);
        static const int buttons[7] = {GT4_PAD_START, GT4_PAD_UP, GT4_PAD_DOWN,
                                       GT4_PAD_LEFT, GT4_PAD_RIGHT, GT4_PAD_CROSS, GT4_PAD_CIRCLE};
        static const int codes[7] = {0, 38, 40, 37, 39, 13, 13};
        for (int k = 0; k < 7; k++) {
            int down = connected && (p.buttons & (1u << buttons[k]));
            if (down && !g->pause_keys_2p[k]) {
                if (k == 0)
                    pause_toggle(g);
                else if (g->pause)
                    race_pause_key(g->pause, codes[k]);
            }
            g->pause_keys_2p[k] = down;
        }
    }
    static const SDL_GameControllerButton button[7] = {
        SDL_CONTROLLER_BUTTON_START,      SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,  SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B};
    static const int code[7] = {0, 38, 40, 37, 39, 13, 13};
    g->pad = plat_sdl_pad(0);
    SDL_JoystickID id = g->pad ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g->pad)) : -1;
    if (!g->pad || id != g->pad_id) {
        memset(g->pause_keys, 0, sizeof g->pause_keys);
        memset(g->pad_held, 0, sizeof g->pad_held);
        g->replay_back_held = 0;
        g->pad_id = id;
    }
    if (!g->pad)
        return;
    if (g->replaying) {
        int down = SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_BACK);
        if (down && !g->replay_back_held)
            g->held = !g->held;
        g->replay_back_held = down;
        if (SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_START) && !g->pause_keys[0])
            g->quit = 1;
        g->pause_keys[0] = SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_START);
        { /* Square, Circle, Cross, L3, up, down, Triangle */
            static const SDL_GameControllerButton b[7] = {
                SDL_CONTROLLER_BUTTON_X,       SDL_CONTROLLER_BUTTON_B,
                SDL_CONTROLLER_BUTTON_A,       SDL_CONTROLLER_BUTTON_LEFTSTICK,
                SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                SDL_CONTROLLER_BUTTON_Y};
            for (int i = 0; i < 7; i++) {
                int down = SDL_GameControllerGetButton(g->pad, b[i]);
                if (down && !g->pad_held[i])
                    replay_button(g, i);
                g->pad_held[i] = down;
            }
        }
        return;
    }
    for (int i = 0; i < 7; i++) {
        int down = SDL_GameControllerGetButton(g->pad, button[i]);
        if (down && !g->pause_keys[i]) {
            /* 003DF978 +0xd4 -> 003EBF40: the licence result receives
               menu input too. main.c leaves pad edges to this poller;
               without this route only keyboard keys reached Retry/Exit.
               Start remains the existing draw_result confirm shortcut. */
            if (g->result && g->last_prepared && license_exam(g)) {
                if (i && i != 6)
                    race_result_key(g->result, code[i]);
            } else if (i == 0)
                pause_toggle(g);
            else if (g->pause)
                race_pause_key(g->pause, code[i]);
            else if (i != 6 && race_pitmenu_up(g->pitmenu)) {
                race_pitmenu_key(g->pitmenu, code[i]);
                g->pit_touched |= 1;
            }
        }
        g->pause_keys[i] = down;
    }
}
int race_game_quit(const race_game *g)
{
    return g && g->quit;
}
