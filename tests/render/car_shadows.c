/* P: geometric projection union, opacity, receiver depth and reversible state. */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "render3d.h"
#include "video_gl.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "car shadow %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static r3d_mesh *rectangle(float x0, float x1, float z0, float z1, float height, int red)
{
    gt4_triangle t[2] = {0};
    const float points[6][3] = {{x0, height, z0}, {x1, height, z0}, {x1, height, z1},
                                {x0, height, z0}, {x1, height, z1}, {x0, height, z1}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            memcpy(t[i].v[j].p, points[i * 3 + j], sizeof points[0]);
            t[i].v[j].rgb[0] = 1;
            t[i].v[j].rgb[1] = t[i].v[j].rgb[2] = red ? 0 : 1;
            t[i].v[j].alpha = 1;
        }
    }
    gt4_model model = {0};
    model.triangles = t;
    model.count = 2;
    return r3d_mesh_create(&model);
}

static int frame(r3d_car_shadow **pass, r3d_mesh *floor, r3d_mesh *caster, r3d_mesh *blocker,
                 int enabled, int duplicate, float offset, uint8_t *pixels)
{
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.cam[1] = 5;
    view.cam[2] = -6;
    view.r[0] = 1;
    view.f[1] = -.4472136f;
    view.f[2] = .8944272f;
    view.u[1] = .8944272f;
    view.u[2] = .4472136f;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    if (!r3d_begin_target(128))
        return 0;
    glClearColor(.25f, .25f, .25f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(floor, &view);
    if (blocker)
        r3d_draw(blocker, &view);
    if (enabled) {
        const float plane[4] = {0, 1, 0, -.02f}, light[3] = {.8f, -1, .3f}, black[3] = {0};
        if (!r3d_car_shadow_begin(pass, &view, plane, light))
            return 0;
        r3d_view posed = view;
        posed.pos[0] = offset;
        r3d_car_shadow_mesh(*pass, caster, &posed);
        if (duplicate)
            r3d_car_shadow_mesh(*pass, caster, &posed);
        r3d_car_shadow_end(*pass, enabled == 2 ? 0 : .6f, black);
    }
    r3d_end();
    r3d_end_target(pixels);
    return 1;
}

static int contact_frame(r3d_mesh *floor, r3d_mesh *contact, r3d_mesh *blocker, int enabled,
                         uint8_t *pixels)
{
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.cam[1] = 6;
    view.cam[2] = 4;
    view.r[0] = view.u[2] = 1;
    view.f[1] = -1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    if (!r3d_begin_target(128))
        return 0;
    glClearColor(.25f, .25f, .25f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r3d_begin(0, 0, 640, 448, NULL);
    r3d_draw(floor, &view);
    if (blocker)
        r3d_draw(blocker, &view);
    if (enabled) {
        view.unlit = 0;
        view.shadow = 1;
        view.shadow_alpha = .65f;
        view.rot[0] = .3f;
        view.rot[8] = .4f;
        view.pos[1] = .02f;
        if (enabled == 2) {
            view.pos[2] = 4;
            view.rot[0] = 1.25f;
            view.rot[8] = 2;
            view.shadow_alpha = .45f;
            r3d_draw(contact, &view);
        }
        for (int wheel = 0; enabled == 1 && wheel < 4; wheel++) {
            view.pos[0] = wheel & 1 ? 1 : -1;
            view.pos[2] = wheel & 2 ? 5 : 3;
            r3d_draw(contact, &view);
        }
    }
    r3d_end();
    r3d_end_target(pixels);
    return 1;
}

int main(int argc, char **argv)
{
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    r3d_mesh *floor = rectangle(-12, 12, -1, 20, 0, 0);
    r3d_mesh *caster = rectangle(-1, 1, 3, 5, 1, 0);
    r3d_mesh *blocker = rectangle(-3, 3, 2, 6, .5f, 1);
    CHECK(floor&&caster&&blocker);
    r3d_car_shadow *pass = NULL;
    uint8_t base[65536], shadow[65536], twice[65536], zero[65536], moved[65536], front[65536],
        front_shadow[65536];
    CHECK(frame(&pass,floor,caster,NULL,0,0,0,base));
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    CHECK(frame(&pass,floor,caster,NULL,negative?0:1,0,0,shadow));
    int changed = 0, soft = 0;
    for (int i = 0; i < 128 * 128; i++) {
        if (shadow[i * 4] < base[i * 4])
            changed++;
        if (base[i * 4] > 80 && shadow[i * 4] > base[i * 4] * .5f &&
            shadow[i * 4] < base[i * 4] * .95f)
            soft++;
    }
    printf("projected pixels=%d soft-edge pixels=%d\n", changed, soft);
    CHECK(changed>30&&soft>5);
    CHECK(frame(&pass,floor,caster,NULL,1,1,0,twice));
    CHECK(!memcmp(shadow,twice,sizeof shadow));
    CHECK(frame(&pass,floor,caster,NULL,2,0,0,zero));
    CHECK(!memcmp(base,zero,sizeof base));
    CHECK(frame(&pass,floor,caster,NULL,1,0,2,moved));
    CHECK(memcmp(shadow,moved,sizeof shadow));
    CHECK(frame(&pass,floor,caster,blocker,0,0,0,front));
    CHECK(frame(&pass,floor,caster,blocker,1,0,0,front_shadow));
    int occluded = 0;
    for (int i = 0; i < 128 * 128; i++)
        if (front[i * 4] > 80 && front[i * 4 + 1] < 5) {
            CHECK(!memcmp(front+i*4,front_shadow+i*4,4));
            occluded++;
        }
    CHECK(occluded>30);
    CHECK(frame(&pass,floor,caster,NULL,0,0,0,zero));
    CHECK(!memcmp(base,zero,sizeof base));
    r3d_mesh *contact = r3d_wheel_contact_create();
    CHECK(contact);
    CHECK(contact_frame(floor, contact, NULL, 0, base));
    CHECK(contact_frame(floor, contact, NULL,
                        !(argc > 1 && !strcmp(argv[1], "--contact-negative")), shadow));
    int contact_pixels = 0, contact_soft = 0;
    for (int i = 0; i < 128 * 128; i++) {
        contact_pixels += shadow[i * 4] + 5 < base[i * 4];
        contact_soft += shadow[i * 4] + 2 < base[i * 4] && shadow[i * 4] > base[i * 4] * .6f;
    }
    for (int quadrant = 0; quadrant < 4; quadrant++) {
        int dark = 0, minimum = 255;
        for (int y = quadrant & 2 ? 64 : 0; y < (quadrant & 2 ? 128 : 64); y++) {
            for (int x = quadrant & 1 ? 64 : 0; x < (quadrant & 1 ? 128 : 64); x++) {
                int at = (y * 128 + x) * 4;
                dark += shadow[at] + 5 < base[at];
                if (shadow[at] < minimum)
                    minimum = shadow[at];
            }
        }
        printf("wheel contact quadrant=%d dark=%d minimum=%d\n", quadrant, dark, minimum);
        CHECK(dark > 8 && minimum < 128 * .7f);
    }
    printf("four wheel contacts: dark=%d soft=%d\n", contact_pixels, contact_soft);
    CHECK(contact_pixels > 20 && contact_soft > 8);
    CHECK(contact_frame(floor, contact, blocker, 0, front));
    CHECK(contact_frame(floor, contact, blocker, 1, front_shadow));
    CHECK(!memcmp(front, front_shadow, sizeof front));
    CHECK(contact_frame(floor, contact, NULL,
                        argc>1 && !strcmp(argv[1],"--ambient-negative")?0:2,shadow));
    int middle = (64 * 128 + 64) * 4;
    printf("ambient body centre=%d baseline=%d\n", shadow[middle], base[middle]);
    CHECK(shadow[middle] < base[middle]*.75f);
    CHECK(contact_frame(floor, contact, blocker, 2, front_shadow));
    CHECK(!memcmp(front, front_shadow, sizeof front));
    r3d_mesh_destroy(contact);
    r3d_car_shadow_destroy(pass);
    r3d_mesh_destroy(floor);
    r3d_mesh_destroy(caster);
    r3d_mesh_destroy(blocker);
    plat_video_shutdown();
    puts("PASS union, soft edge, opacity, moving caster, receiver depth and restoration");
    return 0;
}
