static void draw_license_trophy(void *user, const race_result_trophy_pose *pose)
{
    race_game *g = user;
    if (!g || !pose || pose->model < 0 || pose->model > 2)
        return;
    /* 003E1B58 loads race/trophy.mdl once; 003E18D0 selects the CLUT
       variant using 006A38F0. All three use geometry model zero. */
    if (!g->trophy_tried) {
        g->trophy_tried = 1;
        size_t size = 0;
        uint8_t *bytes = gt4_vol_read(g->vol, "/race/trophy.mdl", &size);
        for (int i = 0; bytes && i < 3; i++) {
            if (!gt4_license_trophy_variant(bytes, size, (unsigned)i))
                continue;
            gt4_model *m = gt4_model_decode_part(bytes, size, 0, 0);
            if (m) {
                g->trophy[i] = r3d_mesh_create(m);
                fprintf(stderr, "licence trophy: model=%d triangles=%d unsupported=%d\n", i,
                        m->count, m->unsupported);
                gt4_model_free(m);
            }
        }
        free(bytes);
    }
    if (!g->trophy[pose->model] || pose->alpha <= 0)
        return;
    /* 003E0FF8 +0x108..0x154: perspective 50 degrees, 640/448
       aspect, near 0x3DCCCCCC; translate, then RotateY, RotateX, RotateZ.
       A: one spatial sample is drawn; the PS2 three-sample accumulation
       and exact field/composite transform are still pending. */
    const float rad = .01745329252f;
    float a = pose->angles[0] * rad, b = pose->angles[1] * rad, c = pose->angles[2] * rad;
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b), cc = cosf(c), sc = sinf(c);
    r3d_view v = {0};
    float rot[9] = {
        cb * cc + sb * sa * sc,  -cb * sc + sb * sa * cc, sb * ca, ca * sc, ca * cc, -sa,
        -sb * cc + cb * sa * sc, sb * sc + cb * sa * cc,  cb * ca};
    memcpy(v.rot, rot, sizeof rot);
    memcpy(v.pos, pose->position, sizeof v.pos);
    v.r[0] = v.u[1] = 1;
    v.f[2] = -1;
    v.focal = 224 / tanf(50 * rad * .5f);
    v.near_clip = .099999994f;
    /* 003E1780: light 0 at (0,1,0,0), ambient 0x3E999999,
       diffuse/specular 0.5; no guessed studio lights. */
    r3d_ui_light light = {0};
    light.direction[1] = 1;
    for (int i = 0; i < 3; i++) {
        light.ambient[i] = .299999982f;
        light.diffuse[i] = light.specular[i] = .5f;
    }
    r3d_begin_ui(0, 0, 640, 480);
    v.ghost = 1;
    r3d_draw_ui(g->trophy[pose->model], &v, &light);
    v.ghost = 2;
    v.ghost_alpha = pose->alpha;
    r3d_draw_ui(g->trophy[pose->model], &v, &light);
    r3d_end();
}

/* the results' play_sound: "ok" on the confirm */
static void result_sound(void *user, const char *name)
{
    race_game *g = user;
    race_audio_menu_sound(g->audio, name, g->se_volume);
}
