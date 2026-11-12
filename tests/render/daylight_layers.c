/* P: alpha coverage does not exclude a material from daylight shading. */
#define main daylight_fixture_main
#include "daylight.c"
#undef main
static int baked_frame(int enabled, int blended, float alpha, uint8_t pixel[3])
{
    gt4_triangle triangles[2] = {0};
    const float vertices[6][3] = {{-2, -2, 3}, {2, -2, 3}, {2, 2, 3},
                                  {-2, -2, 3}, {2, 2, 3},  {-2, 2, 3}};
    for (int i = 0; i < 2; i++) {
        triangles[i].kind = 1;
        triangles[i].blend_enabled = blended;
        triangles[i].state.blend = 0x44;
        triangles[i].texture = -1;
        triangles[i].state.depth_func = 1;
        triangles[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &triangles[i].v[j];
            memcpy(v->p, vertices[i * 3 + j], 12);
            v->normal[2] = -1;
            v->alpha = alpha;
            for (int c = 0; c < 3; c++) {
                v->rgb[c] = .8f;
                v->ambient[c] = .5f;
            }
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
    view.modern_light = 1;
    moon_daylight light = moon_daylight_at(12);
    view.daylight = enabled;
    memcpy(view.sun_direction, light.sun, 12);
    memcpy(view.sun_color, light.direct, 12);
    memcpy(view.sky_color, light.sky, 12);
    memcpy(view.haze_color, light.haze, 12);
    video_hdr_options(1, 0);
    plat_video_begin_frame();
    video_begin_scene();
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_lights lights = {0};
    lights.count = 1;
    lights.direction[0][2] = -1;
    for (int c = 0; c < 3; c++) {
        lights.ambient[0][c] = .2f;
        lights.diffuse[0][c] = .25f;
    }
    r3d_draw_lit(mesh, &view, &lights);
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
    r3d_modern_lighting(1);
    r3d_material_shading(1);
    for (int enabled = 0; enabled <= 1; enabled++) {
        uint8_t opaque[3], decal[3], glass[3], restored[3];
        CHECK(baked_frame(enabled,0,1,opaque));
        CHECK(baked_frame(enabled,1,1,decal));
        CHECK(baked_frame(enabled,1,.5f,glass));
        CHECK(baked_frame(enabled,0,1,restored));
        if (argc > 1 && enabled) memset(decal,0,3);
        for (int c = 0; c < 3; c++) {
            CHECK(abs((int)opaque[c]-decal[c])<=1);
            CHECK(glass[c]<decal[c]);
        }
        CHECK(!memcmp(opaque,restored,3));
    }
    plat_video_shutdown();
    return 0;
}
