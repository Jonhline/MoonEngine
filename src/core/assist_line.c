/*
 * The assist line, built from the game's own AI course line.
 *
 * The driver model already carries the authored ADLN line in memory
 * (src/core/physics/ai.c: the AI's racing line, also run for the player).
 * That is the only authored per-course source on the disc: the RNW4 path is
 * the road edge, not a racing line. This file reads the exported points and
 * turns them into one continuous 0.15 m ribbon 0.30 m above the road.
 *
 * Colour comes from the line's own speed trim and arc radius: red where the
 * AI brakes, blue where it accelerates. Draw state follows the licence
 * ribbon: unlit vertex colours, LEQUAL depth, no depth write, so the car and
 * anything in front hide it.
 */
#include "assist_line.h"

#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define LINE_HALF_WIDTH 0.075f
#define LINE_LIFT 0.03f
#define LINE_LOOP_CLOSE 30.f /* m: close the lap when the ends already meet */
#define LINE_LATERAL_GRIP 12.0f /* m/s^2, the cornering limit */
#define LINE_BRAKE 9.0f         /* m/s^2, service braking */
#define LINE_ACCEL 5.0f         /* m/s^2, out of a corner */
#define LINE_MAX_SPEED 80.0f    /* m/s, 288 km/h */
#define LINE_RED_RADIUS 280.f   /* m: a tighter arc is a braking corner */

/* The authored licence ribbon (/crs/bestline/usl0a0007) carries the GT
   blue and red as GS colours in units of 128: (0, 0, 255) and (255, 0, 0).
   Counted from that mesh, not picked by eye. */
#define LINE_BLUE_R 0.0f
#define LINE_BLUE_G 0.0f
#define LINE_BLUE_B (255.f / 128.f)
#define LINE_RED_R (255.f / 128.f)
#define LINE_RED_G 0.0f
#define LINE_RED_B 0.0f

static void colour(gt4_vertex *v, int red, int blue)
{
    (void)blue;
    float r = LINE_BLUE_R, g = LINE_BLUE_G, b = LINE_BLUE_B;
    if (red) {
        r = LINE_RED_R;
        g = LINE_RED_G;
        b = LINE_RED_B;
    }
    v->rgb[0] = r;
    v->rgb[1] = g;
    v->rgb[2] = b;
    v->normal[0] = 0;
    v->normal[1] = 1;
    v->normal[2] = 0;
    v->uv[0] = v->uv[1] = 0;
    v->alpha = 1;
}

/* The authored line is the AI's driving line, which is not always on the
   tarmac. Probe the road across the ribbon and, when a sample sits off it,
   pull that sample to the middle of the nearest run of road. The height is
   re-read there, so the correction never leaves the surface either. */
#define LINE_ROAD_SEARCH 15.0f /* m either side of the AI point */
#define LINE_ROAD_STEP .5f      /* m between probes */
#define LINE_ROAD_TOL 2.0f      /* m: height difference still the same road */
#define LINE_ROAD_EDGE .75f     /* m kept inside the road edge */

static int road_at(const gt4_course_surface *s, float x, float z, float ref, float *y)
{
    if (!s)
        return 0;
    float road, normal[3];
    if (!gt4_course_surface_at(s, x, z, ref + LINE_ROAD_TOL, &road, normal))
        return 0;
    if (!isfinite(road) || fabsf(road - ref) > LINE_ROAD_TOL)
        return 0;
    *y = road;
    return 1;
}

static int lateral_centre(const gt4_course_surface *s, float x, float z, float ref, float rx,
                          float rz, float *out_x, float *out_z, float *out_y)
{
    int steps = (int)(LINE_ROAD_SEARCH / LINE_ROAD_STEP);
    float y;
    if (road_at(s, x, z, ref, &y))
        return 0; /* already on the road: keep the AI's own line */
    int best_lo = 0, best_hi = 0, found = 0;
    float best_gap = 1e30f;
    for (int k = -steps; k <= steps; k++) {
        float t = (float)k * LINE_ROAD_STEP, yy;
        if (!road_at(s, x + rx * t, z + rz * t, ref, &yy))
            continue;
        int lo = k, hi = k;
        while (lo > -steps) {
            float u = (float)(lo - 1) * LINE_ROAD_STEP;
            if (!road_at(s, x + rx * u, z + rz * u, ref, &yy))
                break;
            lo--;
        }
        while (hi < steps) {
            float u = (float)(hi + 1) * LINE_ROAD_STEP;
            if (!road_at(s, x + rx * u, z + rz * u, ref, &yy))
                break;
            hi++;
        }
        float gap = k < 0 ? -(float)hi * LINE_ROAD_STEP : (float)lo * LINE_ROAD_STEP;
        if (gap < 0)
            gap = 0;
        if (!found || gap < best_gap) {
            found = 1;
            best_gap = gap;
            best_lo = lo;
            best_hi = hi;
        }
        k = hi; /* the run's other samples are the same road */
    }
    if (!found || best_lo == best_hi)
        return 0;
    float lo = (float)best_lo * LINE_ROAD_STEP, hi = (float)best_hi * LINE_ROAD_STEP;
    float centre = (lo + hi) * .5f;
    float edge = LINE_ROAD_EDGE;
    if (hi - lo < edge * 2.f)
        edge = (hi - lo) * .25f;
    if (centre < lo + edge)
        centre = lo + edge;
    if (centre > hi - edge)
        centre = hi - edge;
    float nx = x + rx * centre, nz = z + rz * centre, ny;
    if (!road_at(s, nx, nz, ref, &ny))
        return 0;
    *out_x = nx;
    *out_z = nz;
    *out_y = ny;
    return 1;
}

