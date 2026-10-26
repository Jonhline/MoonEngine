int race_game_over(const race_game *g)
{
    if (g && g->ready && g->replaying)
        return g->replay_end >= 3 || g->quit;
    if (g && g->ready && g->mission_presentation == GT4_MISSION_STOPPED)
        return 1;
    return g && g->ready && race_result_done(g->result);
}
int race_game_can_replay(const race_game *g)
{
    return g && g->tape_n > 0 && (!g->split || g->tape_n_2p > 0);
}
void race_game_set_replay_options(race_game *g, int display, int dive)
{
    if (g)
        g->rmode = display ? 2 : dive ? 1 : 0;
}
int race_game_replay(race_game *g, char *msg, size_t len)
{
    if (!g || !g->tape_n || !g->last_course[0]) {
        snprintf(msg, len, "no replay");
        return 0;
    }
    if (race_game_replay_refusal(g, msg, len))
        return 0;
    g->replay_next = 1;
    g->disc_segment = g->disc_segment_2p = 0;
    return race_game_restart(g, msg, len);
}
int race_game_replaying(const race_game *g)
{
    return g && g->replaying;
}
int race_game_read_replay(race_game *g, const char *name, gt4_replay_file *r, char *msg, size_t len)
{
    if (!g || !name || !r)
        return 0;
    memset(r, 0, sizeof *r);
    if (strncmp(name, "/replay/", 8) && gt4_replay_load(name, r))
        return 1;
    if (!g->disc_replays)
        g->disc_replays = gt4_disc_replay_open(g->vol);
    return !strncmp(name, "/replay/", 8)
               ? gt4_disc_replay_read(g->disc_replays, name, r, msg, len)
               : gt4_disc_replay_saved(g->disc_replays, name, r, msg, len);
}
int race_game_demonstrating(const race_game *g)
{
    return g && g->demonstrating;
}
int race_game_end_demo(race_game *g, char *msg, size_t len)
{
    if (!g || !g->demonstrating)
        return 0;
    free(g->tape);
    g->tape = g->before_demo.tape;
    g->before_demo.tape = NULL;
    g->demonstrating = 0;
    g->replay_next = 0;
    int ok = race_game_restart(g, msg, len);
    g->tape_n = g->before_demo.n;
    g->tape_cap = g->before_demo.cap;
    g->thumb_on = g->before_demo.thumb_on;
    g->license_log = g->before_demo.log;
    g->license_log_collecting = 0;
    return ok;
}
int race_game_license_demo(race_game *g, const char *code, char *msg, size_t len)
{
    gt4_replay_file r;
    char path[64];
    if (!g || g->demonstrating || !g->last_prepared || !license_exam(g))
        return 0;
    if (!code)
        code = g->last_desc.license_code;
    if (!*code) {
        snprintf(msg, len, "examination has no demonstration label");
        return 0;
    }
    snprintf(path, sizeof path, "/replay/license/%s%s", strncmp(code, "us", 2) ? "us" : "", code);
    if (!race_game_read_replay(g, path, &r, msg, len))
        return 0;
    /* 00110230 +98..F4 copies the recording's driving options;
       +F8..110 invokes 003EAD58 (+1C8), importing only the recorder into
       the current examination. Historical setup IDs are not restored. */
    gt4_race_desc d = g->last_desc;
    d.manual = r.manual;
    d.assist_asm = r.assist_asm;
    d.assist_tcs = r.assist_tcs;
    g->before_demo.tape = g->tape;
    g->before_demo.n = g->tape_n;
    g->before_demo.cap = g->tape_cap;
    g->before_demo.thumb_on = g->thumb_on;
    g->before_demo.log = g->license_log;
    g->tape = r.tape;
    r.tape = NULL;
    g->tape_n = g->tape_cap = r.steps;
    g->disc_segments = g->disc_segment = g->disc_segments_2p = g->disc_segment_2p = 0;
    g->demonstrating = 1;
    g->replay_next = 1;
    int ok = race_game_start_impl(g, d.course, NULL, 0, NULL, NULL, d.cars - 1, &d, msg, len);
    if (ok) {
        /* 0011D1B8 +44..60: demonstration Scene, RaceInfo +DC=1. */
        uint32_t race = GT4_U32(gt4_race_run_conductor(g->run));
        GT4_U32(race + 0xdc) = 1;
        fprintf(stderr, "demonstration: %s, %zu original commands\n", code, g->tape_n);
    } else {
        char ignored[256];
        race_game_end_demo(g, ignored, sizeof ignored);
    }
    gt4_replay_free(&r);
    return ok;
}

