static uint32_t session_best = 0x157529ffu;
static int32_t machine_best_speed; /* Power & Speed: MQuickWork +0x49C */
static char machine_race[32];      /* its race_code (mt400_testcourse ...): quick-mt.ad raceLabel */
/* logger.ad analyze_start: the last race replayed a frame at a time
   (00122FA8's thread), 12 ms of steps a frame */
static race_game *logger_game;
static int logger_analysis_begin(void *user)
{
    (void)user;
    char msg[160];
    int ok = race_game_logger_analyze_begin(logger_game, msg, sizeof msg);
    if (!ok)
        fprintf(stderr, "logger: %s\n", msg);
    return ok;
}
static int logger_analysis_step(void *user, float *progress)
{
    (void)user;
    return race_game_logger_analyze_step(logger_game, 0.012, progress);
}
static void logger_analysis_end(void *user, int ok)
{
    (void)user;
    race_game_logger_analyze_end(logger_game, 1, ok);
    menu_logger_source src;
    if (ok && race_game_logger_source(logger_game, &src)) {
        if (career_license.active && career_license.result_applied &&
            (src.time[0] == 0 || src.time[0] == 0x157529ffu))
            src.time[0] = career_license.last_time;
        menu_set_logger_source(&src);
    }
}
static void session_best_update(race_game *g, menu *mn, const char *course, const char *car)
{
    if (career_license.active) {
        menu_set_quick_best(career_license.best_time);
        return;
    }
    { /* 00115CB4..00115D74 (Power & Speed): with car 0 through, the
           session's best time or top speed (+0x498 / +0x49C, km/h x 100:
           m/s x 3600 + 5, /10), then the record (001161A0) */
        int test;
        uint32_t t;
        float v;
        if (free_mode == 4 && race_game_machine_result(g, &test, &t, &v)) {
            int unit = test == 1 ? (strcmp(course, "lasvegas") ? 0 : 3) : test == 2 ? 1 : 2;
            if (unit == 2) {
                int32_t k = (int32_t)((v * 60.f * 60.f + 5.f) / 10.f);
                if (k > machine_best_speed)
                    machine_best_speed = k;
                menu_set_quick_best_speed(machine_best_speed);
            } else if (t < session_best) {
                session_best = t;
                menu_set_quick_best(session_best);
            }
            menu_machine_record_insert(mn, unit, car, unit == 2 ? 0x157529ffu : t,
                                       unit == 2 ? v : 0);
            return;
        }
    }
    uint32_t best = race_game_best_lap(g);
    /* 00113320 (single race) / 00117898 (Time Trial): a new session best
       goes into the course's ranking. */
    if (best < session_best && !career_race.active && !race_game_split(g) &&
        !race_game_replaying(g))
        menu_course_record_insert(mn, course, car, best);
    if (best < session_best)
        session_best = best;
    menu_set_quick_best(session_best);
    menu_set_quick_ghost(race_game_has_ghost(g));
}
static void quick_race_set(race_game *g, const char *course, const char *car, int color,
                           const char *const *others, const int *colors, int n, int tire)
{
    menu_quick_race q;
    memset(&q, 0, sizeof q);
    session_best = 0x157529ffu;
    menu_set_quick_best(session_best);
    machine_best_speed = 0;
    menu_set_quick_best_speed(0);
    snprintf(q.course, sizeof q.course, "%s", course);
    q.entries = n + 1;
    q.player = n;
    q.tire = tire;
    q.dry = 1;
    if (free_mode == 4)
        snprintf(q.race, sizeof q.race, "%s", machine_race);
    race_game_gear_range(g, &q.gear, &q.gear_min, &q.gear_max); /* 00111B50 */
    for (int k = 0; k <= n; k++) {
        int e = k < n ? k + 1 : 0, power = 0, weight = 0;
        snprintf(q.grid[k].car, sizeof q.grid[k].car, "%s", k < n ? others[k] : car);
        q.grid[k].color = k < n ? (colors ? colors[k] : 0) : color;
        if (app_car_ratings && race_game_car_id(g, e))
            gt4_car_ratings_spec(app_car_ratings, race_game_car_id(g, e), &power, &weight);
        q.grid[k].power = power;
        q.grid[k].weight = weight;
    }
    menu_set_quick_race(&q);
}