/* The AI line's own speed envelope: the point's arc radius gives the target
   speed, then the braking/acceleration passes mark where it slows and where
   it gets back on the power. Same budgets as the old generator; the source
   is now the game's line, not a curvature guess on a road edge. */
static void profile(int count, const gt4_ai_line_point *points, int closed,
                    unsigned char *red, unsigned char *blue)
{
    float *v = calloc((size_t)count, sizeof *v);
    if (!v)
        return;
    for (int i = 0; i < count; i++) {
        if (points[i].speed_trim < 0)
            v[i] = LINE_MAX_SPEED * (1.f + (float)points[i].speed_trim / 100.f);
        else if (points[i].kind && fabsf(points[i].arc_radius) > .5f)
            v[i] = sqrtf(LINE_LATERAL_GRIP * fabsf(points[i].arc_radius));
        else
            v[i] = LINE_MAX_SPEED;
        if (v[i] > LINE_MAX_SPEED)
            v[i] = LINE_MAX_SPEED;
        if (v[i] < 5.f)
            v[i] = 5.f;
    }
    float *tmp = calloc((size_t)count, sizeof *tmp);
    if (tmp) {
        for (int pass = 0; pass < 3; pass++) {
            memcpy(tmp, v, (size_t)count * sizeof *tmp);
            for (int i = 0; i < count; i++) {
                int a = i > 0 ? i - 1 : count - 1, b = i + 1 < count ? i + 1 : 0;
                v[i] = (tmp[a] + 2.f * tmp[i] + tmp[b]) * .25f;
            }
        }
        free(tmp);
    }
    for (int i = count - 2; i >= 0; i--) {
        float dx = points[i + 1].x - points[i].x, dz = points[i + 1].z - points[i].z;
        float ds = sqrtf(dx * dx + dz * dz);
        float limit = sqrtf(v[i + 1] * v[i + 1] + 2.f * LINE_BRAKE * ds);
        if (limit < v[i])
            v[i] = limit;
    }
    for (int i = 1; i < count; i++) {
        float dx = points[i].x - points[i - 1].x, dz = points[i].z - points[i - 1].z;
        float ds = sqrtf(dx * dx + dz * dz);
        float limit = sqrtf(v[i - 1] * v[i - 1] + 2.f * LINE_ACCEL * ds);
        if (limit < v[i])
            v[i] = limit;
    }
    for (int i = 0; i < count; i++) {
        int next = i + 1 < count ? i + 1 : (closed ? 0 : i);
        red[i] = blue[i] = 0;
        float dx = points[next].x - points[i].x, dz = points[next].z - points[i].z;
        float ds = sqrtf(dx * dx + dz * dz);
        if (ds < 1e-4f)
            continue;
        float a = (v[i] * v[i] - v[next] * v[next]) / (2.f * ds);
        if (a > 2.f)
            red[i] = 1;
        else if (a < -1.f)
            blue[i] = 1;
        if (points[i].speed_trim < 0 ||
            (!blue[i] && points[i].kind && fabsf(points[i].arc_radius) > .5f &&
             fabsf(points[i].arc_radius) < LINE_RED_RADIUS)) {
            red[i] = 1;
            blue[i] = 0;
        }
    }
    free(v);
}

