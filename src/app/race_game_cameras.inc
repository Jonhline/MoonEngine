/* 001119A0 / 00388880: Arcade's menu camera manager runs mode 5.
   Its course bytes come from +0xC0, never the +0xD0 standing-start bank.
   Only presentation advances (0012BC18 -> conductor slot +0x38). */
/* a camera from its eye, the point it looks at and its roll (the
   pregrid's construction) */
static void look(race_game *g, const float eye[3], const float target[3], float roll, float sx,
                 float sy)
{
    float b[3], r[3], u[3], rows[9], n;
    for (int k = 0; k < 3; k++)
        b[k] = eye[k] - target[k];
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
    float a = roll * .017453292519943295f, cs = cosf(a), sn = sinf(a);
    for (int k = 0; k < 3; k++) {
        rows[k] = cs * r[k] - sn * u[k];
        rows[k + 3] = sn * r[k] + cs * u[k];
        rows[k + 6] = b[k];
    }
    race_view_set_camera(g->view, eye, rows, sx, sy, .1f);
}

/* The rolling start's lead-in (cond+0xCBE4, the race's own rolling
   flag) is watched through the TV cameras: the view selector
   (func_00372DF8 -> func_003744F0) picks camera type 1 there, the
   ShotManager (func_003747B8, table 0x0069F9C0 -> func_003FD980), before
   the race's start frame race+0xBC. A: the exact moment it hands back
   to the driving camera is not read (the selector's +0x128/+0x138
   history); the flag's end is used. Tests: MOON_RACE_TV=1 keeps the
   TV cameras for the whole race. */
static int rolling_phase(race_game *g)
{
    gt4_race_hud h;
    return gt4_race_run_hud(g->run, 0, &h) && h.rolling;
}

/* the TV cameras on the player's car (CameraShot): one 60 Hz tick */
static int ground_at(void *ctx, float x, float y, float z, float *height)
{
    const gt4_course *c = race_view_course(((race_game *)ctx)->view);
    return c && gt4_course_height(c, x, z, y, height);
}

static int ground_id(void *ctx, const float p[3])
{
    return gt4_race_run_ground_id(((race_game *)ctx)->run, p);
}

static int camera_wall(void *ctx, float margin, const float from[3], const float to[3],
                       float out[3], int *id)
{
    return gt4_race_run_camera_wall(((race_game *)ctx)->run, margin, from, to, out, id);
}

static float distance_of(void *ctx, const float p[3])
{
    return gt4_race_run_course_distance(((race_game *)ctx)->run, p);
}

static int has_point(void *ctx, int i)
{
    float p[8];
    return race_view_camera_point(((race_game *)ctx)->view, i, p);
}

static int course_point(void *ctx, float d, float out[3])
{
    return gt4_race_run_course_point(((race_game *)ctx)->run, d, out);
}

/* the replay's Dive (the selector's type 8): the director on the car
   watched, cutting on the music's marks */
