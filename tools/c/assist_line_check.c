/*
 * assist_line_check: the generated assist line against the disc's own 32
 * authored licence ribbons (National A and B, USA). This is the gate for
 * Stage B: if the generated line is not within a few metres of the authored
 * one, and does not mark the same braking stretches, it is not an assist
 * line, it is decoration.
 *
 *   assist_line_check <disc.iso> [--negative]
 *
 * --negative corrupts the generated line on purpose and must fail.
 */
#include "assist_line.h"
#include "career_license_disc.h"
#include "career_race_lookup.h"
#include "car.h"
#include "course.h"
#include "model.h"
#include "vol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEAN_LIMIT 5.0
#define OVERLAP_LIMIT 0.70

static float seg_dist2(float px, float pz, const float a[3], const float b[3])
{
    float vx = b[0] - a[0], vz = b[2] - a[2];
    float wx = px - a[0], wz = pz - a[2];
    float len2 = vx * vx + vz * vz;
    float t = len2 > 1e-8f ? (wx * vx + wz * vz) / len2 : 0;
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    float dx = px - (a[0] + t * vx), dz = pz - (a[2] + t * vz);
    return dx * dx + dz * dz;
}

static int authored_red(const gt4_vertex *v)
{
    return v->rgb[0] > .5f && v->rgb[0] > v->rgb[2];
}

static void metrics(const gt4_model *authored, const gt4_assist_line *line, int negative,
                    double *mean, double *worst, double *overlap, int *red_count)
{
    double sum = 0, high = 0;
    unsigned samples = 0, red_seen = 0, red_hit = 0;
    for (int t = 0; t < authored->count; t++) {
        for (int k = 0; k < 3; k++) {
            const gt4_vertex *v = &authored->triangles[t].v[k];
            float px = v->p[0] + (negative ? 12.f : 0.f), pz = v->p[2];
            float best = 1e30f;
            int near = 0;
            for (int i = 0; i + 1 < line->count; i++) {
                float ax = line->points[i][0] - line->points[i + 1][0];
                float az = line->points[i][2] - line->points[i + 1][2];
                if (ax * ax + az * az > 250000.f)
                    continue; /* a real section jump (>500 m), not an AI line segment */
                float d = seg_dist2(px, pz, line->points[i], line->points[i + 1]);
                if (d < best) {
                    best = d;
                    near = i;
                }
            }
            float m = sqrtf(best);
            sum += m;
            if (m > high)
                high = m;
            samples++;
            if (authored_red(v)) {
                red_seen++;
                if (line->red[near] || (near + 1 < line->count && line->red[near + 1]))
                    red_hit++;
            }
        }
    }
    *mean = samples ? sum / samples : 0;
    *worst = high;
    *overlap = red_seen ? (double)red_hit / red_seen : 1.0;
    *red_count = (int)red_seen;
}