int gt4_assist_line_build(const gt4_course *course, const gt4_ai_line_point *points, int count,
                          gt4_assist_line *out)
{
    (void)course;
    if (!out)
        return 0;
    memset(out, 0, sizeof *out);
    if (!points || count < 2)
        return 0;

    clock_t started = clock();
    static int colours_reported;
    if (!colours_reported) {
        colours_reported = 1;
        fprintf(stderr, "assist: colours %.3f %.3f %.3f / %.3f %.3f %.3f (from usl0a0007)\n",
                LINE_BLUE_R, LINE_BLUE_G, LINE_BLUE_B, LINE_RED_R, LINE_RED_G, LINE_RED_B);
    }
    float(*p)[3] = calloc((size_t)count, sizeof *p);
    unsigned char *red = calloc((size_t)count, 1), *blue = calloc((size_t)count, 1);
    if (!p || !red || !blue) {
        free(p);
        free(red);
        free(blue);
        return 0;
    }
    /* Put every sample on the road under it: the AI line's own height can
       climb over bridges or hit a bad ground probe. The course surface is
       the native one, and the lift is the same 0.30 m as before. */
    gt4_course_surface *surface = course ? gt4_course_surface_create(course) : NULL;
    for (int i = 0; i < count; i++) {
        float y = points[i].y;
        float road, normal[3];
        if (surface && gt4_course_surface_at(surface, points[i].x, points[i].z, y + 5.f, &road,
                                            normal) &&
            fabsf(road - y) < 10.f)
            y = road;
        p[i][0] = points[i].x;
        p[i][1] = y + LINE_LIFT;
        p[i][2] = points[i].z;
    }

    /* Does the exported lap already meet at its ends? Then close it. */
    float dx = p[count - 1][0] - p[0][0], dz = p[count - 1][2] - p[0][2];
    int segments = count - 1;
    int closed = sqrtf(dx * dx + dz * dz) < LINE_LOOP_CLOSE;
    if (closed)
        segments = count; /* the last segment is count-1 -> 0 */

    /* Pull off-road samples back onto the tarmac before the ribbon is built. */
    int corrected = 0;
    if (surface) {
        for (int i = 0; i < count; i++) {
            int prev = i > 0 ? i - 1 : (closed ? count - 1 : i);
            int next = i + 1 < count ? i + 1 : (closed ? 0 : i);
            float tx = p[next][0] - p[prev][0], tz = p[next][2] - p[prev][2];
            float len = sqrtf(tx * tx + tz * tz);
            if (len < 1e-4f)
                continue;
            float nx, nz, ny;
            if (lateral_centre(surface, p[i][0], p[i][2], p[i][1] - LINE_LIFT, tz / len,
                               -tx / len, &nx, &nz, &ny)) {
                p[i][0] = nx;
                p[i][2] = nz;
                p[i][1] = ny + LINE_LIFT;
                corrected++;
            }
        }
        fprintf(stderr, "assist: %d of %d samples pulled onto the road\n", corrected, count);
    }
    gt4_course_surface_free(surface);
    profile(count, points, closed, red, blue);

    gt4_model *m = calloc(1, sizeof *m);
    if (!m) {
        free(p);
        free(red);
        free(blue);
        return 0;
    }
    m->capacity = (unsigned)segments * 2;
    m->triangles = calloc(m->capacity, sizeof *m->triangles);
    if (!m->triangles) {
        free(m);
        free(p);
        free(red);
        free(blue);
        return 0;
    }
    for (int i = 0; i < segments; i++) {
        int j = (i + 1) % count;
        float ax = p[j][0] - p[i][0], az = p[j][2] - p[i][2];
        float len = sqrtf(ax * ax + az * az);
        if (len < 1e-4f)
            continue;
        ax /= len;
        az /= len;
        float rx = az * LINE_HALF_WIDTH, rz = -ax * LINE_HALF_WIDTH;
        float l0[3] = {p[i][0] - rx, p[i][1], p[i][2] - rz};
        float r0[3] = {p[i][0] + rx, p[i][1], p[i][2] + rz};
        float l1[3] = {p[j][0] - rx, p[j][1], p[j][2] - rz};
        float r1[3] = {p[j][0] + rx, p[j][1], p[j][2] + rz};
        gt4_triangle *t = &m->triangles[m->count];
        memcpy(t->v[0].p, l0, sizeof l0);
        memcpy(t->v[1].p, r0, sizeof r0);
        memcpy(t->v[2].p, r1, sizeof r1);
        colour(&t->v[0], red[i], blue[i]);
        colour(&t->v[1], red[i], blue[i]);
        colour(&t->v[2], red[j], blue[j]);
        t->texture = -1;
        t->state.color_mask = 0;
        t->state.depth_test = 1;
        t->state.depth_write = 0;
        t->state.depth_func = 3; /* LEQUAL: the car and the road's furniture hide it */
        t->state.cull = 0;
        m->count++;
        t = &m->triangles[m->count];
        memcpy(t->v[0].p, l0, sizeof l0);
        memcpy(t->v[1].p, r1, sizeof r1);
        memcpy(t->v[2].p, l1, sizeof l1);
        colour(&t->v[0], red[i], blue[i]);
        colour(&t->v[1], red[j], blue[j]);
        colour(&t->v[2], red[j], blue[j]);
        t->texture = -1;
        t->state = m->triangles[0].state;
        m->count++;
    }
    if (!m->count) {
        gt4_model_free(m);
        free(p);
        free(red);
        free(blue);
        return 0;
    }
    out->model = m;
    out->count = count;
    out->triangles = m->count;
    out->points = p;
    out->red = red;
    out->blue = blue;
    out->build_ms = (double)(clock() - started) * 1000.0 / CLOCKS_PER_SEC;
    return 1;
}

void gt4_assist_line_free(gt4_assist_line *line)
{
    if (!line)
        return;
    gt4_model_free(line->model);
    free(line->points);
    free(line->red);
    free(line->blue);
    memset(line, 0, sizeof *line);
}
