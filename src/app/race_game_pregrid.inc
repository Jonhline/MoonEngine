/* 0011D1B8 +44..60 / 00387EF0: reconstruct Scene mode 2 with DC=1.
   003EAAC0/003EB370 step the selected recorder behind the menu. Reuse the
   existing native runner; the player's attempt tape and telemetry stay owned
   by the player. Scene thread completion remains an explicit boundary. */
static int license_menu_reset(race_game *g, char *error, size_t len)
{
    /* 0012B888 +1C..4C closes/reconstructs the same Scene mode; no menu
       reload or player recording reset. Native runner construction is A. */
    gt4_race_run_close(g->run);
    g->run = gt4_race_run_open(g->disc, &g->license_menu_desc, error, len);
    if (!g->run) {
        g->ready = 0;
        return 0;
    }
    player = gt4_race_run_car(g->run, 0);
    playing = g;
    gt4_phys_pad.read = host_read;
    gt4_phys_pad.wheel_state = host_wheel;
    gt4_phys_pad.write = host_write;
    uint32_t race = GT4_U32(gt4_race_run_conductor(g->run));
    GT4_U32(race + 0xdc) = 1;
    g->license_menu_at = 0;
    g->license_menu_ticks = 0;
    gt4_race_run_pose(g->run, 0, g->license_menu_pos, g->license_menu_rot, g->license_menu_wheels);
    gt4_race_run_place(g->run, 0, g->license_menu_origin, g->license_menu_origin + 3);
    if (g->license_menu_motion &&
        !gt4_license_motion_select(g->license_menu_motion, 3, "replay_start_camera1"))
        return 0;
    gt4_tvcam_reset(g->tvcam);
    update_wheel_motion(g, 0, g->license_menu_rot);
    return 1;
}
static int license_menu_prime(race_game *g)
{
    /* C: slot05 Scene E48C recorder+40=286 at Mot1 clock 4.74998
       (285 camera ticks); menu-late recorder+40=1026. Preserve the
       world tick preceding the camera clock, not an invented delay.
       00389990 +0x134..0x14c steps the world before +0x150..0x154
       commits the camera. Full asynchronous Scene scheduling is A. */
    g->license_menu_playing = 1;
    return gt4_race_run_frame(g->run);
}
static int license_menu_begin(race_game *g)
{
    gt4_replay_file r;
    char path[64], error[256];
    const char *code = g->last_desc.license_code;
    /* 0011D1B8 does NOT call 003EAD58 (teacher import): 003EA8C8
       selects the retained E48C recorder. Slot 05 has 1721 player
       commands, not the 1471-command teacher. Always copy the attempt;
       the preview must never consume or overwrite the player's tape.
       P: with no attempt, keep the user-requested automatic teacher. */
    if (g->tape_n) {
        uint8_t *copy = malloc(g->tape_n * 32);
        if (!copy)
            return 0;
        memcpy(copy, g->tape, g->tape_n * 32);
        g->license_menu_desc = g->last_desc;
        if (!license_menu_reset(g, error, sizeof error)) {
            free(copy);
            fprintf(stderr, "license menu replay: %s\n", error);
            return 0;
        }
        free(g->license_menu_tape);
        g->license_menu_tape = copy;
        g->license_menu_steps = g->tape_n;
        g->license_menu_at = 0;
        g->license_menu_playing = 1;
        memset(&g->license_menu_cycle, 0, sizeof g->license_menu_cycle);
        g->license_menu_cycles = 0;
        fprintf(stderr, "license menu replay: %s, %zu player commands\n", code,
                g->license_menu_steps);
        return license_menu_prime(g);
    }
    snprintf(path, sizeof path, "/replay/license/%s%s", strncmp(code, "us", 2) ? "us" : "", code);
    if (!race_game_read_replay(g, path, &r, error, sizeof error)) {
        fprintf(stderr, "license menu demo: %s\n", error);
        return 0;
    }
    g->license_menu_desc = g->last_desc;
    g->license_menu_desc.manual = r.manual;
    g->license_menu_desc.assist_asm = r.assist_asm;
    g->license_menu_desc.assist_tcs = r.assist_tcs;
    if (!license_menu_reset(g, error, sizeof error)) {
        gt4_replay_free(&r);
        fprintf(stderr, "license menu demo: %s\n", error);
        return 0;
    }
    free(g->license_menu_tape);
    g->license_menu_tape = r.tape;
    r.tape = NULL;
    g->license_menu_steps = r.steps;
    g->license_menu_at = 0;
    g->license_menu_playing = 1;
    memset(&g->license_menu_cycle, 0, sizeof g->license_menu_cycle);
    g->license_menu_cycles = 0;
    fprintf(stderr, "license menu demo: %s, %zu original commands\n", code, g->license_menu_steps);
    gt4_replay_free(&r);
    return license_menu_prime(g);
}