/* the last race as a file (replay_file.h): its lineup and commands; the
   tape is lent, not copied */
int race_game_replay_refusal(const race_game *g, char *msg, size_t len)
{
    int phantoms;
    if (!g || !g->run || (phantoms = gt4_race_run_phantom_count(g->run)) <= 0)
        return 0;
    snprintf(msg, len,
             "This race had %d cars; replays support the %d official cars.",
             gt4_race_run_cars(g->run) + phantoms, MOON_CONDUCTOR_CARS);
    return 1;
}

int race_game_replay_export(const race_game *g, gt4_replay_file *r, int kind)
{
    char why[160];
    if (!g || !r || !g->tape_n || !g->last_course[0])
        return 0;
    if (race_game_replay_refusal(g, why, sizeof why)) {
        fprintf(stderr, "replay: %s\n", why);
        return 0;
    }
    memset(r, 0, sizeof *r);
    r->kind = kind;
    r->legacy_math = g->replaying && g->legacy_replay_math;
    snprintf(r->course, sizeof r->course, "%s", g->last_course);
    snprintf(r->car, sizeof r->car, "%s", g->last_car);
    r->color = g->last_color;
    r->nothers = g->last_nothers;
    r->has_colors = g->last_has_colors;
    for (int i = 0; i < 5; i++) {
        snprintf(r->others[i], sizeof r->others[i], "%s", g->last_others[i]);
        r->colors[i] = g->last_colors[i];
    }
    r->laps = g->laps;
    r->tire = g->tire;
    r->manual = g->manual;
    r->assist_asm = g->assist_asm;
    r->assist_tcs = g->assist_tcs;
    r->time_trial = g->time_trial;
    r->world_mode = g->world_mode;
    if (g->split) {
        const gt4_race_desc *d = &g->split_options;
        r->split_battle = 1;
        r->tape_2p = g->tape_2p;
        r->steps_2p = g->tape_n_2p;
        r->tire_2p = d->tire_2p;
        r->manual_2p = d->manual_2p;
        r->assist_asm_2p = d->assist_asm_2p;
        r->assist_tcs_2p = d->assist_tcs_2p;
        r->split_boost = d->split_boost;
        r->split_handicap = d->split_handicap;
        r->split_tire_damage = d->split_tire_damage;
        for (int i = 0; i < 2; i++)
            if (d->owned_car[i]) {
                r->owned_on[i] = 1;
                memcpy(r->owned[i], d->owned_car[i], GT4_REPLAY_OWNED);
            }
    }
    r->prepared = g->last_prepared;
    if (r->prepared) {
        const gt4_race_desc *d = &g->last_desc;
        r->championship = d->championship;
        r->cars = d->cars;
        r->rng = g->last_rng;
        r->laps = d->laps;
        r->tire = d->tire;
        r->manual = d->manual;
        r->assist_asm = d->assist_asm;
        r->assist_tcs = d->assist_tcs;
        r->time_trial = d->time_trial;
        for (int i = 0; i < 6; i++) {
            r->car_id[i] = d->car[i];
            r->tuned_car[i] = d->tuned_car[i];
            r->event_paint[i] = d->event_paint[i];
            r->owned_on[i] = d->owned_car[i] != NULL;
            if (d->owned_car[i])
                memcpy(r->owned[i], g->last_owned[i], sizeof r->owned[i]);
        }
        if (d->event_row)
            memcpy(r->event_row, d->event_row, sizeof r->event_row);
        else
            memset(r->event_row, 0, sizeof r->event_row);
    }
    snprintf(r->player, sizeof r->player, "%s", gt4_save_player());
    /* the list's car: its name as the game writes it (CAR_NAME), not the label */
    if (g->names[0][0])
        snprintf(r->car_name, sizeof r->car_name, "%s", g->names[0]);
    r->race_time = (uint32_t)(g->tape_n * 1000 / 60);
    r->best_lap = race_game_best_lap(g);
    r->tape = g->tape;
    r->steps = g->tape_n;
    r->thumb = g->thumb_on ? (uint8_t *)g->thumb : NULL;
    return 1;
}

