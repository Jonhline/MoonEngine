static int native_result_scroll, native_result_down, native_result_up;

/* The authored board's metrics (result.strb trace): 40 px row pitch, icon
   64x32 at x=80, name at x=148 in hm_16, time right-aligned at x=523 in
   hc_20, header at y=116, title centred at x=320/y=43. */
static void result_clock(char out[24], uint32_t ms)
{
    snprintf(out, 24, "%u'%02u.%03u", ms / 60000u, ms / 1000u % 60u, ms % 1000u);
}

static void native_result_draw(race_game *g, const gt4_race_entrant *e, int n, int scroll)
{
    static const uint8_t white[4] = {212, 212, 212, 255};
    static const uint8_t yellow[4] = {220, 172, 30, 255};
    static const uint8_t panel[4] = {8, 8, 10, 200};
    static const uint8_t row[4] = {0, 0, 0, 55};
    static const uint8_t player_row[4] = {110, 110, 110, 55};
    int official = gt4_race_run_cars(g->run);
    int visible = n < 7 ? n : 7;
    if (scroll < 0)
        scroll = 0;
    if (scroll > n - visible)
        scroll = n - visible;
    if (scroll < 0)
        scroll = 0;
    plat_video_set_view(640, 448);
    plat_draw_quad(NULL, 56, 84, 528, 360, 0, 0, 1, 1, panel);
    race_hud_text(g->hud, "hb_24#0*125,125", 320, 43, 4 | 3, white, "Results");
    race_hud_text(g->hud, "hn_14*90,90", 44.94f, 116, 4, white, "pos.");
    race_hud_text(g->hud, "hn_14*90,90", 84.94f, 116, 4, white, "car");
    race_hud_text(g->hud, "hn_14*90,90", 484.99f, 115.54f, 4, white, "time");
    uint32_t leader = 0;
    for (int i = 0; i < n; i++)
        if (e[i].finished && e[i].place == 1) {
            leader = e[i].finish_time;
            break;
        }
    for (int i = scroll; i < scroll + visible; i++) {
        float y = 135 + (float)(i - scroll) * 40;
        if (e[i].id == 0)
            plat_draw_quad(NULL, 56, y - 4, 528, 38, 0, 0, 1, 1, player_row);
        else
            plat_draw_quad(NULL, 56, y - 4, 528, 38, 0, 0, 1, 1, row);
        char s[24];
        snprintf(s, sizeof s, "%d", e[i].place);
        race_hud_text(g->hud, "hb_24#1*130,130", 78, y - 2, 4 | 2, white, s);
        int icon = e[i].official ? e[i].id : g->others + (e[i].id - official) + 1;
        draw_icon(g, icon, 80, y, 64, 32, white);
        race_hud_text(g->hud, "hm_16#1*86,88", 148, y + 6, 4, white, e[i].name);
        if (!e[i].finished)
            snprintf(s, sizeof s, "Running...");
        else if (e[i].place == 1)
            result_clock(s, e[i].finish_time);
        else {
            char t[24];
            result_clock(t, e[i].finish_time > leader ? e[i].finish_time - leader : 0);
            snprintf(s, sizeof s, "+%s", t);
        }
        race_hud_text(g->hud, "hc_20#1*91,91", 523.29f, y + 4, 4 | 2, yellow, s);
    }
}

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
    { /* tests: MOON_RACE_RESULT_CONFIRM presses the confirm on the results, on and off */
        static unsigned tick;
        if (getenv("MOON_RACE_RESULT_CONFIRM") && g->finished > 8)
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
        native_result_scroll = 0;
        native_result_down = native_result_up = 0;
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
        /* 003DFC50 copies the entry short name (+0x350C), not CAR_NAME +0. */
        rs.name[i] = g->hud_names[i];
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
    const char *result_keys = getenv("MOON_LICENSE_RESULT_KEYS");
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
    gt4_race_entrant ents[MOON_MAX_CARS];
    int nclass = gt4_race_run_classification(g->run, ents, MOON_MAX_CARS);
    if (nclass > 6) {
        int all = 1;
        for (int i = 0; i < nclass; i++)
            if (!ents[i].finished)
                all = 0;
        if (!all)
            return;
        if (getenv("MOON_RESULT_NATIVE_TRACE")) {
            static int last_n = -1, last_scroll = -1;
            if (nclass != last_n || native_result_scroll != last_scroll) {
                fprintf(stderr, "native result: rows=%d scroll=%d visible=%d\n", nclass,
                        native_result_scroll, nclass < 7 ? nclass : 7);
                last_n = nclass;
                last_scroll = native_result_scroll;
            }
        }
        int visible = nclass < 7 ? nclass : 7;
        int maxscroll = nclass > visible ? nclass - visible : 0;
        int down = keys[SDL_SCANCODE_DOWN], up = keys[SDL_SCANCODE_UP];
        if (down && !native_result_down && native_result_scroll < maxscroll)
            native_result_scroll++;
        if (up && !native_result_up && native_result_scroll > 0)
            native_result_scroll--;
        native_result_down = down;
        native_result_up = up;
        native_result_draw(g, ents, nclass, native_result_scroll);
    } else
        race_result_draw(g->result);
}
