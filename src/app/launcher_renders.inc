/* P: the Options column's real pictures of the launcher's own rows: the room rendered by the
   showroom itself in each look (Theme: light and dark; Light Colour: the colour before, the
   one chosen and the next), taken before the launcher's frame, one per frame. Every look taken
   is kept (theme by colour) until the car or the window changes, so going through the colours
   never shows a gap. No drawn stand-ins. */
static struct {
    plat_texture *image[2][STUDIO_MOODS];
    int raster;
    char code[48]; /* the car they were taken with */
} studio_renders;
static int studio_renders_ready(int theme, int mood)
{
    return studio_renders.image[theme & 1][(mood + STUDIO_MOODS) % STUDIO_MOODS] != NULL;
}
static void studio_renders_close(void)
{
    for (int t = 0; t < 2; t++)
        for (int m = 0; m < STUDIO_MOODS; m++) {
            plat_texture_destroy(studio_renders.image[t][m]);
            studio_renders.image[t][m] = NULL;
        }
}
/* id: the focused row (-1 Theme, -2 Light Colour, -3 Scene: the room as it is); anything else
   takes nothing */
/* car, angle: where the car stands; eye, target, fov: the camera to give back afterwards. The
   picture is taken from START's framing (the front three-quarter), whatever the panel shows. */
static void studio_renders_step(showroom *cars, const char *code, int raster, int id, int theme,
                                int mood, const float car[3], float angle, const float eye[3],
                                const float target[3], float fov)
{
    if (id < -3 || id > -1)
        return;
    /* the car loaded and faded in first (its fade is 0.4 s): a picture without it shows nothing */
    static int settled;
    if (showroom_prepare(cars, code) != 1) {
        settled = 0;
        return;
    }
    if (settled < 40) {
        settled++;
        return;
    }
    if (raster != studio_renders.raster || strcmp(code, studio_renders.code)) {
        studio_renders_close();
        studio_renders.raster = raster;
        snprintf(studio_renders.code, sizeof studio_renders.code, "%s", code);
    }
    /* the ones on screen first, then the rest of this theme's colours, then the other theme */
    int want[2 * STUDIO_MOODS + 3][2], n = 0;
    if (id == -1)
        want[n][0] = 0, want[n++][1] = mood, want[n][0] = 1, want[n++][1] = mood;
    else if (id == -3)
        want[n][0] = theme, want[n++][1] = mood;
    else
        for (int k = -1; k <= 1; k++)
            want[n][0] = theme, want[n++][1] = (mood + k + STUDIO_MOODS) % STUDIO_MOODS;
    for (int t = 0; t < 2; t++)
        for (int m = 0; m < STUDIO_MOODS; m++)
            want[n][0] = t ? !theme : theme, want[n++][1] = (mood + m) % STUDIO_MOODS;
    for (int k = 0; k < n; k++) {
        int t = want[k][0], m = want[k][1];
        if (studio_renders.image[t][m])
            continue;
        /* the room alone in that look, the car where it stands, then the player's look back */
        float e[3], a[3];
        lcam_place(car, angle, LCAM_PLAY_EYE, e);
        lcam_place(car, angle, LCAM_PLAY_TARGET, a);
        showroom_set_rig(cars, e, a, car, angle);
        showroom_set_rig_fov(cars, LCAM_HOME_FOV);
        showroom_set_room_mood(cars, t, studio_moods[m].rgb);
        plat_video_begin_frame();
        video_begin_scene();
        plat_video_set_view((float)raster, 448.f);
        /* behind a Scene the room is not drawn, so its car fades in here (else the frame's
           own draw does it); kept only once the car is fully there */
        int shown = showroom_draw(cars, code, launcher_scene_ready() ? .05f : 0);
        video_end_scene();
        if (shown)
            studio_renders.image[t][m] = video_grab(NULL, 0, 0, (float)raster, 448.f);
        showroom_set_room_mood(cars, theme, studio_moods[mood].rgb);
        showroom_set_rig(cars, eye, target, car, angle);
        showroom_set_rig_fov(cars, fov);
        return; /* one a frame */
    }
}
