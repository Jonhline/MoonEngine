static uint32_t license_place_table(race_game *g, unsigned *count)
{
    /* 00395338/00395380: active RNW4 variant, 32-byte record,
       count +0xe, 16-byte authored positions at +0x10. */
    uint32_t car = gt4_race_run_car(g->run, 0), course = GT4_U32(car + CAR_COURSE),
             rw = GT4_U32(GT4_U32(course + 4) + 0xdc);
    int variant = (int32_t)GT4_U32(course + 0xb0);
    *count = 0;
    if (variant < 0 || variant >= GT4_U16(rw + 0x54))
        return 0;
    uint32_t row = GT4_U32(rw + 0x9c) + (unsigned)variant * 32;
    *count = GT4_U16(row + 14);
    return GT4_U32(row + 16);
}
static int license_ground(uint32_t course, const float pos[3], float *y, gt4_ray_hit *surface)
{
    /* 00395930 +0x44..0x78: authored y + 1; 00409A48 +0x88..0xc4
       solves the plane, then 00395760 queries ground context from y + 5.
       Query failures keep normal (0,1,0), as 00407EF0 +0x8c..0xc8. */
    gt4_ray_hit hit;
    float query[3] = {pos[0], pos[1] + 1, pos[2]};
    uint32_t mesh = GT4_U32(GT4_U32(course + 4) + 0xdc);
    if (surface) {
        memset(surface, 0, sizeof *surface);
        surface->normal[1] = 1;
        surface->u = 1;
        surface->leaf = -1;
    }
    if (!gt4_course_ray_down(mesh, &hit, query))
        return 0;
    *y = hit.normal[1]
             ? -((hit.normal[0] * pos[0] + hit.normal[2] * pos[2]) + hit.plane) / hit.normal[1]
             : 0;
    if (surface) {
        query[1] = *y + 5;
        if (gt4_course_ray_down(mesh, &hit, query))
            *surface = hit;
    }
    return 1;
}
static void license_place(race_game *g)
{
    unsigned count;
    uint32_t table = license_place_table(g, &count);
    if (!table || (unsigned)g->concourse.placement >= count)
        return;
    uint32_t car = gt4_race_run_car(g->run, 0), params = GT4_U32(car + CAR_PARAMS),
             layout = gt4_car_layout(car), course = GT4_U32(car + CAR_COURSE);
    fenv_t env;
    fegetenv(&env);
    gt4_phys_fpu_enter();
    memcpy(g->license_origin, gt4_ptr(table + g->concourse.placement * 16), 16);
    /* 00408DB8 +0xa4: the original uses 3.1400001, not M_PI. */
    g->license_origin[3] += gt4_f32(0x4048f5c2);
    float y;
    if (license_ground(course, g->license_origin, &y, NULL))
        g->license_origin[1] = y;
    float wheels[2][2];
    for (unsigned i = 0; i < 2; i++) {
        float blend = GT4_F32(car + CAR_VIEW_BLEND);
        wheels[i][0] = GT4_F32(car + 0x900 + i * 4) * blend +
                       GT4_F32(car + 0x16c + i * 0xec) * (1 - blend); /* 00344AA8 */
        int front = (int8_t)GT4_U8(layout + 0x3c + i);
        float half = GT4_F32(params + 0x18) * .5f;
        wheels[i][1] = front > 0 ? -half : front < 0 ? half : 0; /* 00344C08 */
    }
    float local[3][4];
    gt4_license_concourse_people(g->license_car.seat[0] + GT4_F32(0x8464c0),
                                 g->license_car.seat[2] + GT4_F32(0x8464bc),
                                 g->license_car.shift[0], wheels, local);
    float sn = sinf(g->license_origin[3]), cs = cosf(g->license_origin[3]);
    for (unsigned i = 0; i < 3; i++) {
        g->license_people[i][0] = (cs * local[i][0] + sn * local[i][2]) + g->license_origin[0];
        g->license_people[i][1] = g->license_origin[1];
        g->license_people[i][2] = (-sn * local[i][0] + cs * local[i][2]) + g->license_origin[2];
        g->license_people[i][3] = local[i][3] + g->license_origin[3];
        if (license_ground(course, g->license_people[i], &y, g->license_surfaces + i))
            g->license_people[i][1] = y;
    }
    fesetenv(&env);
}
static void license_begin(race_game *g, const char *car, int cars)
{
    unsigned count;
    license_place_table(g, &count);
    gt4_license_concourse_begin(&g->concourse, (int)count, g->license_camera_cycle);
    gt4_license_fade_begin(&g->license_fade);
    g->license_saturation = 1;
    g->license_image_valid = g->license_image_active = 0;
    if (!g->concourse.enabled)
        return;
    car_model_tables *tables = car_model_tables_open(g->vol);
    int metadata = car_model_race_driver(g->vol, tables, car, cars, &g->license_car);
    car_model_tables_close(tables);
    size_t size;
    uint8_t *data = gt4_vol_read(g->vol, "/pit/license_001", &size);
    if (data && metadata)
        g->license_motion = gt4_license_motion_open(data, size);
    free(data);
    if (!g->license_motion) {
        fprintf(stderr, "licence scene: cannot load package/car geometry\n");
        g->concourse.enabled = 0;
        return;
    }
    g->license_scene = license_scene_open(g->license_motion);
    for (unsigned i = 0; i < 3; i++)
        gt4_license_motion_select(g->license_motion, i,
                                  gt4_license_concourse_actor(&g->concourse, i));
    if (g->concourse.placement_ready)
        license_place(g);
}
static void license_step(race_game *g)
{
    if (!g->license_motion || g->demonstrating)
        return;
    /* 00409168: actor/camera clocks first, director transitions after. */
    for (unsigned i = 0; i < 3; i++)
        gt4_license_motion_step(g->license_motion, i, (float)STEP);
    if (gt4_license_concourse_has_camera(&g->concourse))
        gt4_license_motion_step(g->license_motion, 3, (float)STEP);
    int32_t state;
    uint32_t time;
    if (race_game_license_result(g, &state, &time) && state != -2)
        gt4_license_concourse_finish(&g->concourse, state > 0);
    gt4_license_concourse_step(&g->concourse, (float)STEP);
    gt4_license_fade_step(&g->license_fade, g->concourse.enabled && g->concourse.outcome,
                          g->concourse.previous, g->concourse.phase, (float)STEP);
    if (g->concourse.changed & GT4_CONCOURSE_ACTORS)
        for (unsigned i = 0; i < 3; i++)
            gt4_license_motion_select(g->license_motion, i,
                                      gt4_license_concourse_actor(&g->concourse, i));
    if (g->concourse.changed & GT4_CONCOURSE_CAMERA)
        gt4_license_motion_select(g->license_motion, 3,
                                  gt4_license_concourse_camera(&g->concourse));
    if (g->concourse.changed & GT4_CONCOURSE_PLACE)
        license_place(g);
    if (gt4_license_concourse_cut(&g->concourse)) {
        /* 003FAB10 presentation handover, using existing native car APIs;
           ownership of core/physics is unchanged. */
        uint32_t car = gt4_race_run_car(g->run, 0), cond = GT4_U32(car + CAR_SESSION),
                 b = car + CAR_POSITION,
                 v = cond + CONDUCTOR_FIELD_UNK_CAB8 + GT4_U8(car + CAR_PLAYER) * 48;
        fenv_t env;
        fegetenv(&env);
        gt4_phys_fpu_enter();
        gt4_car_place(car, 0, g->license_origin[0], -g->license_origin[2], g->license_origin[1],
                      g->license_origin[3]);
        GT4_U32(b + CAR_BLOCK_VIEW_YAW_LAG) = GT4_U32(b + 0x600) = 0;
        GT4_F32(v) = GT4_F32(b + CAR_BLOCK_HEADING);
        GT4_U32(v + 4) = GT4_U32(v + 8) = 0;
        GT4_F32(v + 12) = GT4_F32(b + CAR_BLOCK_ANGLES);
        for (unsigned o = 0x10; o <= 0x2c; o += 4)
            GT4_U32(v + o) = 0; /* 003455F0 */
        gt4_car_rest(car);
        gt4_walls_in(cond);
        gt4_walls_setup(cond);
        if (GT4_U8(car + CAR_56A) != 4)
            GT4_U8(car + CAR_56B) = 8; /* 003455C8 */
        fesetenv(&env);
        fprintf(stderr, "licence scene: phase %d, %s, authored place %.6f %.6f %.6f\n",
                g->concourse.phase, gt4_license_concourse_camera(&g->concourse),
                g->license_origin[0], g->license_origin[1], g->license_origin[2]);
    }
}
static int license_look(race_game *g, const float look[9], float fov, const float origin[4])
{
    /* 004094C8 +0xb8..0x1a0 rotates eye/target/up by root heading;
       only eye and target receive the authored world translation. */
    float sn = sinf(origin[3]), cs = cosf(origin[3]), world[9];
    for (unsigned i = 0; i < 3; i++) {
        world[i * 3] = cs * look[i * 3] + sn * look[i * 3 + 2];
        world[i * 3 + 1] = look[i * 3 + 1];
        world[i * 3 + 2] = -sn * look[i * 3] + cs * look[i * 3 + 2];
        if (i < 2)
            for (unsigned k = 0; k < 3; k++)
                world[i * 3 + k] += origin[k];
    }
    float back[3], right[3], up[3], rows[9];
    float norm = 0;
    for (unsigned k = 0; k < 3; k++) {
        back[k] = world[k] - world[3 + k];
        norm += back[k] * back[k];
    }
    if (norm <= 0)
        return 0;
    norm = sqrtf(norm);
    for (unsigned k = 0; k < 3; k++)
        back[k] /= norm;
    right[0] = world[7] * back[2] - world[8] * back[1];
    right[1] = world[8] * back[0] - world[6] * back[2];
    right[2] = world[6] * back[1] - world[7] * back[0];
    norm = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (norm <= 0)
        return 0;
    for (unsigned k = 0; k < 3; k++)
        right[k] /= norm;
    up[0] = back[1] * right[2] - back[2] * right[1];
    up[1] = back[2] * right[0] - back[0] * right[2];
    up[2] = back[0] * right[1] - back[1] * right[0];
    memcpy(rows, right, 12);
    memcpy(rows + 3, up, 12);
    memcpy(rows + 6, back, 12);
    float sx, sy;
    gt4_license_camera_projection(fov, &sx, &sy);
    /* C: lens coefficients from 0037DC78/003794A8. A: native viewport
       and inherited .1 near plane; complete PS2 raster proof pending. */
    race_view_set_camera(g->view, world, rows, sx, sy, .1f);
    race_view_show_car(g->view, 1);
    return 1;
}
static int license_camera(race_game *g)
{
    if (!g->license_motion || !gt4_license_concourse_has_camera(&g->concourse))
        return 0;
    float look[9], fov, camera_coord, target_coord;
    if (!gt4_license_motion_camera(g->license_motion, look, &fov, &camera_coord, &target_coord))
        return 0;
    return license_look(g, look, fov, g->license_origin);
}