static int dive_camera(race_game *g)
{
    if (!g->dive_tried) {
        const gt4_course *c = race_view_course(g->view);
        g->dive_tried = 1;
        if (c)
            g->dive = gt4_dive_open(c->data, c->size);
        if (!g->dive)
            fprintf(stderr, "race: no camera points (REP4 +0xB0 +0x30) on this course\n");
    }
    if (!g->dive)
        return 0;
    int watched = g->focus;
    uint32_t car = gt4_race_run_car(g->run, watched);
    float rot[9], wheels[4][3];
    gt4_dive_target t;
    gt4_tvcam_view v;
    if (!car || !gt4_race_run_pose(g->run, watched, t.pos, rot, wheels))
        return 0;
    memcpy(t.rot, rot, sizeof t.rot);
    if (!g->dive_had)
        memcpy(g->dive_prev, t.pos, sizeof t.pos);
    memcpy(t.prev, g->dive_prev, sizeof t.prev);
    memcpy(g->dive_prev, t.pos, sizeof t.pos);
    g->dive_had = 1;
    t.distance = GT4_F32(car + CAR_FIELD_COURSE_DISTANCE);
    t.speed = GT4_F32(car + CAR_ABS_SPEED);
    t.cue = g->cue;
    t.cue_passed = g->cue_passed;
    if (g->cue_passed && getenv("MOON_DIVE_LOG"))
        fprintf(stderr, "dive cue: a mark went by, the next in %.2f s\n", g->cue);
    t.course_point = course_point;
    t.has_point = has_point;
    t.ground = ground_at;
    t.ground_id = ground_id;
    t.wall = camera_wall;
    t.distance_of = distance_of;
    if (!gt4_race_run_place(g->run, watched, t.place, &t.yaw)) {
        memcpy(t.place, t.pos, sizeof t.place);
        t.yaw = 0;
    }
    { /* func_003F3708..3748: the params' sizes */
        uint32_t params = GT4_U32(car + CAR_PARAMS);
        t.size[0] = GT4_F32(params + 0) + GT4_F32(params + 0x50);
        t.size[1] = GT4_F32(params + 4) - GT4_F32(params + 0x50);
        t.size[2] = GT4_F32(params + 8);
        t.size[3] = GT4_F32(params + 0xc);
    }
    t.ctx = g;
    int kind = gt4_dive_step(g->dive, &t, &v);
    if (kind && v.cut)
        race_audio_camera_cut(g->audio);
    if (kind ==
        2) { /* a camera on the car: its model's point (A: the player's model's points for any car) */
        float point[8], eye[3], rows[9], sx;
        if (!race_view_camera_point(g->view, v.shot, point) ||
            !gt4_race_run_camera(g->run, watched, v.shot, point, eye, rows))
            return 0;
        if (getenv("MOON_DIVE_LOG")) {
            static int last = -1;
            if (last != v.shot)
                fprintf(
                    stderr,
                    "dive car point %d fov %.1f pt %.2f %.2f %.2f turns %.1f %.1f %.1f w %.2f\n",
                    v.shot, v.fov, point[0], point[1], point[2], point[3], point[4], point[5],
                    point[6]);
            last = v.shot;
        }
        sx = v.fov > 0     ? 1 / tanf(v.fov * .5f * 0.017453292f)
             : v.shot == 1 ? 1.73205f
                           : 1 / tanf(atanf(1 / point[6]) * 4 / 3);
        sx *= speed_lens_scale(v.shot);
        if (v.heading != 0) { /* the hand's turn (func_004A79B0) about the camera's up */
            float a = v.heading * 0.017453292f, cs = cosf(a), sn = sinf(a), r[3], b[3];
            memcpy(r, rows, sizeof r);
            memcpy(b, rows + 6, sizeof b);
            for (int k = 0; k < 3; k++) {
                rows[k] = cs * r[k] + sn * b[k];
                rows[k + 6] = -sn * r[k] + cs * b[k];
            }
        }
        if (v.pitch != 0) { /* and its nod (func_004A7988) about the camera's right */
            float a = v.pitch * 0.017453292f, cs = cosf(a), sn = sinf(a), u[3], b[3];
            memcpy(u, rows + 3, sizeof u);
            memcpy(b, rows + 6, sizeof b);
            for (int k = 0; k < 3; k++) {
                rows[k + 3] = cs * u[k] - sn * b[k];
                rows[k + 6] = sn * u[k] + cs * b[k];
            }
        }
        if (v.roll != 0) { /* the camera's roll (func_004A79D8), turned as look() turns it */
            float a = v.roll * 0.017453292f, cs = cosf(a), sn = sinf(a), r[3], u[3];
            memcpy(r, rows, sizeof r);
            memcpy(u, rows + 3, sizeof u);
            for (int k = 0; k < 3; k++) {
                rows[k] = cs * r[k] - sn * u[k];
                rows[k + 3] = sn * r[k] + cs * u[k];
            }
        }
        race_view_set_camera(g->view, eye, rows, sx, sx * 1.30122f, 0.1f);
        race_view_show_car(g->view, v.shot != 0);
        return 1;
    }
    if (!kind)
        return 0;
    look(g, v.eye, v.target, v.roll, v.sx, v.sy);
    race_view_show_car(g->view, 1);
    return 1;
}

/* CarTarget 003FBE70 +E4..118 fills +14 from 00344798/003447F8/
   00344828; 003FC618 +C returns that camera anchor. +894 is the model
   origin and is not the TV target. Read through the existing runner API. */
static int tv_target(race_game *g, int watched, gt4_tvcam_target *target)
{
    uint32_t car = gt4_race_run_car(g->run, watched);
    float yaw;
    if (!car || !gt4_race_run_place(g->run, watched, target->pos, &yaw))
        return 0;
    /* 00343BC0: car+0x5B0 and the race's byte +0xCBE5 (car+4) */
    int lap = (int16_t)GT4_U16(car + CAR_FIELD_LAP_COUNT) + (int8_t)GT4_U8(GT4_U32(car + 4) + 0xcbe5);
    target->distance = gt4_tvcam_distance(g->tvcam, GT4_F32(car + CAR_FIELD_COURSE_DISTANCE), lap);
    return 1;
}

static int tv_camera(race_game *g)
{
    if (!g->tv_tried) {
        const gt4_course *c = race_view_course(g->view);
        g->tv_tried = 1;
        if (c)
            g->tvcam = gt4_tvcam_open(c->data, c->size);
        if (!g->tvcam)
            fprintf(stderr, "race: no TV cameras (REP4 +0xB0) on this course\n");
    }
    if (!g->tvcam)
        return 0;
    int watched = watched_car(g);
    gt4_tvcam_target t;
    gt4_tvcam_view v;
    if (!tv_target(g, watched, &t))
        return 0;
    gt4_tvcam_set_zoom(g->tvcam, g->replaying ? g->zoom : 1); /* Triangle's zoom */
    if (!gt4_tvcam_step(g->tvcam, &t, &v))
        return 0;
    if (v.cut)
        race_audio_camera_cut(g->audio);
    look(g, v.eye, v.target, v.roll, v.sx, v.sy);
    race_view_show_car(g->view, 1);
    return 1;
}
