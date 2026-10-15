/* B-Spec's orders through 003F1BD8 for the player's car */
static void bspec_order(void *user, int kind, int value)
{
    race_game *g = user;
    if (!g->run)
        return;
    uint32_t car = gt4_race_run_car(g->run, 0);
    if (car)
        gt4_bspec_order(gt4_race_run_conductor(g->run), GT4_U8(car + CAR_POSITION + 0x440), kind,
                        value);
}
/* the monitor's sounds (+0x9C): 2 a move, 6 an order, 3 refused. A: the
   names taken from the menu's set */
static void bspec_sound(void *user, int id)
{
    race_game *g = user;
    race_audio_menu_sound(g->audio, id == 2 ? "cursor" : id == 6 ? "ok" : "disable", g->se_volume);
}
/* the movie's extrenders: the watched car's picture (current_car) */
/* func_003DC7B0: a car's colour in each section of the lap (the session's
   0x150 the count): grey, darker in the odd ones, without a time there
   (003FA3D0: cond+0xD7A4, 16 a car), else by how many cars were quicker
   through it, red the quickest down to dark blue */
static int bspec_sectors(race_game *g, int car, uint32_t colors[16])
{
    uint32_t cond = gt4_race_run_conductor(g->run);
    int n = (int)gt4_session(cond, 0x150, 0, 0, 0, 0, 0, NULL), cars = gt4_race_run_cars(g->run);
    if (n > 16)
        n = 16;
    for (int s = 0; s < n; s++) {
        uint32_t t = GT4_U32(cond + CONDUCTOR_FIELD_UNK_D7A4_U + (uint32_t)(car * 16 + s) * 4);
        int quicker = 0;
        if (t == 0x157529ffu) {
            colors[s] = s & 1 ? 0x80181818u : 0x80282828u;
            continue;
        }
        for (int k = 0; k < cars; k++)
            quicker += GT4_U32(cond + CONDUCTOR_FIELD_UNK_D7A4_U + (uint32_t)(k * 16 + s) * 4) < t;
        float f = cars > 1 ? (float)quicker / (float)(cars - 1) : 0, e = 1 - f, r = e * 255,
              gg = e * 0.2f * 255, b = (f * 0.4f + e * 0.2f) * 255; /* 0x3ECCCCCC, 0x3E4CCCCC */
        colors[s] =
            0x80000000u | (uint32_t)(int)b << 16 | (uint32_t)(int)gg << 8 | (uint32_t)(int)r;
    }
    return n < 0 ? 0 : n;
}

static void bspec_ext(void *user, const char *path, float x, float y, float w, float h,
                      const uint8_t color[4])
{
    race_game *g = user;
    const char *leaf = strrchr(path, '.');
    leaf = leaf ? leaf + 1 : path;
    if (!strcmp(leaf, "current_car"))
        race_view_draw_icon_front(g->view, race_bspec_selected(g->bspec_ui), x, y, w,
                                  h); /* 00393790 mode 0 */
    else if (!strcmp(leaf, "cargap") && !strncmp(path, "CARPOS_", 7)) {
        /* the strip's car: the one whose grid letter names its CARPOS */
        uint32_t cond = gt4_race_run_conductor(g->run),
                 entries = GT4_U32(GT4_U32(GT4_U32(cond) + 0x60) + 8);
        for (int i = 0; i < gt4_race_run_cars(g->run); i++)
            if ((int)GT4_U32(GT4_U32(entries + (uint32_t)i * 4) + 0x3444) == path[7] - 'A') {
                race_view_draw_icon(g->view, i, x, y, w, h);
                break;
            }
    } else if (!strcmp(leaf, "chip") && !strncmp(path, "BAR", 3)) {
        /* 003D7930: the row's car, its paint chip (003D87F0 -> 00445AE8) */
        uint32_t chip[4];
        int car = race_bspec_row(g->bspec_ui, atoi(path + 3) - 1);
        if (car >= 0 && gt4_race_run_paint_chip(g->run, car, chip))
            race_hud_draw_chip(chip, x, y, w, h, color[3] / 128.f);
    } else if (!strcmp(leaf, "sector") && !strncmp(path, "BAR", 3)) {
        int car = race_bspec_row(g->bspec_ui, atoi(path + 3) - 1);
        uint32_t colors[16];
        if (car >= 0)
            race_hud_draw_sectors(colors, bspec_sectors(g, car, colors),
                                  GT4_U8(gt4_race_run_car(g->run, car) + 0x5b2), x, y, w, h,
                                  color[3] / 128.f);
    } else if (!strcmp(leaf, "tire") && g->bspec_hud)
        race_hud_draw_tire_panel(g->hud, g->bspec_hud, x, y, w, h, color[3] / 128.f);
    else if (!strcmp(leaf, "map") && g->bspec_hud) {
        int letter[6] = {0};
        uint32_t cond = gt4_race_run_conductor(g->run),
                 entries = GT4_U32(GT4_U32(GT4_U32(cond) + 0x60) + 8);
        for (int i = 0; i < 6 && i < gt4_race_run_cars(g->run); i++)
            letter[i] = (int)GT4_U32(GT4_U32(entries + (uint32_t)i * 4) + 0x3444);
        race_hud_draw_map_cars(g->hud, g->bspec_hud, x + w * .5f, y + h * .5f, fminf(w, h), letter,
                               race_bspec_selected(g->bspec_ui), color[3] / 128.f);
    }
}

