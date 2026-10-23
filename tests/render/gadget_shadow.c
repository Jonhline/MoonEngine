/* The actual resident GDGT meshes and race drawing path, not a mock renderer. */
#include "../../src/platform/sdl/race_view.c"
#include "miniz.h"
#include "plat_sdl.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "gadget GPU %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static uint8_t *shot(race_view *v, const char *path)
{
    plat_video_begin_frame();
    race_view_draw(v);
    int w, h;
    size_t size = 0;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    void *png = p ? tdefl_write_image_to_png_file_in_memory(p, w, h, 4, &size) : NULL;
    FILE *f = png ? fopen(path, "wb") : NULL;
    if (f) {
        fwrite(png, 1, size, f);
        fclose(f);
    }
    mz_free(png);
    plat_video_end_frame();
    return p;
}
static race_view *fixture;
static int run(int argc, char **argv)
{
    CHECK(argc>=3);
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_car_lamps(0);
    r3d_daylight_options(0, 12, 0);
    r3d_night_sky(0);
    r3d_shadow_map_quality(0);
    r3d_car_shadows(0);
    r3d_modern_lighting(0);
    race_view *v = fixture = race_view_open(argv[1]);
    CHECK(v);
    CHECK(race_view_request(v,"tsukuba","ka_01",0));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_Delay(5);
        ready = race_view_poll(v, 0);
    }
    CHECK(ready==1);
    race_asset *a = v->now;
    CHECK(a->gadget_shadow && a->gadgets && v->gadget_shadows);
    gt4_course_gadgets *g = a->gadgets;
    unsigned picked = g->count;
    for (unsigned i = 0; i < g->count; i++)
        if (v->gadget_shadows[g->instance[i].type]) {
            picked = i;
            break;
        }
    CHECK(picked<g->count);
    gt4_course_gadget *p = &g->instance[picked];
    // Read-only course query initializes the static viewer's ground adapter.
    for (unsigned i = 0; i < g->count; i++) {
        gt4_course_gadget *q = &g->instance[i];
        float ground, normal[3];
        int tri;
        if (gt4_course_surface_triangle(a->surface, q->pos[0], q->pos[2], q->pos[1] + 5, &ground,
                                        normal, &tri)) {
            memcpy(q->normal, normal, 12);
            q->plane = -((normal[0] * q->pos[0] + normal[1] * ground) + normal[2] * q->pos[2]);
        }
    }
    for (unsigned s = 0; s <= g->sections; s++)
        if (picked >= g->first[s] && picked < g->first[s] + g->length[s])
            v->gadget_section = (int)s - 1;
    float eye[3] = {p->pos[0] + 3, p->pos[1] + 2, p->pos[2] + 4},
          aim[3] = {p->pos[0], p->pos[1] + .3f, p->pos[2]};
    r3d_view look = {0};
    look_at(&look, eye, aim);
    float axes[9];
    for (int k = 0; k < 3; k++) {
        axes[k] = look.r[k];
        axes[k + 3] = look.u[k];
        axes[k + 6] = -look.f[k];
    }
    race_view_set_camera(v, eye, axes, 1.73205f, 2.25378f, .1f);
    v->hide_car = 1;
    size_t bytes = g->count * sizeof *g->instance;
    void *before = malloc(bytes);
    CHECK(before);
    memcpy(before, g->instance, bytes);
    char path[1024];
    snprintf(path, sizeof path, "%s-off.png", argv[2]);
    a->gadget_shadow = 0;
    uint8_t *off = shot(v, path);
    CHECK(off);
    a->gadget_shadow = argc > 3 ? 0 : 1;
    snprintf(path, sizeof path, "%s-on.png", argv[2]);
    uint8_t *on = shot(v, path);
    CHECK(on);
    unsigned changed = 0;
    for (int i = 0; i < 640 * 480; i++)
        if (memcmp(off + i * 4, on + i * 4, 3))
            changed++;
    printf("GDGT types=%u placements=%u source direction=%g,%g,%g changed=%u chosen=%u plane=%g\n",
           g->types, g->count, a->gadget_shadow_direction[0], a->gadget_shadow_direction[1],
           a->gadget_shadow_direction[2], changed, picked, p->plane);
    CHECK(changed>0);
    CHECK(!memcmp(before,g->instance,bytes));
    a->gadget_shadow = 0;
    snprintf(path, sizeof path, "%s-restored.png", argv[2]);
    uint8_t *restore = shot(v, path);
    CHECK(restore && !memcmp(off,restore,640*480*4));
    free(restore);
    free(before);
    free(on);
    free(off);
    puts("PASS authored shadow pixels, off restoration and immutable instance state");
    return 0;
}

int main(int argc, char **argv)
{
    int result = run(argc, argv);
    race_view_close(fixture);
    plat_video_shutdown();
    return result;
}
