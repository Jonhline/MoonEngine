#include "photo_mode.h"
#include "vol.h"
#include "ee.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern ee_fn recomp_lookup(uint32_t);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "photo regions %d: %s\n", __LINE__, #x);                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static void clear(ee_ctx *e)
{
    memset(e->ram + e->r[4].uw[0], (int)e->r[5].uw[0], e->r[6].uw[0]);
    e->r[2].uw[0] = e->r[4].uw[0];
}
static void angle(ee_ctx *e)
{
    e->fpr[0].f = atanf(e->fpr[12].f);
}
static void sine_inverse(ee_ctx *e)
{
    e->fpr[0].f = asinf(e->fpr[12].f);
}

static void tangent(ee_ctx *e)
{
    e->fpr[0].f = tanf(e->fpr[12].f);
}
static void zero_value(ee_ctx *e)
{
    e->fpr[0].f = 0;
}
static void no_op(ee_ctx *e)
{
    (void)e;
}
static void identity_transform(ee_ctx *e)
{
    memcpy(e->ram + e->r[4].uw[0], e->ram + e->r[5].uw[0], 16);
}
static void front(ee_ctx *e)
{
    e->fpr[0].f = 2.2f;
}
static void rear(ee_ctx *e)
{
    e->fpr[0].f = 2.f;
}
static void width(ee_ctx *e)
{
    e->fpr[0].f = 1.8f;
}
static void height(ee_ctx *e)
{
    e->fpr[0].f = 1.4f;
}
static ee_fn lookup(uint32_t pc)
{
    switch (pc) {
    case 0x57d380:
        return tangent;
    case 0x344798:
    case 0x3447f8:
    case 0x344828:
    case 0x3448b8:
    case 0x344858:
    case 0x344888:
        return zero_value;
    case 0x3f3708:
        return front;
    case 0x3f3720:
        return rear;
    case 0x3f3738:
        return width;
    case 0x3f3748:
        return height;
    case 0x4a0690:
    case 0x4a53f8:
    case 0x4a7454:
    case 0x4a7844:
    case 0x4a79b0:
    case 0x4a7988:
    case 0x4a79d8:
    case 0x4a5400:
    case 0x4a06f0:
        return no_op;
    case 0x4a7aa8:
        return identity_transform;
    }
    return pc == 0x5a48d8 ? clear : pc == 0x57d008 ? angle : pc == 0x59d2a0 ? sine_inverse : NULL;
}
static unsigned word(const unsigned char *p)
{
    unsigned u;
    memcpy(&u, p, 4);
    return u;
}
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    ee_ctx e;
    CHECK(!ee_init(&e));
    CHECK(ee_load_image(&e,argv[1],0x100000)>0);
    e.lookup = recomp_lookup;
    e.hook = lookup;
    const char *error = NULL;
    gt4_vol *v = gt4_vol_open(argv[2], &error);
    CHECK(v);
    const char *names[] = {"brooklyn",  "forum",    "fremont", "gion",    "grandcanyon",
                           "louisburg", "nanzenji", "rialt",   "sagano",  "sanmarco",
                           "shibuya",   "snow",     "times",   "tsukiji", "tsumago"};
    unsigned cases = 0;
    for (unsigned k = 0; k < 15; k++) {
        char path[96];
        snprintf(path, sizeof path, "/crs/ss_%s", names[k]);
        size_t size;
        unsigned char *data = gt4_vol_read(v, path, &size);
        CHECK(data);
        gt4_photo_location p;
        CHECK(gt4_photo_location_read(data,size,&p));
        CHECK(p.size<0x700000);
        const unsigned base = 0x1000000;
        memcpy(e.ram + base, p.data, p.size);
        ee_w32(&e, base + 0x1c, base + p.region_table);
        ee_w32(&e, base + 0x24, base + p.camera_table);
        for (unsigned b = 0; b < p.cameras; b++) {
            unsigned set = p.camera_table + b * 16, list = word(p.data + set + 4),
                     n = p.data[set + 2] | (unsigned)p.data[set + 3] << 8;
            CHECK(list<p.size && n<(p.size-list)/4);
            ee_w32(&e, base + set + 4, base + list);
            for (unsigned j = 0; j < n; j++)
                ee_w32(&e, base + list + j * 4, base + word(p.data + list + j * 4));
        }
        for (unsigned i = 0; i < p.regions; i++) {
            unsigned at = p.region_table + i * 64;
            ee_w32(&e, base + at + 0x10, base + word(p.data + at + 0x10));
        }
        ee_w32(&e, 0x1850000, 0x1800000);
        ee_w32(&e, 0x1800004, 0x1810000);
        ee_w32(&e, 0x18100cc, base);
        for (unsigned ix = 0; ix <= 20; ix++)
            for (unsigned iz = 0; iz <= 20; iz++) {
                float point[3] = {p.bounds[0] + (p.bounds[2] - p.bounds[0]) * ix / 20, 6,
                                  p.bounds[1] + (p.bounds[3] - p.bounds[1]) * iz / 20};
                float native[3];
                memcpy(native, point, 12);
                memcpy(e.ram + 0x1820000, point, 12);
                e.r[4].uw[0] = 0x1820000;
                e.r[5].uw[0] = 0x1800000;
                e.r[29].uw[0] = 0x1ef0000;
                e.fpr[12].f = 1.3f;
                recomp_lookup(0x3fecb0)(&e);
                int region = gt4_photo_region(&p, native, 1.3f);
                if (argc > 3 && !strcmp(argv[3], "--negative") && cases == 20)
                    region++;
                CHECK(region==e.r[2].sw[0]);
                CHECK(!memcmp(native,e.ram+0x1820000,12));
                cases++;
            }

        for (unsigned region = 0; region < p.regions; region++) {
            const unsigned char *record = p.data + p.region_table + region * 64;
            unsigned bundle = record[4] | (unsigned)record[5] << 8;
            float centre[3] = {p.camera[0], 0, p.camera[2]};
            /* A valid test start in each polygon bundle, independent of the adapter. */
            int found = 0;
            for (int ix = 1; ix < 80 && !found; ix++)
                for (int iz = 1; iz < 80; iz++) {
                    float xz[2] = {p.bounds[0] + (p.bounds[2] - p.bounds[0]) * ix / 80,
                                   p.bounds[1] + (p.bounds[3] - p.bounds[1]) * iz / 80};
                    if (gt4_photo_region_contains(&p, bundle, xz)) {
                        centre[0] = xz[0];
                        centre[2] = xz[1];
                        found = 1;
                        break;
                    }
                }
            if (!found)
                continue;
            float start[2] = {centre[0], centre[2]};
            gt4_photo_contact native;
            gt4_photo_contact_reset(&native, start);
            memcpy(e.ram + 0x1830000, &native, sizeof native);
            for (int angle = 0; angle < 360; angle += 10) {
                float yaw = (float)angle, fov = 60;
                int adjusted;
                int valid = gt4_photo_aim(&p, region, centre, &yaw, &fov, &adjusted);
                if (argc > 3 && !strcmp(argv[3], "--negative-aim"))
                    adjusted ^= 1;
                float input_fov = 60;
                memcpy(e.ram + 0x1840000, centre, 12);
                memcpy(e.ram + 0x1840020, &input_fov, 4);
                e.r[4].uw[0] = 0x1850000;
                e.r[5].uw[0] = 0x1840010;
                e.r[6].uw[0] = 0x1840020;
                e.r[7].uw[0] = 0x1840000;
                e.r[8].uw[0] = region;
                e.r[29].uw[0] = 0x1ef0000;
                e.fpr[12].f = (float)angle;
                recomp_lookup(0x370ef8)(&e);
                float original_fov;
                memcpy(&original_fov, e.ram + 0x1840020, 4);
                if (valid != e.r[2].sw[0] || adjusted != (int)ee_r32(&e, 0x1840010) ||
                    fabsf(fov - original_fov) > .001f) {
                    fprintf(stderr, "aim %s region%u angle%d N%d,%d,%g EE%d,%d,%g\n", names[k],
                            region, angle, valid, adjusted, fov, e.r[2].sw[0],
                            (int)ee_r32(&e, 0x1840010), original_fov);
                    return 1;
                }
            }
            for (unsigned step = 0; step < 160; step++) {
                float delta[2] = {sinf(step * 1.7f) * p.map_length * .2f,
                                  cosf(step * 1.7f) * p.map_length * .2f};
                float current[3] = {native.point[0], 0, native.point[1]};
                memcpy(e.ram + 0x1820000, current, 12);
                float movement[3] = {delta[0], 0, delta[1]};
                memcpy(e.ram + 0x1840000, movement, 12);
                e.r[4].uw[0] = 0x1820000;
                e.r[5].uw[0] = 0x1840000;
                e.r[6].uw[0] = 0x1800000;
                e.r[7].uw[0] = region;
                e.r[8].uw[0] = 0x1830000;
                e.r[29].uw[0] = 0x1ef0000;
                recomp_lookup(0x3fee08)(&e);
                gt4_photo_region_move(&p, bundle, delta, &native);
                if (argc > 3 && !strcmp(argv[3], "--negative-movement"))
                    native.point[0] += 1;
                gt4_photo_contact original;
                memcpy(&original, e.ram + 0x1830000, sizeof original);
                if (fabsf(native.point[0] - original.point[0]) > .002f ||
                    fabsf(native.point[1] - original.point[1]) > .002f) {
                    fprintf(stderr, "%s region%u move%u native%g,%g kind%d EE%g,%g kind%d\n",
                            names[k], region, step, native.point[0], native.point[1], native.kind,
                            original.point[0], original.point[1], original.kind);
                    return 1;
                }
            }
        }
        free(data);
    }
    for (int i = 1; i < 2000; i++) {
        float distance = i * .125f, out[2];
        gt4_photo_distance_fov(distance, out);
        e.r[4].uw[0] = 0x1820000;
        e.r[5].uw[0] = 0x1820004;
        e.r[29].uw[0] = 0x1ef0000;
        e.fpr[12].f = distance;
        recomp_lookup(0x3ffc68)(&e);
        CHECK(!memcmp(out,e.ram+0x1820000,8));
    }

    const char *courses[] = {"route5", "tsukuba", "sarthe24", "hongkong", "motegi"};
    for (unsigned i = 0; i < 5; i++) {
        char path[96];
        snprintf(path, sizeof path, "/crs/%s", courses[i]);
        size_t n;
        unsigned char *data = gt4_vol_read(v, path, &n);
        CHECK(data);
        gt4_course course;
        CHECK(gt4_course_open(&course,data,n));
        unsigned base = word(data + 0xb0);
        CHECK(base<n && n-base<0x700000);
        unsigned count = word(data + base + 0x2c), at = word(data + base + 0x30);
        CHECK(count>0);
        memcpy(e.ram + 0x1000000, data + base, n - base);
        ee_w32(&e, 0x1000030, 0x1000000 + at);
        for (int step = 0; step <= 100; step++) {
            float distance = course.length * step / 100, range = 500;
            unsigned offsets[63];
            unsigned native = gt4_photo_replay_points(data + base, n - base, course.length,
                                                      distance, offsets, 63);
            e.r[4].uw[0] = 0x1000000;
            e.r[5].uw[0] = 0x1860000;
            e.r[6].uw[0] = 63;
            e.r[7].uw[0] = 0;
            e.r[29].uw[0] = 0x1ef0000;
            e.fpr[12].f = distance;
            e.fpr[13].f = range;
            e.fpr[14].f = course.length;
            recomp_lookup(0x3de268)(&e);
            if (argc > 3 && !strcmp(argv[3], "--negative-points") && native)
                offsets[0] ^= 4;
            CHECK(native==e.r[2].uw[0]);
            for (unsigned k = 0; k < native; k++)
                CHECK(offsets[k]+0x1000000==ee_r32(&e,0x1860000+k*4));
        }
        free(data);
    }
    const float dimensions[4] = {2.2f, 2.f, 1.8f, 1.4f};
    ee_w32(&e, 0x1850000 + 0x184, 0x1841000);
    ee_w32(&e, 0x1800000 + 0x18, 0x1880000);
    for (int i = 0; i < 2000; i++) {
        float eye[3] = {sinf(i * .73f) * 6, cosf(i * .31f) * 2 + 1, sinf(i * .47f) * 6},
              near_clip = (i % 3 + 1) * .1f, fov = 2 + (i % 80),
              distance = sqrtf(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]);
        memcpy(e.ram + 0x1841004, eye, 12);
        memcpy(e.ram + 0x1841020, &distance, 4);
        memcpy(e.ram + 0x1842044, &near_clip, 4);
        e.r[4].uw[0] = 0x1850000;
        e.r[5].uw[0] = 0x1800000;
        e.r[6].uw[0] = 0x1842000;
        e.r[29].uw[0] = 0x1ef0000;
        e.fpr[12].f = fov;
        recomp_lookup(0x380c50)(&e);
        int native = gt4_photo_car_clear(eye, dimensions, fov, near_clip, distance);
        if (argc > 3 && !strcmp(argv[3], "--negative-zoom"))
            native = !native;
        if (native != e.r[2].sw[0]) {
            fprintf(stderr, "car clear case%d point%g,%g,%g fov%g native%d EE%d\n", i, eye[0],
                    eye[1], eye[2], fov, native, e.r[2].sw[0]);
            return 1;
        }
    }
    gt4_vol_close(v);
    ee_free(&e);
    printf(
        "photo: %u region/centroid samples on 15 destinations and 1999 distance FOV limits match original\n",
        cases);
    return 0;
}