/* func_003FA508: car a closes on car b ahead: at the last two sections a
   passed, the time it lost to b (the race's section times: cond+0xD40C
   the count, +0xD48C the cars, +0xCDEC their times) is less at the
   later one */
static int bspec_pace_up(uint32_t cond, int a, int b)
{
    uint32_t sa = cond + CONDUCTOR_FIELD_CARS_U + (uint32_t)a * 0x10a0 + 0x104, base = cond + 0xc;
    if (!(int16_t)GT4_U16(sa + 0x4ac) || (GT4_U32(sa + 0x4ac) & 0xffffffu) == 1)
        return 0;
    int sections = (int)gt4_session(cond, 0x150, 0, 0, 0, 0, 0, NULL), gap[2] = {0, 0};
    int sec = (int)GT4_U8(sa + 0x4ae) - 1;
    for (int pass = 0; pass < 2; pass++, sec--) {
        if (sec < 0)
            sec = sections - 1;
        if (sec < 0)
            return 0;
        uint32_t ta = 0x157529ffu, tb = 0x157529ffu;
        int n = (int8_t)GT4_U8(base + 0xd400u + (uint32_t)sec);
        for (int k = 0; k < n; k++) {
            int id = (int8_t)GT4_U8(base + 0xd480u + (uint32_t)sec * 6 + (uint32_t)k);
            uint32_t t = GT4_U32(base + 0xcde0u + (uint32_t)sec * 24 + (uint32_t)k * 4);
            if (id == a)
                ta = t;
            else if (id == b)
                tb = t;
        }
        if (ta == 0x157529ffu || tb == 0x157529ffu)
            return 0;
        gap[pass] = (int)(ta - tb);
        if (!pass && gap[0] <= 0)
            return 0;
    }
    return gap[1] > 0 && gap[0] < gap[1];
}

