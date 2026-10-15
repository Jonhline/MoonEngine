/* 00122FA8 +0x1f0..0x210: the Logger's entries 0 and 1 after the
   session step, until the player's finish (no result-screen frames) */
static void logger_sample(race_game *g)
{
    fenv_t env;
    fegetenv(&env);
    gt4_phys_fpu_enter();
    gt4_logger_input input;
    gt4_logger_car_input(&input, gt4_ptr(gt4_race_run_car(g->run, 0)));
    gt4_logger_sample(&g->license_log, &input);
    float bx, bz; /* entry 1 while it is in the race (the Time Trial's ghost waits off it) */
    if (gt4_race_run_cars(g->run) >= 2 && gt4_race_run_xy(g->run, 1, &bx, &bz) >= 2) {
        gt4_logger_car_input(&input, gt4_ptr(gt4_race_run_car(g->run, 1)));
        gt4_logger_sample(&g->log_b, &input);
    }
    fesetenv(&env);
    if (gt4_race_run_finished(g->run)) {
        if (getenv("OPENGT4_LICENSE_LOGGER_LOG"))
            fprintf(stderr, "logger: finished at %u points\n", g->license_log.count);
        g->license_log_collecting = 0;
        /* 00122D38: each entry's laps added up; A: the race's clock
           when it last crossed the line */
        for (int i = 0; i < 2 && i < gt4_race_run_cars(g->run); i++) {
            gt4_race_hud hud;
            if (gt4_race_run_hud(g->run, i, &hud) && hud.line_time != 0x157529FFu)
                g->log_time[i] = hud.line_time;
        }
        if (getenv("OPENGT4_LICENSE_LOGGER_LOG"))
            fprintf(stderr, "licence logger: points=%u distance=%.6f max-speed=%.6f\n",
                    g->license_log.count, gt4_logger_length(&g->license_log),
                    g->license_log.maximum_speed);
    }
}

/* 00122FA8 / 0010FEE8: the Logger's analysis of the session again, its
   recorded commands replayed with no picture and the ghost (a best lap
   loaded for a track) driving as entry 1; the loaded track A swaps the
   two. A: the original replays the world on a thread with a progress */
int race_game_logger_analyze_begin(race_game *g, char *msg, size_t len)
{
    if (!g || !(g->time_trial || (g->last_prepared && license_exam(g))) ||
        !race_game_can_replay(g)) {
        snprintf(msg, len, "no race to analyse");
        return 0;
    }
    g->log_analyzing = 1;
    g->replay_next = 1;
    g->disc_segment = g->disc_segment_2p = 0;
    if (!race_game_restart(g, msg, len)) {
        g->log_analyzing = 0;
        return 0;
    }
    g->log_steps = 0;
    return 1;
}
/* 00122FA8 +0x1B8..0x1E0: steps until the tape ends; its progress as
   00122CC0 reads it (+0x2AC / +0x2B0: where the replay is of its whole) */
int race_game_logger_analyze_step(race_game *g, double budget, float *progress)
{
    if (!g || !g->log_analyzing || !g->run)
        return -1;
    uint64_t start = plat_time_ns();
    while (g->tape_at < g->tape_n && g->log_steps < g->tape_n + 600) {
        if (!gt4_race_run_frame(g->run))
            return -1;
        g->log_steps++;
        if (g->license_log_collecting)
            logger_sample(g);
        if ((double)(plat_time_ns() - start) / 1e9 >= budget)
            break;
    }
    if (progress)
        *progress = g->tape_n ? (float)g->tape_at / (float)g->tape_n : 1.f;
    return g->tape_at >= g->tape_n || g->log_steps >= g->tape_n + 600;
}
void race_game_logger_analyze_end(race_game *g, int track, int ok)
{
    if (!g || !g->log_analyzing)
        return;
    if (ok && g->run)
        for (int i = 0; i < 2 && i < gt4_race_run_cars(g->run); i++) {
            gt4_race_hud hud;
            if (!g->log_time[i] && gt4_race_run_hud(g->run, i, &hud) &&
                hud.line_time != 0x157529FFu)
                g->log_time[i] = hud.line_time;
        }
    g->license_log_collecting = 0;
    g->log_analyzing = 0;
    if (ok && track == 0 && g->log_b.count) { /* the loaded lap as track A */
        gt4_logger_sampler t = g->license_log;
        g->license_log = g->log_b;
        g->log_b = t;
        gt4_logger_point *p = g->license_log_points;
        g->license_log_points = g->log_b_points;
        g->log_b_points = p;
        uint32_t u = g->log_time[0];
        g->log_time[0] = g->log_time[1];
        g->log_time[1] = u;
    }
    race_game_return_pregrid(g);
    if (getenv("OPENGT4_LICENSE_LOGGER_LOG"))
        fprintf(stderr, "logger analysis%s: A %u points, B %u points\n", ok ? "" : " stopped",
                g->license_log.count, g->log_b.count);
}
int race_game_logger_analyze(race_game *g, int track, char *msg, size_t len)
{
    if (!race_game_logger_analyze_begin(g, msg, len))
        return 0;
    int r;
    while ((r = race_game_logger_analyze_step(g, 1e9, NULL)) == 0) {
    }
    if (r < 0)
        snprintf(msg, len, "the analysis step failed");
    race_game_logger_analyze_end(g, track, r > 0);
    return g->license_log.count > 0;
}
/* 0010FEE8 +174 / 00122FA8: load a recording into one Logger bank.
   P: synchronous native reconstruction, atomic bank replacement. A: native
   file metadata replaces FileReplayBestLap. No solver formula is changed. */
