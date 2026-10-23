/* Presentation adapter compared with the original callback/upload functions. */
#define r3d_draw observe_draw
#include "../../src/platform/sdl/race_view.c"
#undef r3d_draw
#include "ee.h"
extern ee_fn recomp_lookup(uint32_t);
static r3d_view seen[4];
static const r3d_mesh *meshes[4];
static int draws;
void observe_draw(r3d_mesh *m, const r3d_view *w)
{
    if (draws < 4) {
        meshes[draws] = m;
        seen[draws++] = *w;
    }
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "backfire visual %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    ee_ctx e;
    CHECK(!ee_init(&e));
    CHECK(ee_load_image(&e,argv[1],0x100000)>0);
    race_view v = {0};
    race_asset asset = {0};
    v.now = &asset;
    v.afterfire_tried = 1;
    const float tint[4] = {.7f, .52f, .49f, .8f};
    memcpy(asset.afterfire_color, tint, 16);
    asset.afterfire_scale[0] = 2;
    asset.afterfire_scale[1] = 3;
    for (int i = 0; i < 6; i++) {
        v.afterfire[i] = (void *)(uintptr_t)(i + 1);
        v.afterfire_ground[i] = (void *)(uintptr_t)(i + 11);
    }
    car_model car = {0};
    car.body_cosine = 1;
    car.exhaust_points = 1;
    car.exhaust[0][0] = .3f;
    car.exhaust[0][1] = .8f;
    car.exhaust[0][2] = -.7f;
    car.exhaust[0][5] = 1;
    r3d_view pose = {0};
    pose.rot[0] = pose.rot[4] = pose.rot[8] = 1;
    car_shadow shadow = {0};
    shadow.on = 1;
    shadow.rot[0] = shadow.rot[4] = shadow.rot[8] = 1;
    shadow.pos[1] = -.6f;
    for (int i = 1; i < 600; i++) {
        float flames[4][3] = {{i / 600.f, .5f, 30}, {0}};
        memcpy(e.ram + 0x1800004, flames, 4);
        e.r[4].uw[0] = 0x1800000;
        e.r[29].uw[0] = 0x1ef0000;
        recomp_lookup(0x452f28)(&e);
        float fraction;
        memcpy(&fraction, e.spr + 0x2c30, 4);
        int frame = e.r[2].sw[0];
        draws = 0;
        draw_afterfire(&v, &car, &pose, flames, &shadow);
        CHECK(draws==2);
        if (argc > 2 && !strcmp(argv[2], "--negative"))
            seen[0].grasp = 0;
        if (argc > 2 && !strcmp(argv[2], "--negative-ground"))
            draws = 1;
        CHECK(draws == 2);
        for (int p = 0; p < 2; p++) {
            CHECK(meshes[p]==(void *)(uintptr_t)(frame+(p?11:1)));
            CHECK(seen[p].grasp==fraction);
            CHECK(!memcmp(seen[p].color_mul,tint,12) && seen[p].vertex_alpha==tint[3]);
            CHECK(fabsf(seen[p].pos[0]-.3f)<1e-6f && fabsf(seen[p].pos[2]+.7f)<1e-6f);
            CHECK(fabsf(seen[p].pos[1]-(p?-.58f:.8f))<1e-6f);
            CHECK(seen[p].depth_bias==(p?.005f:0));
        }
    }
    float inactive[4][3] = {{0}};
    draws = 0;
    draw_afterfire(&v, &car, &pose, inactive, &shadow);
    CHECK(!draws);
    ee_free(&e);
    puts(
        "backfire: 599 native body/ground draws match original branch/fraction; tint, scale frame and inactive gate checked");
    return 0;
}