/* B-Spec's display each frame: the race's state as 003D8DB8 reads it */
static void bspec_frame(race_game *g, double dt)
{
    /* A pit-menu takeover posts event 10 through 0035BF08. Observe its
       native world mode after the event, rather than the start descriptor. */
    if (g->run) {
        int mode = race_game_driver_mode(g);
        g->run_bspec = mode == 1 || mode == 3;
    }
    if (!race_game_bspec(g) || g->replaying || !g->run) {
        race_bspec_close(g->bspec_ui);
        g->bspec_ui = NULL;
        return;
    }
    if (!g->bspec_ui) {
        race_bspec_host h = {bspec_order, bspec_sound, bspec_ext, g};
        g->bspec_ui = race_bspec_open(g->vol, &h, 0);
        if (!g->bspec_ui)
            return;
        if (getenv("OPENGT4_RACE_BSPEC_MONITOR"))
            g->bspec_pressed |= RACE_BSPEC_R1; /* tests: straight into the Race Monitor */
    }
    uint32_t cond = gt4_race_run_conductor(g->run), race = GT4_U32(cond),
             entries = GT4_U32(GT4_U32(race + 0x60) + 8);
    race_bspec_state st;
    memset(&st, 0, sizeof st);
    int n = gt4_race_run_cars(g->run);
    if (n > RACE_BSPEC_CARS)
        n = RACE_BSPEC_CARS;
    int rank[RACE_BSPEC_CARS];
    for (int i = 0; i < n; i++) {
        gt4_race_hud c;
        race_bspec_car *k = &st.car[i];
        uint32_t car = gt4_race_run_car(g->run, i), b = car + CAR_POSITION,
                 entry = GT4_U32(entries + (uint32_t)i * 4);
        if (!gt4_race_run_hud(g->run, i, &c))
            continue;
        rank[i] = c.rank;
        k->index = i;
        k->grid = (int)GT4_U32(entry + 0x3444);
        snprintf(k->name, sizeof k->name, "%s", g->names[i]);
        k->lap = c.lap;
        k->last_lap = c.last_lap;
        k->best_lap = c.best_lap;
        k->run_laps = (int16_t)GT4_U16(car + CAR_FIELD_LAP_COUNT);
        k->distance = GT4_F32(car + CAR_FIELD_COURSE_DISTANCE);
        for (int e = 0; e < 3;
             e++) { /* func_003FA4B0: the race's pit plan (+0xD9F0 the laps, +0xDA20 the tyres) */
            uint32_t at = (uint32_t)(i * 4 + e) * 2;
            k->pit_lap[e] = (int16_t)GT4_U16(cond + 0xd9f0u + at); /* lh: 0xFFFF none */
            k->pit_tire[e][0] = GT4_U8(cond + 0xda20u + at);
            k->pit_tire[e][1] = GT4_U8(cond + 0xda21u + at);
        }
        k->diff = GT4_U32(entry + 0x11e8);
        k->diff_laps = (int32_t)GT4_U32(entry + 0x11ec);
        k->pits =
            GT4_U16(cond + CONDUCTOR_FIELD_PIT_STOP_COUNT_U + (uint32_t)i * 2); /* func_003FA498 */
        /* func_003DA6F8: 3 in the pit (0035BBD8; A: its sequence's cases
           read as the pit flag), 2 out of fuel (00354E38/00354E68), 4 a
           penalty (0035FC80), 5 all four wheels off the course (003F4EA0) */
        k->in_pit =
            GT4_U8(b + CAR_BLOCK_PIT_LANE_STATE) && (GT4_U8(b + CAR_BLOCK_FLAGS_4B1) & 0x10);
        if (k->in_pit)
            k->state = 3;
        else if (GT4_F32(car + CAR_FUEL_TANK) > 0 && GT4_F32(b + CAR_BLOCK_FUEL_TANK) > 0 &&
                 GT4_F32(b + CAR_BLOCK_UNK_4D0) <= 0)
            k->state = 2;
        else if (!c.finished && GT4_U16(car + CAR_FIELD_UNK_712))
            k->state = 4;
        else {
            int off = 1;
            for (int w = 0; w < 4; w++)
                if (!gt4_ground_off_road(car + CAR_GROUND + (uint32_t)w * 0x34))
                    off = 0;
            k->state = off ? 5 : 0;
        }
        if (!i) {
            st.laps = c.laps;
        }
    }
    /* func_003B6FD0's order: the race's places */
    for (int a = 0; a < n; a++)
        for (int z = a + 1; z < n; z++)
            if (rank[z] < rank[a]) {
                int t = rank[a];
                rank[a] = rank[z];
                rank[z] = t;
                race_bspec_car x = st.car[a];
                st.car[a] = st.car[z];
                st.car[z] = x;
            }
    for (int r = 1; r < n; r++)
        st.car[r].pace_up = bspec_pace_up(cond, st.car[r].index, st.car[r - 1].index);
    st.cars = n;
    st.player = 0;
    st.course_length =
        GT4_F32(GT4_U32(GT4_U32(GT4_U32(cond + CONDUCTOR_FIELD_COURSE_U) + 4) + 0xdc) +
                0x14); /* func_00395400 */
    st.feet = g->unit == 1;
    st.clock = GT4_U32(cond + CONDUCTOR_FIELD_RACE_CLOCK_U) / 3; /* func_00358440 */
    uint32_t pcar = gt4_race_run_car(g->run, 0);
    st.pace = GT4_U8(pcar + 0x14 + 0x6d);
    st.overtake = GT4_U8(pcar + 0x14 + 0x6e);
    st.pit = (GT4_U8(pcar + 0x104 + 0x4b1) & 8) != 0;
    /* 003D9998 reads the watched entry, not just the menu visibility. */
    st.pit_sequence = gt4_pit_stage(gt4_race_run_car(g->run, watched_car(g)));
    race_bspec_update(g->bspec_ui, &st, g->bspec_pressed, g->bspec_buttons, dt);
    g->bspec_pressed = 0;
}

void race_game_inspect_toggle(race_game *g)
{
    if (!g || !g->ready || g->split)
        return;
    if (g->cinema.on)
        race_game_cinematic_toggle(g);
    g->inspect_on = !g->inspect_on;
    g->inspect_seeded = 0;
    fprintf(stderr, "race: inspection camera %s (F3; WASD, Q/E, arrows, Shift)\n",
            g->inspect_on ? "on" : "off");
}
