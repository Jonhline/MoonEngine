/* Authored local lighting is radiance, not unlit albedo for a second exposure. */
#define main baked_day_fixture_main
#include "daylight.c"
#undef main
static int authored_course;
static int night_frame(int dynamic, int authored, int hdr, int car, float source, uint8_t pixel[3])
{
    gt4_triangle t[2] = {0};
    float p[6][3] = {{-2, -2, 3}, {2, -2, 3}, {2, 2, 3}, {-2, -2, 3}, {2, 2, 3}, {-2, 2, 3}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].kind = car ? 1 : 0;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &t[i].v[j];
            memcpy(v->p, p[i * 3 + j], 12);
            v->alpha = 1;
            v->normal[2] = -1;
            for (int c = 0; c < 3; c++) {
                v->rgb[c] = source * 255.f / 128.f;
                v->ambient[c] = .5f;
            }
        }
    }
    gt4_model model = {.triangles = t, .count = 2};
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_view v = {0};
    v.rot[0] = v.rot[4] = v.rot[8] = 1;
    v.r[0] = v.u[1] = v.f[2] = 1;
    v.focal = 320;
    v.focal_y = 224;
    v.unlit = !car;
    v.modern_light = car;
    moon_daylight d = moon_night_at(22);
    v.daylight = dynamic;
    v.authored_environment=authored_course;
    v.authored_night = moon_authored_night_weight(authored, d.sun[1]);
    memcpy(v.sun_direction, d.sun, 12);
    memcpy(v.sun_color, d.direct, 12);
    memcpy(v.sky_color, d.sky, 12);
    memcpy(v.haze_color, d.haze, 12);
    r3d_lights lights = {0};
    lights.count = 1;
    lights.direction[0][2] = -1;
    lights.vertex_limit = 1;
    for (int c = 0; c < 3; c++)
        lights.ambient[0][c] = 1;
    r3d_night_sky(1);
    r3d_modern_lighting(dynamic && car);
    video_hdr_options(hdr, 0);
    plat_video_begin_frame();
    video_begin_scene();
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw_lit(mesh, &v, &lights);
    r3d_end();
    video_end_scene();
    int w, h;
    uint8_t *data = plat_sdl_read_pixels(&w, &h);
    if (!data)
        return 0;
    memcpy(pixel, data + (h / 2 * w + w / 2) * 4, 3);
    free(data);
    plat_video_end_frame();
    r3d_mesh_destroy(mesh);
    return 1;
}
static int run(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    video_scene_quality(1, 2);
    r3d_car_lamps(0);
    r3d_car_shadows(0);
    r3d_shadow_map_quality(0);
    r3d_material_shading(0);
    for (int i = 0; i < 2400; i++) {
        moon_daylight d = moon_night_at(i * .01f);
        float w = moon_authored_night_weight(1, d.sun[1]);
        CHECK(w>=0 && w<=1);
        CHECK(moon_authored_night_weight(0,d.sun[1])==0);
        if (d.sun[1] < -.12f)
            CHECK(w==1);
        if (d.sun[1] > .08f)
            CHECK(w==0);
    }
    for (int hdr = 0; hdr < 2; hdr++)
        for (int car = 0; car < 2; car++)
            for (int level = 1; level < 5; level++) {
                uint8_t source[3], night[3], dark[3], restored[3];
                float brightness = .08f * level;
                CHECK(night_frame(0,0,hdr,car,brightness,source));
                CHECK(night_frame(1,argc>1 && !strcmp(argv[1],"--negative")?0:1,hdr,car,brightness,night));
                CHECK(night_frame(1,0,hdr,car,brightness,dark));
                CHECK(night_frame(0,0,hdr,car,brightness,restored));
                printf("night hdr=%d car=%d source=%u restored=%u retained=%u discarded=%u\n", hdr,
                       car, source[0], restored[0], night[0], dark[0]);
                for (int c = 0; c < 3; c++)
                    CHECK(abs((int)source[c]-night[c])<=1);
                CHECK(!memcmp(source,restored,3));
                CHECK(dark[0]+2<night[0]);
            }
    puts(
        "PASS authored night radiance, SDR/HDR/course/car, legacy restoration, continuous frame weight");
    return 0;
}
int main(int argc, char **argv)
{
    int result = run(argc, argv);
    if(result){plat_video_shutdown();return result;}
    authored_course=1;
    for(int level=1;level<5;level++) {
        uint8_t original[3],modern[3];float brightness=.08f*level;
        CHECK(night_frame(0,0,0,0,brightness,original));
        if(argc>1 && !strcmp(argv[1],"--authored-negative"))authored_course=0;
        CHECK(night_frame(1,1,1,0,brightness,modern));
        printf("authored night source=%u HDR=%u\n",original[0],modern[0]);
        for(int c=0;c<3;c++)CHECK(abs((int)original[c]-modern[c])<=2);
    }
    plat_video_shutdown();
    return result;
}