void race_game_pregrid_update(race_game *g, double dt)
{
    if (!g || !g->ready)
        return;
    if (race_game_license_active(g)) {
        /* 0011D1B8 +44..60: Scene mode 2, race+DC=1; slot 05
           confirms camera kind 4, state 2 and the course +D4 bank.
           003749A0 / 0037D734 select replay_start_camera1; 003EBC10
           advances its controlled sequence alongside the teacher replay. */
        if (!g->license_menu_playing && !license_menu_begin(g))
            return;
        if (!g->license_menu_motion) {
            const gt4_course *c = race_view_course(g->view);
            uint32_t at = 0;
            if (c && c->size >= 0xd8)
                memcpy(&at, c->data + 0xd4, 4);
            if (c && at && at < c->size)
                g->license_menu_motion = gt4_license_motion_open_camera(c->data + at, c->size - at);
            if (!g->license_menu_motion ||
                !gt4_license_motion_select(g->license_menu_motion, 3, "replay_start_camera1"))
                return;
        }
        race_view_set_pose(g->view, g->license_menu_pos, g->license_menu_rot);
        race_view_set_wheels(g->view, g->license_menu_wheels);
        for (int i = 0; i < 7; i++)
            race_view_set_opponent(g->view, i, NULL, NULL, NULL);
        race_view_set_smoke(g->view, NULL);
        race_view_set_leaves(g->view, NULL);
        g->pregrid_acc += dt;
        while (g->pregrid_acc >= STEP) {
            if (gt4_license_menu_cycle_update(&g->license_menu_cycle)) {
                char error[256];
                if (!license_menu_reset(g, error, sizeof error)) {
                    fprintf(stderr, "license menu cycle: %s\n", error);
                    return;
                }
                if (!license_menu_prime(g))
                    return;
                g->license_menu_cycle.job = 0;
                g->license_menu_cycle.ready = 1;
                g->license_menu_cycles++;
                fprintf(stderr, "license menu cycle: %s restarted %u\n", g->last_desc.license_code,
                        g->license_menu_cycles);
            }
            if (g->license_menu_cycle.paused || g->license_menu_cycle.phase) {
                gt4_license_menu_cycle_draw(&g->license_menu_cycle, 0);
                g->pregrid_acc -= STEP;
                continue;
            }
            /* 003C0208: countdown seconds from the authored RACE kind.
               0037D338 +FC and 0037D620 +B4: opening adds 60 frames,
               then decrements before choosing D_00621290's control. */
            if (g->license_menu_at < g->license_menu_steps && !gt4_race_run_frame(g->run))
                return;
            if (gt4_race_run_pose(g->run, 0, g->license_menu_pos, g->license_menu_rot,
                                  g->license_menu_wheels)) {
                race_view_set_pose(g->view, g->license_menu_pos, g->license_menu_rot);
                race_view_set_wheels(g->view, g->license_menu_wheels);
                update_wheel_motion(g, 0, g->license_menu_rot);
            }
            float shadow_pos[3], shadow_rot[9], sun, level;
            if (gt4_race_run_shadow(g->run, 0, shadow_pos, shadow_rot, &sun, &level))
                race_view_set_shadow(g->view, 0, shadow_pos, shadow_rot, sun, level);
            else
                race_view_set_shadow(g->view, 0, NULL, NULL, 0, 0);
            g->license_menu_distance = GT4_F32(player + 0x5a8);
            unsigned kind = g->last_event_row[0x40] + 1;
            int seconds = kind == 2 ? 8 : kind == 3 ? 10 : kind == 4 ? 0 : g->last_event_row[0x41];
            int left = seconds * 60 - (int)++g->license_menu_ticks;
            if (left < -60) {
                /* 0037DAD8 changes the opening's E8 to 3 at expiry;
                   003745B0..5D8 then selects TV. Confirmed independently
                   in the late menu PINE capture (kind 1/state 2).
                   The TV target follows the native teacher car. */
                if (!g->tv_tried) {
                    const gt4_course *c = race_view_course(g->view);
                    g->tv_tried = 1;
                    if (c)
                        g->tvcam = gt4_tvcam_open(c->data, c->size);
                }
                gt4_tvcam_target target;
                gt4_tvcam_view camera;
                if (g->tvcam && tv_target(g, 0, &target) &&
                    gt4_tvcam_step(g->tvcam, &target, &camera)) {
                    look(g, camera.eye, camera.target, camera.roll, camera.sx, camera.sy);
                    race_view_show_car(g->view, 1);
                }
                /* 003EB370 +298..2CC reports recorder completion. A:
                   decoded pad EOF replaces the original Scene thread signal. */
                gt4_license_menu_cycle_draw(&g->license_menu_cycle,
                                            g->license_menu_at == g->license_menu_steps);
                g->pregrid_acc -= STEP;
                continue;
            }
            int control = gt4_license_opening_control(left + 59);
            if (!gt4_license_motion_step_control(g->license_menu_motion, 3, gt4_f32(0x3c888888),
                                                 control))
                return;
            float look[9], fov, cc, tc;
            if (!gt4_license_motion_camera(g->license_menu_motion, look, &fov, &cc, &tc) ||
                !license_look(g, look, fov, g->license_menu_origin))
                return;
            gt4_license_menu_cycle_draw(&g->license_menu_cycle,
                                        g->license_menu_at == g->license_menu_steps);
            g->pregrid_acc -= STEP;
        }
        return;
    }
    if (!g->pregrid_tried) {
        const gt4_course *c = race_view_course(g->view);
        g->pregrid_tried = 1;
        if (c)
            g->pregrid = gt4_pregrid_open(c->data, c->size);
        if (!g->pregrid)
            fprintf(stderr, "pregrid: unsupported or missing REP4 camera bank\n");
    }
    if (!g->pregrid)
        return;
    g->pregrid_acc += dt;
    while (g->pregrid_acc >= STEP) {
        gt4_pregrid_camera c;
        if (!gt4_pregrid_step(g->pregrid, &c))
            return;
        float b[3], r[3], u[3], rows[9], n;
        for (int k = 0; k < 3; k++)
            b[k] = c.eye[k] - c.target[k];
        n = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        if (n <= 0)
            return;
        for (int k = 0; k < 3; k++)
            b[k] /= n;
        n = sqrtf(b[0] * b[0] + b[2] * b[2]);
        if (n <= 0)
            return;
        r[0] = b[2] / n;
        r[1] = 0;
        r[2] = -b[0] / n;
        u[0] = b[1] * r[2];
        u[1] = b[2] * r[0] - b[0] * r[2];
        u[2] = -b[1] * r[0];
        /* 0037C138 +0x660..+0x674 builds Rz(roll)*Rx(pitch)*Ry(yaw).
           004A7350 +0x14..+0x28 has degree multiplier 0x3C8EFA35.
           A: cross products/libm replace the matrix VU path, not yet compared. */
        float a = c.roll * .017453292519943295f, cs = cosf(a), sn = sinf(a);
        for (int k = 0; k < 3; k++) {
            rows[k] = cs * r[k] - sn * u[k];
            rows[k + 3] = sn * r[k] + cs * u[k];
            rows[k + 6] = b[k];
        }
        race_view_set_camera(g->view, c.eye, rows, c.sx, c.sy,
                             .1f); /* A: inherited race near plane; pregrid original not proved */
        g->pregrid_acc -= STEP;
    }
}
int race_game_draw_pregrid(race_game *g)
{
    if (!g || !g->ready)
        return 0;
    if (race_game_license_active(g))
        return g->license_menu_motion && race_view_draw_license_menu(g->view);
    return g->pregrid && race_view_draw_pregrid(g->view);
}
void race_game_draw_pregrid_overlay(race_game *g)
{
    if (!g || !g->ready || !g->license_menu_playing || !race_game_license_active(g))
        return;
    /* 0012BC18 +1D4..214: black alpha over Scene AND menu. E scalar
       clock; A 8-bit GL conversion/fullscreen rasterization. */
    float alpha = g->license_menu_cycle.alpha;
    if (alpha <= 0)
        return;
    uint8_t black[4] = {0, 0, 0, (uint8_t)(alpha * 255.f)};
    plat_video_set_view(640, 480);
    plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
}
void race_game_return_pregrid(race_game *g)
{
    if (!g)
        return;
    if (g->cinema.on)
        race_game_cinematic_toggle(g);
    race_audio_close(g->audio);
    g->audio = NULL;
    race_result_close(g->result);
    g->result = NULL;
    g->inspect_on = g->inspect_seeded = 0;
    race_pause_close(g->pause);
    g->pause = NULL;
    g->quit = 0;
    gt4_pregrid_reset(g->pregrid);
    g->pregrid_acc = 0;
    if (g->license_menu_motion)
        gt4_license_motion_select(g->license_menu_motion, 3, "replay_start_camera1");
    g->license_menu_ticks = 0;
    g->license_menu_playing = 0;
    gt4_tvcam_reset(g->tvcam);
}
