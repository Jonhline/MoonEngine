/* 0032F028 +0x8C..0x120 / 00426D50: port 1 belongs to entry 1.
   P: WASD/Q/E/C/Left Ctrl are PC bindings; port config is the original's. */
static void read_second_controls(race_game *g, const uint8_t *keys)
{
    plat_pad_state state;
    gt4_pad_config config;
    gt4_pad_actions pad = {0};
    int connected = plat_input_poll(1, &state);
    if (connected) {
        menu_pad_configuration(NULL, 1, &config);
        gt4_pad_evaluate(&config, &state, &pad);
    }
    if (plat_input_is_wheel(1)) {
        pad.steer = -state.axes[0];
        pad.action[GT4_PAD_ACCEL] = state.axes[4];
        pad.action[GT4_PAD_BRAKE] = state.axes[5];
        pad.analog |= (1u << GT4_PAD_LSTEER) | (1u << GT4_PAD_ACCEL) | (1u << GT4_PAD_BRAKE);
    }
    race_command *c = &command_2p;
    memset(c, 0, sizeof *c);
    c->device = plat_input_is_wheel(1) ? 4 : connected ? 2 : 1;
    float steer = (float)(keys[SDL_SCANCODE_A] - keys[SDL_SCANCODE_D]);
    int analog = 0;
    if (steer == 0 && pad.steer != 0) {
        steer = pad.steer;
        analog = !!(pad.analog & (1u << GT4_PAD_LSTEER));
    }
    c->flags = (uint8_t)((analog ? 1 : 0) | (pad.analog & (1u << GT4_PAD_ACCEL) ? 2 : 0) |
                         (pad.analog & (1u << GT4_PAD_BRAKE) ? 4 : 0) |
                         (pad.analog & (1u << GT4_PAD_REVERSE) ? 8 : 0) |
                         (pad.analog & (1u << GT4_PAD_SIDE) ? 16 : 0));
    c->steer = (int16_t)(steer * 4096);
    c->throttle = (int16_t)(fmaxf(keys[SDL_SCANCODE_W] ? 1.f : 0.f, pad.action[GT4_PAD_ACCEL]) *
                            (c->flags & 2 ? 4096 : 1));
    c->brake = (int16_t)(fmaxf(keys[SDL_SCANCODE_S] ? 1.f : 0.f, pad.action[GT4_PAD_BRAKE]) *
                         (c->flags & 4 ? 4096 : 1));
    c->handbrake = (int16_t)(fmaxf(keys[SDL_SCANCODE_LCTRL] ? 1.f : 0.f, pad.action[GT4_PAD_SIDE]) *
                             (c->flags & 16 ? 4096 : 1));
    c->reverse = (int16_t)(pad.action[GT4_PAD_REVERSE] * (c->flags & 8 ? 4096 : 1));
    c->shift = (int8_t)(keys[SDL_SCANCODE_Q] - keys[SDL_SCANCODE_E] +
                        (pad.action[GT4_PAD_SHIFTUP] > 0) - (pad.action[GT4_PAD_SHIFTDOWN] > 0));
    c->special = pad.action[GT4_PAD_SPECIAL] > 0;
    int view = keys[SDL_SCANCODE_C] || pad.action[GT4_PAD_PLAYVIEW] > 0;
    if (view && !g->view_held_2p) {
        static const int order[3] = {0, 1, 6};
        int k = 0;
        while (k < 3 && order[k] != g->camera_2p)
            k++;
        g->camera_2p = order[(k + 1) % 3];
    }
    g->view_held_2p = view;
    g->look_back_2p = pad.action[GT4_PAD_BACKVIEW] > 0;
}

/* The keyboard is a digital pad (the game eases button steering itself);
   a controller's stick or triggers make it an analog one */
