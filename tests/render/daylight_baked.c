/* Baked course colours must not receive their ambient light twice. */
#include "plat_sdl.h"
#undef main
#define main daylight_fixture_main
#include "daylight.c"
#undef main
#define main SDL_main
static int authored_fixture,hdr_fixture=1;
static int baked_frame(int enabled, int hour, float source, int face, uint8_t pixel[3])
{
    gt4_triangle triangles[2] = {0};
    const float vertices[6][3] = {{-2, -2, 3}, {2, -2, 3}, {2, 2, 3},
                                  {-2, -2, 3}, {2, 2, 3},  {-2, 2, 3}};
    for (int i = 0; i < 2; i++) {
        triangles[i].texture = -1;
        triangles[i].state.depth_func = 1;
        triangles[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &triangles[i].v[j];
            memcpy(v->p, vertices[i * 3 + j], 12);
            /* Shadow-side wall, ground, front wall, and absent authored normals. */
            if (face == 0)
                v->normal[0] = -1;
            if (face == 1)
                v->normal[1] = 1;
            if (face == 2)
                v->normal[2] = -1;
            v->alpha = 1;
            for (int c = 0; c < 3; c++)
                v->rgb[c] = source * 255.f / 128.f;
        }
    }
    gt4_model model = {.triangles = triangles, .count = 2};
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    moon_daylight light = moon_daylight_at(hour);
    view.daylight = enabled;
    view.authored_environment=authored_fixture;
    memcpy(view.sun_direction, light.sun, 12);
    memcpy(view.sun_color, light.direct, 12);
    memcpy(view.sky_color, light.sky, 12);
    memcpy(view.haze_color, light.haze, 12);
    video_hdr_options(hdr_fixture, 0);
    plat_video_begin_frame();
    video_begin_scene();
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(mesh, &view);
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
int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    video_scene_quality(1, 2);
    r3d_material_shading(0);
    r3d_modern_lighting(0);
    r3d_shadow_map_quality(0);
    const float levels[] = {.05f, .1f, .2f, .5f};
    for (int face = 0; face < 4; face++)
        for (unsigned i = 0; i < sizeof levels / sizeof *levels; i++) {
            uint8_t original[3], noon[3], restored[3], evening[3];
            CHECK(baked_frame(0,12,levels[i],face,original));
            CHECK(baked_frame(1,12,levels[i],face,noon));
            CHECK(baked_frame(1,17,levels[i],face,evening));
            CHECK(baked_frame(0,12,levels[i],face,restored));
            if (argc > 1 && !strcmp(argv[1],"--negative"))
                memset(noon, 0, sizeof noon); /* Negative: lost shadow-side detail. */
            printf("baked %.2f original=%u noon=%u evening=%u\n", levels[i], original[0], noon[0],
                   evening[0]);
            for (int c = 0; c < 3; c++)
                CHECK(abs((int)noon[c]-original[c])<=1);
            CHECK(!memcmp(original,restored,3));
            CHECK(memcmp(noon,evening,3));
        }
    /* Actual course draws set authored_environment. Their LDR colours
       are display-referred, so neutral relighting must not tone-map twice. */
    for(int hdr=0;hdr<2;hdr++)for(int face=0;face<4;face++)
        for(unsigned i=0;i<sizeof levels/sizeof *levels;i++) {
            uint8_t original[3],modern[3];
            authored_fixture=1;hdr_fixture=0;
            CHECK(baked_frame(0,12,levels[i],face,original));
            authored_fixture=argc>1 && !strcmp(argv[1],"--authored-negative")?0:1;
            hdr_fixture=hdr;
            CHECK(baked_frame(1,12,levels[i],face,modern));
            printf("authored HDR=%d source=%.2f classic=%u modern=%u\n",hdr,levels[i],original[0],modern[0]);
            for(int c=0;c<3;c++)CHECK(abs((int)modern[c]-original[c])<=2);
        }
    plat_video_shutdown();
    return 0;
}
