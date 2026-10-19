/* CAR4 callback 7 ("timezone", 00453698 -> the draw object's +0x7C) picks the car's
   geometry for the course's time of day: 00387FB0 sets 1 on courses whose Env0 +0x10
   has bit 0x8000 (route5, hongkong, sarthe...), 0 elsewhere. Branch 1 is the night
   geometry (lit lamps). Survey: every /car/lod model decodes in both; the ones whose
   callback 7 has two branches change, the others stay identical. */
#include "model.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "car time zone line %d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct {
    char names[1200][32];
    int n;
} list;
static void add(void *user, const char *name, int is_dir, uint32_t size)
{
    list *l = user;
    (void)size;
    if (!is_dir && l->n < 1200 && strlen(name) < 32)
        strcpy(l->names[l->n++], name);
}
static int same(const gt4_model *a, const gt4_model *b)
{
    if (a->count != b->count)
        return 0;
    for (int i = 0; i < a->count; i++)
        if (memcmp(a->triangles[i].v, b->triangles[i].v, sizeof a->triangles[i].v) ||
            a->triangles[i].texture != b->triangles[i].texture)
            return 0;
    return 1;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    int negative = argc > 2 && !strcmp(argv[2], "--negative");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    static list l;
    CHECK(gt4_vol_list(vol, "/car/lod", add, &l) && l.n > 100);
    int changed = 0, decoded = 0, night = 0, glows = 0;
    char first[32] = "";
    for (int i = 0; i < l.n; i++) {
        char path[64];
        snprintf(path, sizeof path, "/car/lod/%s", l.names[i]);
        size_t size;
        unsigned char *data = gt4_vol_read(vol, path, &size);
        if (!data || size < 32 || memcmp(data, "CAR4", 4)) {
            free(data);
            continue;
        }
        gt4_model_stats stats;
        gt4_model *probe = gt4_model_decode_stats(data, size, 0, 0, &stats);
        int has = probe && stats.callbacks[7];
        gt4_model_free(probe);
        gt4_model *day = gt4_model_decode_car_zone(data, size, 0, 0, 0);
        gt4_model *dark = gt4_model_decode_car_zone(data, size, 0, 0, negative ? 0 : 1);
        gt4_model *plain = gt4_model_decode(data, size, 0);
        CHECK(day && dark && plain);
        CHECK(same(day, plain)); /* daytime: as before */
        decoded++;
        night += has;
        if (!same(day, dark)) {
            changed++;
            if (!first[0])
                snprintf(first, sizeof first, "%s", l.names[i]);
        } else
            CHECK(!has); /* unchanged only without the callback */
        gt4_model *light = gt4_model_decode_part(data, size, 0, 3);
        if (light && light->count) {
            glows++;
            if (getenv("CAR_TIME_ZONE_DUMP") && !strncmp(l.names[i], getenv("CAR_TIME_ZONE_DUMP"), 4)) {
                float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
                for (int t = 0; t < light->count; t++)
                    for (int k = 0; k < 3; k++)
                        for (int c = 0; c < 3; c++) {
                            float v = light->triangles[t].v[k].p[c];
                            lo[c] = v < lo[c] ? v : lo[c];
                            hi[c] = v > hi[c] ? v : hi[c];
                        }
                float zlo = 1e9f, zhi = -1e9f;
                for (int t = 0; t < day->count; t++) {
                    float z = day->triangles[t].v[0].p[2];
                    zlo = z < zlo ? z : zlo;
                    zhi = z > zhi ? z : zhi;
                }
                printf("%s body z %g..%g\n", l.names[i], zlo, zhi);
                printf("%s light: %d triangles, x %g..%g y %g..%g z %g..%g, alpha %g blend %d\n",
                       l.names[i], light->count, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
                       light->triangles[0].v[0].alpha, light->triangles[0].blend_enabled);
            }
        }
        gt4_model_free(light);
        if (!strcmp(l.names[i], "ac__0014"))
            printf("ac__0014: day %d triangles, night %d\n", day->count, dark->count);
        gt4_model_free(day);
        gt4_model_free(dark);
        gt4_model_free(plain);
        free(data);
    }
    printf("car time zone: %d lod cars, %d with callback 7, %d change at night (first %s), %d with lamp light\n",
           decoded, night, changed, first, glows);
    CHECK(decoded > 100 && changed > 0 && glows > 0);
    /* the course side (race_view.c): Env0 +0x10 bit 0x8000 night, +0x58 the lamps' light */
    static const struct {
        const char *course;
        int night;
    } courses[] = {{"route5", 1}, {"hongkong", 1}, {"sarthe24", 1}, {"tsukuba", 0}, {"laguna", 0}};
    for (int i = 0; i < 5; i++) {
        char path[64];
        snprintf(path, sizeof path, "/crs/%s", courses[i].course);
        size_t size;
        unsigned char *data = gt4_vol_read(vol, path, &size);
        CHECK(data && size > 0x84);
        unsigned env, flags;
        float light;
        memcpy(&env, data + 0x80, 4);
        CHECK(env < size - 0x60 && !memcmp(data + env, "Env0", 4));
        memcpy(&flags, data + env + 0x10, 4);
        memcpy(&light, data + env + 0x58, 4);
        CHECK(!!(flags & 0x8000) == courses[i].night && !(flags & 0x80000));
        CHECK(courses[i].night ? light > 0 : light == 0);
        free(data);
    }
    gt4_vol_close(vol);
    return 0;
}