/* A saved replay's picture: the frame the player crosses the line, or
   ten seconds in for a race left before (A: the PS2's own moment and
   picture not read), drawn without the HUD, scaled to 128x96 by averaging. */
int race_game_wants_thumbnail(const race_game *g)
{
    return g && g->ready && !g->replaying &&
           ((!g->thumb_on && g->tape_n >= 600) || (g->thumb_on < 2 && g->finished >= 0));
}
void race_game_set_thumbnail(race_game *g, const uint8_t *rgba, int w, int h)
{
    if (!g || !rgba || w < GT4_REPLAY_THUMB_W || h < GT4_REPLAY_THUMB_H)
        return;
    for (int y = 0; y < GT4_REPLAY_THUMB_H; y++)
        for (int x = 0; x < GT4_REPLAY_THUMB_W; x++) {
            int x0 = x * w / GT4_REPLAY_THUMB_W, x1 = (x + 1) * w / GT4_REPLAY_THUMB_W;
            int y0 = y * h / GT4_REPLAY_THUMB_H, y1 = (y + 1) * h / GT4_REPLAY_THUMB_H;
            unsigned sum[3] = {0}, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++, n++)
                    for (int c = 0; c < 3; c++)
                        sum[c] += rgba[((size_t)yy * w + xx) * 4 + c];
            for (int c = 0; c < 3; c++)
                g->thumb[(y * GT4_REPLAY_THUMB_W + x) * 3 + c] = (uint8_t)(n ? sum[c] / n : 0);
        }
    g->thumb_on = g->finished >= 0 ? 2 : 1;
}

