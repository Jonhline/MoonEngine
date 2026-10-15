/* 0032F028 +0x1C0..1D4 binds the same ResultArcade movie in Battle2P.
   A: its full split-world presentation timing is not yet compared. */
static void draw_result(race_game *g, int cars, double dt)
{
    race_result_state rs;
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    int confirm = keys[SDL_SCANCODE_RETURN] ||
                  (g->pad && SDL_GameControllerGetButton(g->pad, SDL_CONTROLLER_BUTTON_START));
    if (g->split) {
        plat_pad_state p;
        if (plat_input_poll(1, &p))
            confirm |= !!(p.buttons & (1u << GT4_PAD_START));
    }
    { /* tests: OPENGT4_RACE_RESULT_CONFIRM presses the confirm on the results, on and off */
        static unsigned tick;
        if (getenv("OPENGT4_RACE_RESULT_CONFIRM") && g->finished > 8)
            confirm = (++tick / 20) & 1;
    }
    if (!g->result) {
        int32_t state;
        uint32_t time;
        g->result = race_game_license_result(g, &state, &time)
                        ? race_result_license_open(g->vol, state, time, g->last_desc.license_code,
                                                   gt4_career_license_time(g->last_event_row, 2), g)
                    : race_game_mission_active(g) ? race_result_mission_open(g->vol, draw_icon, g)
                                                  : race_result_open(g->vol, draw_icon, g);
        race_result_set_sound(g->result, result_sound);
        race_result_set_trophy(g->result, draw_license_trophy);
        race_result_set_screen(g->result, draw_license_screen);
    }
    memset(&rs, 0, sizeof rs);
    rs.cars = cars < 6 ? cars : 6;
    rs.player = 0;
    for (int i = 0; i < rs.cars; i++) {
        gt4_race_hud c;
        if (!gt4_race_run_hud(g->run, i, &c))
            continue;
        rs.rank[i] = c.rank;
        rs.finished[i] = c.finished;
        rs.time[i] = c.line_time;
        rs.laps[i] = c.lap;
        rs.name[i] = g->names[i];
        /* 003BE060..: RaceMachineTest's board, mode 4 for the top speed
           (car +0x6CC), else 5: the entry's laps added up (+0x1B0) */
        if (g->world_mode == 15) {
            uint32_t car = gt4_race_run_car(g->run, i),
                     race = GT4_U32(gt4_race_run_conductor(g->run));
            uint32_t entry = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + (uint32_t)i * 4);
            int test = g->laps < 4 ? g->laps : 0;
            if (test == 0 || test == 3) {
                rs.mode = 4;
                rs.top_speed[i] = GT4_F32(car + CAR_TOP_SPEED);
            } else {
                rs.mode = 5;
                rs.time[i] = GT4_U32(entry + 0x1b0);
            }
            rs.mph = g->unit == 1;
        }
    }
    if (confirm && !g->confirm_held)
        race_result_press(g->result);
    g->confirm_held = confirm;
    race_result_update(g->result, &rs, dt);
    /* Test-only key stream for the authored result menu. It neither
       sets a result nor grants a medal. Seconds are presentation time. */
    const char *result_keys = getenv("OPENGT4_LICENSE_RESULT_KEYS");
    if (g->last_prepared && license_exam(g) && result_keys) {
        g->license_result_test_clock += dt;
        unsigned at = 0;
        for (const char *item = result_keys; item && *item;
             item = strchr(item, ',') ? strchr(item, ',') + 1 : NULL, at++) {
            const char *colon = strchr(item, ':');
            if (colon && at == g->license_result_test_sent &&
                g->license_result_test_clock >= atof(item)) {
                int key = atoi(colon + 1);
                race_result_key(g->result, key);
                g->license_result_test_sent++;
            }
        }
    }
    race_result_draw(g->result);
}