static void read_controls(race_game *g)
{
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    static const uint8_t no_camera_keys[SDL_NUM_SCANCODES];
    if (g->cinema.on)
        keys = no_camera_keys; /* camera keyboard, driving pad remains live */
    float steer = (float)(keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT]);
    float throttle = keys[SDL_SCANCODE_UP] ? 1.f : 0.f, brake = keys[SDL_SCANCODE_DOWN] ? 1.f : 0.f;
    if (getenv(
            "MOON_RACE_TEST_DRIVE")) { /* tests: throttle on, the wheel swinging with the wall clock */
        double t = SDL_GetTicks() / 1000.0;
        throttle = 1;
        steer = (float)(sin(t * 1.7) * .5);
    }
    /* Test controller follows the same B-1 input tape as license_drive_run.
       It changes only pad input; no examination state/threshold is injected. */
    const char *license_brake = getenv("MOON_LICENSE_TEST_BRAKE_AT");
    if (license_brake && g->last_prepared && license_exam(g)) {
        unsigned brake_at = (unsigned)strtoul(license_brake, NULL, 10);
        const char *license_wait = getenv("MOON_LICENSE_TEST_WAIT");
        unsigned wait = license_wait ? (unsigned)strtoul(license_wait, NULL, 10) : 0;
        steer = 0;
        throttle = g->license_test_frame >= wait && g->license_test_frame < brake_at;
        brake = g->license_test_frame >= brake_at;
    }
    float hand = keys[SDL_SCANCODE_SPACE] ? 1.f : 0.f;
    int analog = 0, view;
    command.shift = (int8_t)(g->split ? (keys[SDL_SCANCODE_PAGEUP] - keys[SDL_SCANCODE_PAGEDOWN])
                                      : (keys[SDL_SCANCODE_A] - keys[SDL_SCANCODE_Z]));
    if (g->split)
        read_second_controls(g, keys);
    plat_pad_state state;
    gt4_pad_config config;
    gt4_pad_actions pad = {0};
    int connected = plat_input_poll(0, &state);
    g->pad = connected ? plat_sdl_pad(0) : NULL;
    if (connected) {
        menu_pad_configuration(NULL, 0, &config);
        gt4_pad_evaluate(&config, &state, &pad);
        /* R3 is BUTTON_LICENCE_BEST_LINE; the Tweaks switch holds the action on. */
        int line_pressed = pad.action[GT4_PAD_BEST_LINE] > .5f;
        if (line_pressed && !g->best_line_button)
            g->best_line = !g->best_line;
        g->best_line_button = line_pressed;
        if (video_settings_assist_line())
            pad.action[GT4_PAD_BEST_LINE] = 1.f;
    } else {
        g->best_line_button = 0;
    }
    if (plat_input_is_wheel(0)) {
        pad.steer = -state.axes[0];
        pad.action[GT4_PAD_ACCEL] = state.axes[4];
        pad.action[GT4_PAD_BRAKE] = state.axes[5];
        pad.analog |= (1u << GT4_PAD_LSTEER) | (1u << GT4_PAD_ACCEL) | (1u << GT4_PAD_BRAKE);
    }
    view = keys[SDL_SCANCODE_V] || pad.action[GT4_PAD_PLAYVIEW] > 0;
    g->look_back = pad.action[GT4_PAD_BACKVIEW] > 0;
    if (view && !g->view_held) {
        /* the views in the game's order (D_006211E8: on the car, behind it,
           the roof), func_0037AD10 stepping through them */
        static const int order[3] = {0, 1, 6};
        int k = 0;
        while (k < 3 && order[k] != g->camera)
            k++;
        g->camera = order[(k + 1) % 3];
    }
    g->view_held = view;
    if (connected) {
        if (pad.steer != 0 && steer == 0) {
            steer = pad.steer;
            analog = !!(pad.analog & (1u << GT4_PAD_LSTEER));
        }
        throttle = fmaxf(throttle, pad.action[GT4_PAD_ACCEL]);
        brake = fmaxf(brake, pad.action[GT4_PAD_BRAKE]);
        hand = fmaxf(hand, pad.action[GT4_PAD_SIDE]);
        command.shift = (int8_t)(command.shift + (pad.action[GT4_PAD_SHIFTUP] > 0) -
                                 (pad.action[GT4_PAD_SHIFTDOWN] > 0));
    }
    /* Each channel carries its own analog flag (driver.c/00349F48), so
       moving a stick cannot turn digital pedals off or make 1 mean 1/4096. */
    command.flags = (uint8_t)((analog ? 1 : 0) | (pad.analog & (1u << GT4_PAD_ACCEL) ? 2 : 0) |
                              (pad.analog & (1u << GT4_PAD_BRAKE) ? 4 : 0) |
                              (pad.analog & (1u << GT4_PAD_REVERSE) ? 8 : 0) |
                              (pad.analog & (1u << GT4_PAD_SIDE) ? 16 : 0));
    command.device = plat_input_is_wheel(0) ? 4 : g->pad ? 2 : 1;
    command.steer = (int16_t)(steer * 4096);
    command.throttle = (int16_t)(throttle * (command.flags & 2 ? 4096 : 1));
    command.brake = (int16_t)(brake * (command.flags & 4 ? 4096 : 1));
    command.reverse = (int16_t)(pad.action[GT4_PAD_REVERSE] * (command.flags & 8 ? 4096 : 1));
    command.handbrake = (int16_t)(hand * (command.flags & 16 ? 4096 : 1));
    command.special = pad.action[GT4_PAD_SPECIAL] > 0;
    if (pad.action[GT4_PAD_GHOST] > 0 && !g->ghost_button)
        g->ghost_hidden = !g->ghost_hidden;
    g->ghost_button = pad.action[GT4_PAD_GHOST] > 0;
    if (g->inspect_on) {
        memset(&command, 0, sizeof command);
        command.device = 1;
    }
    if (race_game_bspec(g) && player && !g->replaying) {
        /* B-Spec: the pad as the monitor's controller reads it (00426AF8
           pressed, 00426AF0 held): R1 0x1, R2 0x8000, Left/Right 0x4/0x8
           (with L1 held 0x100/0x200), Up/Down 0x20/0x40, L1 0x80,
           Triangle 0x10000, Circle 0x20000, Cross 0x2000 (the live view's
           camera) and 0x400 (the monitor's map), Square 0x1000. The help
           lines name them (Tool Tip1, RaceMonitorToolTip1). A: the
           controller's own mapping not read; the keyboard stands in with
           the arrows, Tab (R1), O, P, V, M and Shift (L1). */
        uint32_t b = connected ? state.buttons : 0, held = 0;
        if (keys[SDL_SCANCODE_LEFT])
            b |= 1u << GT4_PAD_LEFT;
        if (keys[SDL_SCANCODE_RIGHT])
            b |= 1u << GT4_PAD_RIGHT;
        if (keys[SDL_SCANCODE_UP])
            b |= 1u << GT4_PAD_UP;
        if (keys[SDL_SCANCODE_DOWN])
            b |= 1u << GT4_PAD_DOWN;
        if (keys[SDL_SCANCODE_TAB])
            b |= 1u << GT4_PAD_R1;
        if (keys[SDL_SCANCODE_O])
            b |= 1u << GT4_PAD_TRIANGLE;
        if (keys[SDL_SCANCODE_P])
            b |= 1u << GT4_PAD_CIRCLE;
        if (keys[SDL_SCANCODE_V] || keys[SDL_SCANCODE_M])
            b |= 1u << GT4_PAD_CROSS;
        if (keys[SDL_SCANCODE_LSHIFT])
            b |= 1u << GT4_PAD_L1;
        int l1 = (b >> GT4_PAD_L1) & 1;
        if (b & (1u << GT4_PAD_R1))
            held |= RACE_BSPEC_R1;
        if (b & (1u << GT4_PAD_R2))
            held |= RACE_BSPEC_R2;
        if (l1)
            held |= RACE_BSPEC_L1;
        if (b & (1u << GT4_PAD_LEFT))
            held |= l1 ? RACE_BSPEC_L1_LEFT : RACE_BSPEC_LEFT;
        if (b & (1u << GT4_PAD_RIGHT))
            held |= l1 ? RACE_BSPEC_L1_RIGHT : RACE_BSPEC_RIGHT;
        if (b & (1u << GT4_PAD_UP))
            held |= RACE_BSPEC_UP;
        if (b & (1u << GT4_PAD_DOWN))
            held |= RACE_BSPEC_DOWN;
        if (b & (1u << GT4_PAD_TRIANGLE))
            held |= RACE_BSPEC_OVERTAKE;
        if (b & (1u << GT4_PAD_CIRCLE))
            held |= RACE_BSPEC_PIT;
        if (b & (1u << GT4_PAD_CROSS))
            held |= RACE_BSPEC_VIEW | RACE_BSPEC_MAP;
        if (b & (1u << GT4_PAD_SQUARE))
            held |= RACE_BSPEC_VIEW_BACK;
        g->bspec_pressed |= held & ~g->bspec_buttons;
        g->bspec_buttons = held;
        memset(&command, 0, sizeof command);
        command.device = 1;
    }
}