int race_game_logger_load(race_game *g, const gt4_replay_file *r, int track, char *msg, size_t len)
{
    if (!g || !r || track < 0 || track > 1 || !g->license_log_points || !g->log_b_points ||
        !g->license_log.count || !r->tape || !r->steps || r->split_battle ||
        strcmp(r->course, g->last_course) || (r->kind == GT4_REPLAY_BEST_LAP && !r->ghost_car)) {
        snprintf(msg, len, "Choose a recording from this course.");
        return 0;
    }
    gt4_replay_file old, borrowed;
    uint8_t *packed = NULL;
    size_t bytes = 0;
    if (!race_game_replay_export(g, &borrowed, GT4_REPLAY_FULL) ||
        !gt4_replay_pack(&borrowed, &packed, &bytes) || !gt4_replay_unpack(packed, bytes, &old)) {
        free(packed);
        snprintf(msg, len, "Cannot preserve the current recording.");
        return 0;
    }
    free(packed);
    gt4_race_desc desc = g->last_desc;
    int owned = g->arcade_owned_on, loaded_power = 0;
    gt4_logger_sampler banks[2] = {g->license_log, g->log_b}, loaded;
    gt4_logger_point *copies[2] = {NULL, NULL}, *points = calloc(GT4_LOGGER_POINTS, sizeof *points);
    int ok = points != NULL;
    uint32_t times[2] = {g->log_time[0], g->log_time[1]};
    for (int i = 0; i < 2; i++)
        if (banks[i].count) {
            copies[i] = malloc((size_t)banks[i].count * sizeof **copies);
            if (!copies[i])
                ok = 0;
            else
                memcpy(copies[i], banks[i].points, (size_t)banks[i].count * sizeof **copies);
        }
    if (ok && race_game_replay_import(g, r)) {
        g->arcade_owned_on = 0;
        g->log_analyzing = 1;
        g->replay_next = 1;
        ok = race_game_restart(g, msg, len);
        if (ok) {
            /* 003F58B0 / 0035ABA8: best-lap analysis begins at its stored
               state, without waiting for a new live-player lap event.
               P: reconstruct into entry zero of a private native session. */
            if (r->kind == GT4_REPLAY_BEST_LAP) {
                uint8_t state[0x14f4] = {0};
                state[2] = (uint8_t)r->ghost_rest;
                memcpy(state + 0x54, r->ghost_car, GT4_GHOST_CAR);
                uint32_t cond = gt4_race_run_conductor(g->run), race = GT4_U32(cond),
                         car = gt4_race_run_car(g->run, 0);
                fenv_t env;
                fegetenv(&env);
                gt4_phys_fpu_enter();
                GT4_U32(cond + CONDUCTOR_FIELD_RACE_CLOCK) = 100;
                GT4_U32(race + 0xbc) = 0;
                disc_checkpoint_restore(cond, 0, state);
                GT4_U16(car + CAR_FIELD_LAP_COUNT) = 1;
                GT4_F32(car + CAR_VIEW_BLEND) = 0;
                fesetenv(&env);
            }
            menu_quick_race grid;
            if (race_game_quick_grid(g, &grid))
                loaded_power = grid.grid[grid.player].power;
            if (ok) {
                fenv_t env;
                fegetenv(&env);
                gt4_phys_fpu_enter();
                gt4_logger_begin(&loaded, points, gt4_logger_spacing(banks[0].course_length),
                                 banks[0].course_length);
                fesetenv(&env);
                for (size_t step = 0; step < r->steps && ok; step++) {
                    ok = gt4_race_run_frame(g->run);
                    if (ok) {
                        gt4_logger_input input;
                        fegetenv(&env);
                        gt4_phys_fpu_enter();
                        gt4_logger_car_input(&input, gt4_ptr(gt4_race_run_car(g->run, 0)));
                        gt4_logger_sample(&loaded, &input);
                        fesetenv(&env);
                    }
                }
                ok = ok && loaded.count > 0;
            }
        }
    } else
        ok = 0;
    g->log_analyzing = 0;
    g->license_log_collecting = 0;
    int restored = race_game_replay_import(g, &old);
    g->last_desc = desc;
    g->arcade_owned_on = owned;
    if (restored) {
        g->replay_next = 1;
        restored = race_game_restart(g, msg, len);
        race_game_return_pregrid(g);
    }
    gt4_replay_free(&old);
    /* The other bank and its pointer remain unchanged even after rebuild. */
    for (int i = 0; i < 2; i++) {
        gt4_logger_sampler *dst = i ? &g->log_b : &g->license_log;
        gt4_logger_point *buf = i ? g->log_b_points : g->license_log_points;
        *dst = banks[i];
        dst->points = buf;
        if (copies[i])
            memcpy(buf, copies[i], (size_t)banks[i].count * sizeof *buf);
        free(copies[i]);
        g->log_time[i] = times[i];
    }
    if (ok && restored) {
        gt4_logger_sampler *dst = track ? &g->log_b : &g->license_log;
        memcpy(dst->points, points, (size_t)loaded.count * sizeof *points);
        gt4_logger_point *buf = dst->points;
        *dst = loaded;
        dst->points = buf;
        g->log_time[track] = r->best_lap != 0x157529ffu ? r->best_lap : r->race_time;
        snprintf(g->logger_loaded_car[track], 96, "%s", r->car);
        g->logger_loaded_power[track] = loaded_power;
    } else
        snprintf(msg, len, "Cannot reconstruct this recording.");
    free(points);
    return ok && restored;
}
int race_game_logger_source(const race_game *g, menu_logger_source *s)
{
    memset(s, 0, sizeof *s);
    s->subnumber = -1;
    if (!g || !g->run || !g->license_log.count)
        return 0;
    s->track[0] = &g->license_log;
    if (g->log_b.count)
        s->track[1] = &g->log_b;
    if (getenv("OPENGT4_LICENSE_LOGGER_LOG"))
        fprintf(stderr, "logger source: A %u points %.1f m, B %u points, collecting %d\n",
                g->license_log.count, gt4_logger_length(&g->license_log), g->log_b.count,
                g->license_log_collecting);
    menu_quick_race q;
    int grid = race_game_quick_grid(g, &q);
    for (int i = 0; i < 2; i++) {
        gt4_race_hud hud;
        s->time[i] = g->log_time[i];
        if (!s->time[i] && i < gt4_race_run_cars(g->run) && gt4_race_run_hud(g->run, i, &hud) &&
            hud.line_time != 0x157529FFu)
            s->time[i] = hud.line_time; /* the Time Trial left from its pause: where it stood */
        if (grid && i < gt4_race_run_cars(g->run) && gt4_race_run_hud(g->run, i, &hud) &&
            hud.grid_slot >= 0 && hud.grid_slot < q.entries) {
            snprintf(s->car[i], sizeof s->car[i], "%s", q.grid[hud.grid_slot].car);
            s->power[i] = q.grid[hud.grid_slot].power;
        } else if (
            i == 1 &&
            g->log_b.count) { /* the ghost: the player's own car (RaceLicense 003BDC88 clones it) */
            snprintf(s->car[1], sizeof s->car[1], "%s", s->car[0]);
            s->power[1] = s->power[0];
        }
        if (!s->car[i][0] && (i == 0 || g->log_b.count))
            snprintf(s->car[i], sizeof s->car[i], "%s",
                     g->last_car); /* the Time Trial's grid keeps no label */
    }
    for (int i = 0; i < 2; i++)
        if (g->logger_loaded_car[i][0]) {
            snprintf(s->car[i], sizeof s->car[i], "%s", g->logger_loaded_car[i]);
            s->power[i] = g->logger_loaded_power[i];
        }
    snprintf(s->course, sizeof s->course, "%s", g->last_course);
    if (getenv("OPENGT4_LICENSE_LOGGER_LOG"))
        fprintf(stderr, "logger source: cars '%s' %d / '%s' %d, times %u / %u\n", s->car[0],
                s->power[0], s->car[1], s->power[1], s->time[0], s->time[1]);
    /* logger.ad setLoggerInfo: RaceModeName's key and the number after
       it (the licence class of its RACE label l0<class><number>, as the
       table names them: GTMODE_LICENSE_A "A" License). A: the keys taken
       from the table, the original getters not read */
    if (license_exam(g)) {
        const char *code = g->last_desc.license_code, *c = code;
        char cls[8] = "";
        int n = 0;
        if (!strncmp(c, "l0", 2))
            c += 2;
        while (*c && !(*c >= '0' && *c <= '9') && n < 7) {
            cls[n++] = (char)(*c >= 'a' && *c <= 'z' ? *c - 32 : *c);
            c++;
        }
        cls[n] = 0;
        snprintf(s->mode, sizeof s->mode, "GTMODE_LICENSE_%s", cls);
        s->subnumber = *c ? atoi(c) : -1;
    } else
        snprintf(s->mode, sizeof s->mode, "ARCADE_TIME_TRIAL");
    return 1;
}