/* a file's race made the last one, ready for race_game_replay */
int race_game_replay_import(race_game *g, const gt4_replay_file *r)
{
    uint8_t *tape;
    if (!g || !r || !r->steps || !r->tape)
        return 0;
    uint8_t *second = NULL;
    if (r->split_battle) {
        if (!r->tape_2p || !r->steps_2p || r->nothers != 1 || !(second = malloc(r->steps_2p * 32)))
            return 0;
        memcpy(second, r->tape_2p, r->steps_2p * 32);
    }
    if (!(tape = malloc(r->steps * 0x20))) {
        free(second);
        return 0;
    }
    race_game_set_split(g, NULL);
    if (r->split_battle) {
        gt4_race_desc d = {0};
        d.tire_2p = r->tire_2p;
        d.manual_2p = r->manual_2p;
        d.assist_asm_2p = r->assist_asm_2p;
        d.assist_tcs_2p = r->assist_tcs_2p;
        d.split_boost = r->split_boost;
        d.split_handicap = r->split_handicap;
        d.split_tire_damage = r->split_tire_damage;
        for (int i = 0; i < 2; i++)
            if (r->owned_on[i])
                d.owned_car[i] = r->owned[i];
        race_game_set_split(g, &d);
    }
    free(g->tape_2p);
    g->tape_2p = second;
    g->tape_n_2p = g->tape_cap_2p = r->split_battle ? r->steps_2p : 0;
    memcpy(tape, r->tape, r->steps * 0x20);
    free(g->tape);
    g->tape = tape;
    g->tape_n = g->tape_cap = r->steps;
    snprintf(g->last_course, sizeof g->last_course, "%s", r->course);
    snprintf(g->last_car, sizeof g->last_car, "%s", r->car);
    g->last_color = r->color;
    g->last_nothers = r->nothers;
    g->last_has_colors = r->has_colors;
    for (int i = 0; i < 5; i++) {
        snprintf(g->last_others[i], sizeof g->last_others[i], "%s", r->others[i]);
        g->last_other_ptr[i] = g->last_others[i];
        g->last_colors[i] = r->colors[i];
    }
    g->laps = r->laps;
    g->tire = r->tire;
    g->manual = r->manual;
    g->assist_asm = r->assist_asm;
    g->assist_tcs = r->assist_tcs;
    g->time_trial = r->time_trial;
    g->world_mode = r->world_mode;
    g->disc_segments = r->disc_segments;
    g->disc_segment = g->disc_segment_2p = 0;
    g->disc_segments_2p = r->disc_segments_2p;
    g->disc_segment_2p = 0;
    for (unsigned i = 0; i < r->disc_segments_2p; i++) {
        g->disc_segment_at_2p[i] = r->disc_segment_at_2p[i];
        memcpy(g->disc_states_2p[i], r->disc_info + r->disc_segment_offset_2p[i], 0x14f4);
    }
    for (unsigned i = 0; i < r->disc_segments; i++) {
        g->disc_segment_at[i] = r->disc_segment_at[i];
        memcpy(g->disc_states[i], r->disc_info + r->disc_segment_offset[i], 0x14f4);
    }
    g->last_prepared = r->prepared;
    g->legacy_replay_math = r->legacy_math;
    if (r->prepared) {
        gt4_race_desc *d = &g->last_desc;
        memset(d, 0, sizeof *d);
        snprintf(d->course, sizeof d->course, "%s", r->course);
        d->cars = r->cars;
        d->championship = r->championship;
        /* GT4M+C / 00388E38 +6C..70, restored via 00387CF0: the
           original model driver must run for a recorded B-Spec session. */
        d->bspec = r->disc_bspec;
        if (r->disc_info) {
            memcpy(g->disc_race_options, r->disc_info + 0x50, 0x60);
            d->disc_race_options = g->disc_race_options;
        }
        d->laps = r->laps;
        d->tire = r->tire;
        d->manual = r->manual;
        d->assist_asm = r->assist_asm;
        d->assist_tcs = r->assist_tcs;
        d->time_trial = r->time_trial;
        d->world_mode = r->world_mode;
        d->license = r->disc_license;
        snprintf(d->license_code, sizeof d->license_code, "%s", r->license_code);
        d->split_battle = r->split_battle;
        d->tire_2p = r->tire_2p;
        d->manual_2p = r->manual_2p;
        d->assist_asm_2p = r->assist_asm_2p;
        d->assist_tcs_2p = r->assist_tcs_2p;
        d->split_boost = r->split_boost;
        d->split_handicap = r->split_handicap;
        d->split_tire_damage = r->split_tire_damage;
        memcpy(g->last_event_row, r->event_row, sizeof g->last_event_row);
        if (r->disc_has_settings)
            d->disc_settings = g->last_event_row;
        /* P: native files represent an absent RACE row with zero bytes.
           Owned Arcade/TT bodies are prepared, but are not career events. */
        const uint8_t no_event[GT4_REPLAY_ROW] = {0};
        d->event_row = (r->disc_info && !r->disc_event) || r->split_battle ||
                               (!r->disc_info && !memcmp(r->event_row, no_event, sizeof no_event))
                           ? NULL
                           : g->last_event_row;
        g->last_rng = r->rng;
        d->event_rng = r->disc_info || r->split_battle || !d->event_row ? NULL : &g->last_rng;
        for (int i = 0; i < 6; i++) {
            if (r->disc_info && i < r->cars) {
                /* Original: func_003DDA08 +0x30 */
                memcpy(g->disc_entries[i], r->disc_info + 0x960 + i * 0x228, 0x228);
                d->disc_entry[i] = g->disc_entries[i];
            }
            d->car[i] = r->car_id[i];
            d->tuned_car[i] = r->tuned_car[i];
            d->event_paint[i] = r->event_paint[i];
            if (r->owned_on[i]) {
                memcpy(g->last_owned[i], r->owned[i], sizeof g->last_owned[i]);
                d->owned_car[i] = g->last_owned[i];
            }
        }
    }
    return 1;
}