static int check_one(gt4_vol *vol, const char *iso, const gt4_career_race_lookup_data *lookup,
                     const char *code, int negative, double *mean, double *worst,
                     double *overlap, int *red_count, int *authored_tris, int *line_points,
                     double *build_ms, char course_name[64])
{
    gt4_career_license_exam exam;
    char error[256] = "";
    if (!gt4_career_license_prepare(&exam, iso, lookup, code, error, sizeof error))
        return 0;
    (void)error;
    snprintf(course_name, 64, "%s", exam.desc.course);
    char path[128];
    snprintf(path, sizeof path, "/crs/%s", exam.desc.course);
    size_t size = 0;
    uint8_t *bytes = gt4_vol_read(vol, path, &size);
    if (!bytes)
        return 0;
    gt4_course course;
    int ok = gt4_course_open(&course, bytes, size);
    if (!ok) {
        free(bytes);
        return 0;
    }

    /* The AI line lives in the guest's lines object, so stand the course's
       runway and .ad blocks in a private 32 MB guest image, build the lines
       exactly as the race does, then export the active line. */
    char adpath[128];
    snprintf(adpath, sizeof adpath, "/crs/%s.ad", exam.desc.course);
    size_t an = 0;
    uint8_t *adf = gt4_vol_read(vol, adpath, &an);
    uint8_t *ram = calloc(0x02000000u, 1);
    if (!adf || !ram || an < 24) {
        free(bytes);
        free(adf);
        free(ram);
        return 0;
    }
    gt4_phys_mem = ram;
    enum {
        COURSE_AT = 0x00100000u,
        DATA_AT = 0x00110000u,
        RW_AT = 0x00300000u,
        AD_AT = 0x01000000u,
        LINES_AT = 0x01800000u
    };
    uint32_t rw_size = (uint32_t)(bytes[course.runway + 8] | bytes[course.runway + 9] << 8 |
                                  bytes[course.runway + 10] << 16 |
                                  bytes[course.runway + 11] << 24);
    if (rw_size == 0 || RW_AT + rw_size > AD_AT || AD_AT + an > LINES_AT) {
        gt4_phys_mem = NULL;
        free(bytes);
        free(adf);
        free(ram);
        return 0;
    }
    memcpy(gt4_ptr(RW_AT), bytes + course.runway, rw_size);
    memcpy(gt4_ptr(AD_AT), adf, an);
    gt4_runway_relocate(RW_AT);
    GT4_U32(COURSE_AT + 4) = DATA_AT;
    GT4_U32(DATA_AT + 0xdc) = RW_AT;
    for (uint32_t k = 0; k < 5; k++) {
        uint32_t off = (uint32_t)adf[4 + k * 4] | (uint32_t)adf[5 + k * 4] << 8 |
                       (uint32_t)adf[6 + k * 4] << 16 | (uint32_t)adf[7 + k * 4] << 24;
        GT4_U32(COURSE_AT + 0x9c + k * 4) = off ? AD_AT + off : 0;
    }
    /* Lines_setup would recompute every point's distance from the course's
       kd tree, which this probe does not build. Relocate the authored line
       block the way lines.c does, then give points a monotonic distance for
       line_at's segment search. Both authored formats are handled: the wide
       ADLN block and the older fixed-point licence format. */
    uint32_t adln = GT4_U32(COURSE_AT + 0x9c);
    static const float pi = 3.14159265358979f;
    int wide = GT4_U32(adln) == 0x4e4c4441u;
    int entries;
    uint32_t table;
    size_t stride;
    if (wide) {
        entries = (int)GT4_U32(adln + 0x14);
        stride = 0x40;
        if (!GT4_U32(adln + 4)) {
            GT4_U32(adln + 4) = adln;
            GT4_U32(adln + 0x18) += adln;
            for (int k = 0; k < entries; k++) {
                uint32_t e = GT4_U32(adln + 0x18) + (uint32_t)k * 4;
                if (!GT4_U32(e))
                    continue;
                uint32_t line = GT4_U32(adln + 4) + GT4_U32(e);
                GT4_U32(e) = line;
                int32_t n = (int32_t)GT4_U32(line);
                if (!n) {
                    GT4_U32(e) = 0;
                    continue;
                }
                for (uint32_t p = line + 0x40; n > 0; n--, p += 0x40) {
                    GT4_F32(p + 4) = -GT4_F32(p + 4);
                    GT4_F32(p + 8) = -GT4_F32(p + 8);
                    float h = GT4_F32(p + 0x24);
                    uint32_t bits;
                    memcpy(&bits, &h, 4);
                    if ((bits & 0x7f800000u) == 0x7f800000u)
                        bits = (bits & 0x80000000u) | 0x7f7fffffu;
                    memcpy(&h, &bits, 4);
                    GT4_F32(p + 0x24) = h + pi;
                }
            }
        }
        table = GT4_U32(adln + 0x18);
        GT4_U32(LINES_AT) = COURSE_AT;
        GT4_U32(LINES_AT + 4) = GT4_U32(adln + 0x14);
        GT4_U32(LINES_AT + 8) = table;
    } else {
        /* legacy: the negative count marks the block, entries are at +8,
           points start at +4 and stride 0x28, and x/z are 1/4096 fixed */
        entries = 8;
        stride = 0x28;
        if ((int32_t)GT4_U32(adln) >= 0) {
            GT4_U32(adln) = 0u - GT4_U32(adln);
            for (int k = 0; k < entries; k++) {
                uint32_t e = adln + 8 + (uint32_t)k * 4;
                if (!GT4_U32(e))
                    continue;
                uint32_t line = adln + GT4_U32(e);
                GT4_U32(e) = line;
                int32_t n = (int32_t)GT4_U32(line);
                if (!n) {
                    GT4_U32(e) = 0;
                    continue;
                }
                for (uint32_t p = line + 4; n > 0; n--, p += 0x28) {
                    GT4_F32(p + 4) = -(float)(int32_t)GT4_U32(p + 4) * (1.f / 4096.f);
                    GT4_F32(p + 8) = -(float)(int32_t)GT4_U32(p + 8) * (1.f / 4096.f);
                    GT4_F32(p + 0x18) =
                        (float)(int32_t)GT4_U32(p + 0x18) * (1.f / 4096.f);
                    GT4_F32(p + 0x24) = ((float)(int32_t)GT4_U32(p + 0x24) * 6.28318530718f) *
                                            (1.f / 4096.f) +
                                        pi;
                }
            }
        }
        table = adln + 8;
        GT4_U32(LINES_AT) = COURSE_AT;
        GT4_U32(LINES_AT + 4) = GT4_U32(adln + 4);
        GT4_U32(LINES_AT + 8) = table;
    }
    GT4_U16(LINES_AT + 0xc) = 1;
    float line_len = 0;
    for (int k = 0; k < entries; k++) {
        uint32_t e = table + (uint32_t)k * 4;
        uint32_t line = GT4_U32(e);
        int32_t n = line ? (int32_t)GT4_U32(line) : 0;
        if (n < 2)
            continue;
        uint32_t start = line + (uint32_t)(wide ? 0x40 : 4);
        float d = 0;
        for (int i = 0; i < n; i++) {
            uint32_t a = start + (uint32_t)i * (uint32_t)stride,
                     b = start + (uint32_t)((i + 1) % n) * (uint32_t)stride;
            GT4_F32(a + 0x14) = d;
            uint8_t kind = GT4_U8(a);
            float dx = GT4_F32(b + 4) - GT4_F32(a + 4),
                  dy = GT4_F32(b + 8) - GT4_F32(a + 8);
            /* Segment selection only needs a monotonic distance. Use the
               arc's own chord and radius (no heading wrap): this keeps the
               sampling even along a long corner. */
            float chord = sqrtf(dx * dx + dy * dy), seg = chord;
            float r = fabsf(GT4_F32(a + 0x18));
            if ((uint8_t)(kind - 1) < 2 && r > chord * .5f && chord > 1e-3f)
                seg = r * 2.f * asinf(fminf(1.f, chord / (2.f * r)));
            d += seg;
        }
        if (d > line_len)
            line_len = d;
    }
    if (line_len > 1)
        GT4_F32(RW_AT + 0x14) = line_len;
    enum { AI_MAX = 16384 };
    gt4_ai_line_point *points = calloc(AI_MAX, sizeof *points);
    int active = -1;
    int npoints = points ? gt4_ai_line_export(LINES_AT, 6, points, AI_MAX, &active) : 0;
    gt4_assist_line line;
    int built = npoints > 1 && gt4_assist_line_build(&course, points, npoints, &line);
    (void)active;
    free(points);
    gt4_phys_mem = NULL;
    free(adf);
    free(ram);

    if (!built) {
        free(bytes);
        return 0;
    }
    free(bytes);
    *line_points = line.count;
    *build_ms = line.build_ms;

    char authored_path[128];
    snprintf(authored_path, sizeof authored_path, "/crs/bestline/us%s", code);
    size = 0;
    bytes = gt4_vol_read(vol, authored_path, &size);
    if (!bytes) {
        gt4_assist_line_free(&line);
        return 0;
    }
    gt4_model_stats st;
    gt4_model *authored = gt4_model_decode_stats(bytes, size, -1, 0, &st);
    free(bytes);
    if (!authored) {
        gt4_assist_line_free(&line);
        return 0;
    }
    *authored_tris = authored->count;
    metrics(authored, &line, negative, mean, worst, overlap, red_count);
    gt4_model_free(authored);
    gt4_assist_line_free(&line);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <disc.iso> [--negative]\n", argv[0]);
        return 2;
    }
    int negative = argc > 2 && !strcmp(argv[2], "--negative");
    gt4_career_race_lookup_data lookup;
    if (!gt4_career_race_lookup_data_load(argv[1], &lookup)) {
        fprintf(stderr, "assist line: cannot read the race lookup from %s\n", argv[1]);
        return 2;
    }
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    if (!vol) {
        fprintf(stderr, "assist line: %s\n", err ? err : "cannot open the disc");
        return 2;
    }
    printf("code        course            authored  generated  build_ms  mean_m  worst_m  red_n  overlap  verdict\n");
    printf("----------  ----------------  --------  ---------  --------  ------  -------  -----  -------  -------\n");
    int failures = 0, covered = 0;
    for (const char *set = "ab"; *set; set++) {
        for (int n = 1; n <= 16; n++) {
            char code[32];
            snprintf(code, sizeof code, "l0%c%04d", *set, n);
            char course[64];
            double mean = 0, worst = 0, overlap = 0;
            int red = 0, tris = 0, points = 0;
            double build_ms = 0;
            if (!check_one(vol, argv[1], &lookup, code, negative, &mean, &worst, &overlap, &red, &tris,
                           &points, &build_ms, course)) {
                printf("%-10s  %-16s  %8s  %9s  %8s  %6s  %7s  %4s  %7s  %s\n", code, "?", "-", "-", "-", "-",
                       "-", "-", "-", "no authored line");
                continue;
            }
            covered++;
            int pass = mean < MEAN_LIMIT && (red == 0 || overlap > OVERLAP_LIMIT);
            if (!pass)
                failures++;
            printf("%-10s  %-16s  %8d  %9d  %8.2f  %6.2f  %7.2f  %5d  %6.1f%%  %s\n", code, course,
                   tris, points, build_ms, mean, worst, red, red ? 100.0 * overlap : 100.0,
                   pass ? "PASS" : "FAIL");
        }
    }
    gt4_vol_close(vol);
    printf("\n%d authored lines compared, %d failed. mean < %.0f m, brake overlap > %.0f%%.\n",
           covered, failures, MEAN_LIMIT, OVERLAP_LIMIT * 100);
    if (!covered)
        return 2;
    if (negative)
        return failures ? 0 : 1; /* the corrupted line must fail */
    return failures ? 1 : 0;
}