static int career_race_result(race_game *game, menu *career, menu *quick)
{
    if (career_license.active) {
        if (career_license.result_applied || race_game_replaying(game))
            return 1;
        int32_t state;
        uint32_t elapsed;
        if (!race_game_license_result(game, &state, &elapsed))
            return 0;
        time_t clock_time = time(NULL);
        struct tm date;
        uint32_t packed = 0;
#ifdef _WIN32
        int clock_ok = !localtime_s(&date, &clock_time);
#else
        int clock_ok = localtime_r(&clock_time, &date) != NULL;
#endif
        if (clock_ok)
            packed = (uint32_t)(date.tm_year + 1900) | ((uint32_t)(date.tm_mon + 1) << 16) |
                     ((uint32_t)date.tm_mday << 24);
        /* Original 004320B8 uses the console clock, not calendar.today.
           PC local clock is the host replacement for that hardware call. */
        if (!menu_license_attempt(career, career_license.code, &career_license.visit, state,
                                  elapsed, packed))
            return 0;
        career_license.last_time = elapsed;
        career_license.best_time = menu_license_best(career, career_license.code);
        menu_save_refresh(quick);
        menu_set_quick_best(career_license.best_time);
        fprintf(stderr, "licence attempt %s: state %d time %u best %u\n", career_license.code,
                state, elapsed, career_license.best_time);
        career_license.result_applied = 1;
        return 1;
    }
    if (!career_race.active || career_race.result_applied || race_game_replaying(game))
        return 1;
    gt4_race_hud hud;
    int position = race_game_event_position(game);
    if (position < 0 || !race_game_player_hud(game, &hud))
        return 0;
    uint32_t prize = hud.finished ? gt4_career_race_prize(career_race.row, (uint64_t)position) : 0;
    if (career_race.mission) {
        /* 0011BB60: through (car +0x5B6) -> 0011C328 (record, prize total,
           calendar with the race time car +0x71C) and 0011C1D8 (the grid's
           names and times); otherwise 0011C2E0. The game's car: no wear. */
        menu_save_capture(quick);
        menu_save_refresh(career);
        if (!menu_apply_mission_result(career, career_race.race, hud.finished, position,
                                       hud.best_lap, hud.aspec_points, prize, &career_race.award))
            return 0;
        menu_quick_race q;
        if (!race_game_quick_results(game, &q))
            return 0;
        snprintf(q.race, sizeof q.race, "%s", career_race.race);
        q.finished = 1;
        q.prize = career_race.award;
        q.can_replay = race_game_can_replay(game);
        menu_set_quick_race(&q);
        menu_save_refresh(quick);
        menu_quick_event_result(quick, 1, career_race.award);
        fprintf(stderr, "mission %s: through %d position %d time %u prize %u\n", career_race.race,
                hud.finished, position, hud.best_lap, prize);
        career_race.result_applied = 1;
        return 1;
    }
    /* 00114498 must execute before replay destroys the completed state.
       Restore the latest paid profile before applying another retry. */
    menu_save_capture(quick);
    menu_save_refresh(career);
    if (!race_game_commit_bspec(game, career))
        return 0;
    uint8_t wear[32];
    if (!race_game_player_wear(game, wear) || !menu_apply_car_wear(career, wear))
        return 0;
    if (!race_game_player_distances(game, &career_race.a_distance, &career_race.b_distance))
        return 0;
    career_race.has_distances = 1;
    if (career_race.championship) {
        uint8_t order[6];
        int n = race_game_finish_order(game, order);
        if (n != 6)
            return 0;
        int32_t before[6];
        memcpy(before, career_race.points, sizeof before);
        const gt4_career_race_lookup_data *rules = gt4_game_race_lookup_data();
        gt4_career_championship_points(career_race.points, order, 6, hud.finished,
                                       (const int8_t *)rules->driver_points);
        int16_t units;
        memcpy(&units, career_race.row + 0x2c, 2);
        if (!menu_apply_championship_result(
                career, career_race.race, career_race.series, hud.finished,
                career_race.round + 1 == career_race.rounds, position, hud.best_lap,
                race_game_driver_mode(game), hud.aspec_points, prize, career_race.points, units,
                &career_race.award, &career_race.grand_prize))
            return 0;
        menu_quick_race q;
        if (!race_game_quick_results(game, &q))
            return 0;
        q.championship = 1;
        q.session_number = (int)career_race.round;
        q.final_session = career_race.round + 1 == career_race.rounds;
        q.series_rank = gt4_career_championship_rank(career_race.points);
        q.grand_prize = career_race.grand_prize;
        for (unsigned i = 0; i < 6; i++) {
            unsigned driver = order[i];
            q.total_points[i] = career_race.points[driver];
            q.round_points[i] =
                (int32_t)((uint32_t)career_race.points[driver] - (uint32_t)before[driver]);
            if (!driver)
                q.player = (int)i;
        }
        snprintf(q.race, sizeof q.race, "%s", career_race.race);
        q.finished = 1;
        q.prize = career_race.award;
        q.can_replay = race_game_can_replay(game);
        menu_set_quick_race(&q);
    } else {
        if (!menu_apply_career_result(career, career_race.race, hud.finished, position,
                                      hud.best_lap, race_game_driver_mode(game), hud.aspec_points,
                                      prize, &career_race.best, &career_race.award))
            return 0;
        menu_quick_race q;
        if (!race_game_quick_results(game, &q))
            return 0;
        snprintf(q.race, sizeof q.race, "%s", career_race.race);
        q.finished = 1;
        q.prize = career_race.award;
        q.can_replay = race_game_can_replay(game);
        menu_set_quick_race(&q);
    }
    menu_save_refresh(quick);
    menu_quick_event_result(quick, 1, career_race.award);
    career_race.result_applied = 1;
    return 1;
}
