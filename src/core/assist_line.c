/*
 * The assist line for every course: the runway carries an ordered path
 * (RNW4 +0x4e points at +0x84, 16 bytes each: x, y, z, tag). It follows one
 * edge of the road, so the generator first finds the drivable corridor around
 * it, then solves the minimum-curvature racing line inside that corridor.
 * That is the cheapest honest source: no triangle soup, no skeleton.
 *
 * The line is split at its authored section jumps, smoothed, given a
 * curvature speed profile (lateral grip in, braking and acceleration
 * limits), and turned into a 0.15 m ribbon 0.30 m above the road.
 */
#include "assist_line.h"

#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define LINE_HALF_WIDTH 0.075f
#define LINE_LIFT 0.30f
#define LINE_MAX_JUMP 20.f
#define LINE_MAX_POINTS 12000
#define LINE_LATERAL_GRIP 12.0f /* m/s^2, the cornering limit */
#define LINE_BRAKE 9.0f         /* m/s^2, service braking */
#define LINE_ACCEL 5.0f         /* m/s^2, out of a corner */
#define LINE_MAX_SPEED 80.0f    /* m/s, 288 km/h */
#define LINE_MAX_LATERAL 20.0f  /* m, the widest road we probe from the centreline */
#define LINE_PROBE_STEP 0.5f
#define LINE_RACING_PASSES 80
#define LINE_ROAD_TOL 1.5f      /* m, stay on the same road surface, not a bridge */

typedef struct {
    int first, count;
} run_span;

static float dist2(const float a[3], const float b[3])
{
    float dx = a[0] - b[0], dz = a[2] - b[2];
    return dx * dx + dz * dz;
}

static void colour(gt4_vertex *v, int red, int blue)
{
    float r = .65f, g = .88f, b = .66f;
    if (red) {
        r = 244.f / 255.f;
        g = 36.f / 255.f;
        b = 61.f / 255.f;
    } else if (blue) {
        r = 0;
        g = 0;
        b = 1;
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

/* Fill the braking/acceleration flags from a curvature speed profile. */
static void profile(int count, const float (*p)[3], unsigned char *red, unsigned char *blue)
{
    float *v = calloc((size_t)count, sizeof *v), *radius = calloc((size_t)count, sizeof *radius);
    if (!v || !radius) {
        free(v);
        free(radius);
        return;
    }
    for (int i = 0; i < count; i++)
        radius[i] = 1e6f;
    for (int i = 1; i + 1 < count; i++) {
        float ax = p[i][0] - p[i - 1][0], az = p[i][2] - p[i - 1][2];
        float bx = p[i + 1][0] - p[i][0], bz = p[i + 1][2] - p[i][2];
        float la = sqrtf(ax * ax + az * az), lb = sqrtf(bx * bx + bz * bz);
        if (la < 1e-4f || lb < 1e-4f)
            continue;
        float cross = ax * bz - az * bx, dot = ax * bx + az * bz;
        float angle = fabsf(atan2f(cross, dot));
        float curvature = angle / ((la + lb) * .5f);
        radius[i] = curvature > 1e-6f ? 1.f / curvature : 1e6f;
    }
    radius[0] = radius[count > 1 ? 1 : 0];
    radius[count - 1] = radius[count > 2 ? count - 2 : 0];
    /* Curvature from a discrete line is noisy: a one-point kink would flip
       red/blue every metre. Smooth the radius before the speed profile. */
    float *filtered = calloc((size_t)count, sizeof *filtered);
    if (filtered) {
        for (int pass = 0; pass < 3; pass++) {
            memcpy(filtered, radius, (size_t)count * sizeof *filtered);
            for (int i = 1; i + 1 < count; i++)
                radius[i] = (filtered[i - 1] + 2.f * filtered[i] + filtered[i + 1]) * .25f;
        }
        free(filtered);
    }
    for (int i = 0; i < count; i++) {
        float corner = sqrtf(LINE_LATERAL_GRIP * radius[i]);
        v[i] = corner < LINE_MAX_SPEED ? corner : LINE_MAX_SPEED;
    }
    for (int i = count - 2; i >= 0; i--) {
        float ax = p[i + 1][0] - p[i][0], az = p[i + 1][2] - p[i][2];
        float ds = sqrtf(ax * ax + az * az);
        float limit = sqrtf(v[i + 1] * v[i + 1] + 2 * LINE_BRAKE * ds);
        if (limit < v[i])
            v[i] = limit;
    }
    for (int i = 1; i < count; i++) {
        float ax = p[i][0] - p[i - 1][0], az = p[i][2] - p[i - 1][2];
        float ds = sqrtf(ax * ax + az * az);
        float limit = sqrtf(v[i - 1] * v[i - 1] + 2 * LINE_ACCEL * ds);
        if (limit < v[i])
            v[i] = limit;
    }
    /* A braking zone is where the speed envelope demands real deceleration
       over a segment, not every tiny numeric wobble. Same budget, applied
       over distance: a = (v0^2 - v1^2) / 2s. */
    for (int i = 0; i < count; i++) {
        red[i] = blue[i] = 0;
        int next = i + 1 < count ? i + 1 : i;
        float ax = p[next][0] - p[i][0], az = p[next][2] - p[i][2];
        float ds = sqrtf(ax * ax + az * az);
        if (ds < 1e-4f)
            continue;
        float decel = (v[i] * v[i] - v[next] * v[next]) / (2.f * ds);
        if (decel > 2.f)
            red[i] = 1;
        else if (decel < -1.f)
            blue[i] = 1;
    }
    free(v);
    free(radius);
}

/* Road height at a point, on the same surface as the point's own road: the
   ceiling finds the road under the authored path even where it sits low. */
static int line_road(const gt4_course_surface *surface, float x, float z, float base, float *y)
{
    float road, normal[3];
    if (!surface || !gt4_course_surface_at(surface, x, z, base + 2.f, &road, normal))
        return 0;
    if (fabsf(road - base) > LINE_ROAD_TOL)
        return 0;
    if (y)
        *y = road;
    return 1;
}

/* The classic minimum-curvature pass: relax the line under the biharmonic
   (shortest-curvature) stencil, then project each point back into the road
   corridor along its normal. Bounds come from probing the drivable surface,
   so the offset can never leave the road. The path table follows one edge of
   the road, so the corridor's middle is the reference line. */
static void racing_line(int count, int runs, const int *run, float (*p)[3],
                        const gt4_course_surface *surface)
{
    (void)runs;
    if (!surface || count < 6)
        return;
    float *nx = calloc((size_t)count, sizeof *nx), *nz = calloc((size_t)count, sizeof *nz);
    float *lo = calloc((size_t)count, sizeof *lo), *hi = calloc((size_t)count, sizeof *hi);
    float *base = calloc((size_t)count, sizeof *base), *mid = calloc((size_t)count, sizeof *mid);
    float(*ref)[3] = calloc((size_t)count, sizeof *ref);
    if (!nx || !nz || !lo || !hi || !base || !mid || !ref) {
        free(nx); free(nz); free(lo); free(hi); free(base); free(mid); free(ref);
        return;
    }
    memcpy(ref, p, (size_t)count * sizeof *ref);
    for (int i = 0; i < count; i++) {
        int a = i > 0 && run[i - 1] == run[i] ? i - 1 : i;
        int b = i + 1 < count && run[i + 1] == run[i] ? i + 1 : i;
        float dx = p[b][0] - p[a][0], dz = p[b][2] - p[a][2];
        float len = sqrtf(dx * dx + dz * dz);
        if (len < 1e-4f) {
            nx[i] = 0;
            nz[i] = 0;
        } else {
            nx[i] = dz / len;
            nz[i] = -dx / len;
        }
        float road, normal[3];
        base[i] = p[i][1];
        if (gt4_course_surface_at(surface, p[i][0], p[i][2], p[i][1] + 20.f, &road, normal))
            base[i] = road;
        if (!nx[i] && !nz[i])
            continue;
        for (float d = LINE_PROBE_STEP; d <= LINE_MAX_LATERAL + 1e-3f; d += LINE_PROBE_STEP) {
            if (!line_road(surface, p[i][0] - d * nx[i], p[i][2] - d * nz[i], base[i], NULL))
                break;
            lo[i] = -d;
        }
        for (float d = LINE_PROBE_STEP; d <= LINE_MAX_LATERAL + 1e-3f; d += LINE_PROBE_STEP) {
            if (!line_road(surface, p[i][0] + d * nx[i], p[i][2] + d * nz[i], base[i], NULL))
                break;
            hi[i] = d;
        }
    }
    /* Probe results wobble at kerbs and seams; smooth the corridor before
       using its middle. */
    float *tmp = calloc((size_t)count, sizeof *tmp);
    if (tmp) {
        for (int pass = 0; pass < 3; pass++) {
            for (int i = 0; i < count; i++) {
                int a = i > 0 && run[i - 1] == run[i] ? i - 1 : i;
                int b = i + 1 < count && run[i + 1] == run[i] ? i + 1 : i;
                tmp[i] = (lo[a] + 2.f * lo[i] + lo[b]) * .25f;
            }
            memcpy(lo, tmp, (size_t)count * sizeof *tmp);
            for (int i = 0; i < count; i++) {
                int a = i > 0 && run[i - 1] == run[i] ? i - 1 : i;
                int b = i + 1 < count && run[i + 1] == run[i] ? i + 1 : i;
                tmp[i] = (hi[a] + 2.f * hi[i] + hi[b]) * .25f;
            }
            memcpy(hi, tmp, (size_t)count * sizeof *tmp);
        }
        free(tmp);
    }
    for (int i = 0; i < count; i++) {
        mid[i] = (nx[i] || nz[i]) ? (lo[i] + hi[i]) * .5f : 0;
        lo[i] -= mid[i];
        hi[i] -= mid[i];
        p[i][0] = ref[i][0] + mid[i] * nx[i];
        p[i][2] = ref[i][2] + mid[i] * nz[i];
    }
    float(*next)[3] = calloc((size_t)count, sizeof *next);
    if (!next) {
        free(nx); free(nz); free(lo); free(hi); free(base); free(mid); free(ref);
        return;
    }
    memcpy(next, p, (size_t)count * sizeof *next);
    for (int pass = 0; pass < LINE_RACING_PASSES; pass++) {
        for (int k = 0; k < count; k++) {
            int i = (pass & 1) ? count - 1 - k : k;
            if (i < 2 || i + 2 >= count)
                continue;
            if (run[i - 2] != run[i] || run[i - 1] != run[i] || run[i + 1] != run[i] ||
                run[i + 2] != run[i])
                continue;
            float px = (4.f * (p[i - 1][0] + p[i + 1][0]) - p[i - 2][0] - p[i + 2][0]) / 6.f;
            float pz = (4.f * (p[i - 1][2] + p[i + 1][2]) - p[i - 2][2] - p[i + 2][2]) / 6.f;
            float bx = ref[i][0] + mid[i] * nx[i], bz = ref[i][2] + mid[i] * nz[i];
            float off = (px - bx) * nx[i] + (pz - bz) * nz[i];
            off = off < lo[i] ? lo[i] : off > hi[i] ? hi[i] : off;
            float cx = bx + off * nx[i], cz = bz + off * nz[i];
            for (int shrink = 0; shrink < 8 && !line_road(surface, cx, cz, base[i], NULL);
                 shrink++) {
                off *= .5f;
                cx = bx + off * nx[i];
                cz = bz + off * nz[i];
            }
            if (line_road(surface, cx, cz, base[i], NULL)) {
                next[i][0] = p[i][0] + .5f * (cx - p[i][0]);
                next[i][2] = p[i][2] + .5f * (cz - p[i][2]);
                next[i][1] = p[i][1];
            } else {
                next[i][0] = p[i][0];
                next[i][1] = p[i][1];
                next[i][2] = p[i][2];
            }
        }
        memcpy(p, next, (size_t)count * sizeof *next);
    }
    free(next);
    free(nx); free(nz); free(lo); free(hi); free(base); free(mid); free(ref);
}

int gt4_assist_line_build(const gt4_course *course, gt4_assist_line *out)
{
    if (!out)
        return 0;
    memset(out, 0, sizeof *out);
    if (!course || !course->data || course->path_count < 2 || course->path_count > LINE_MAX_POINTS)
        return 0;

    clock_t started = clock();
    int n = course->path_count;
    float(*raw)[3] = calloc((size_t)n, sizeof *raw);
    int *run = calloc((size_t)n, sizeof *run), *keep = calloc((size_t)n, sizeof *keep);
    if (!raw || !run || !keep) {
        free(raw);
        free(run);
        free(keep);
        return 0;
    }
    int kept = 0, runs = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t *e = course->data + course->path + (size_t)i * 16;
        uint32_t tag;
        memcpy(&raw[i][0], e, 4);
        memcpy(&raw[i][1], e + 4, 4);
        memcpy(&raw[i][2], e + 8, 4);
        memcpy(&tag, e + 12, 4);
        if (tag == 65536)
            continue; /* the empty marker between authored sections */
        if (kept && dist2(raw[i], raw[keep[kept - 1]]) > LINE_MAX_JUMP * LINE_MAX_JUMP)
            runs++;
        run[kept] = runs;
        keep[kept++] = i;
    }
    if (kept < 2) {
        free(raw);
        free(run);
        free(keep);
        return 0;
    }
    runs++;

    /* Smooth each authored section on its own; never bridge a jump. */
    float(*smooth)[3] = calloc((size_t)kept, sizeof *smooth);
    if (!smooth) {
        free(raw);
        free(run);
        free(keep);
        return 0;
    }
    memcpy(smooth, raw, (size_t)kept * sizeof *smooth);
    for (int pass = 0; pass < 2; pass++) {
        float(*next)[3] = calloc((size_t)kept, sizeof *next);
        if (!next)
            break;
        memcpy(next, smooth, (size_t)kept * sizeof *next);
        for (int i = 0; i < kept; i++) {
            if ((i && run[i - 1] != run[i]) || (i + 1 < kept && run[i + 1] != run[i]))
                continue;
            if (i == 0 || i + 1 >= kept)
                continue;
            for (int k = 0; k < 3; k++)
                next[i][k] = (smooth[i - 1][k] + 2 * smooth[i][k] + smooth[i + 1][k]) * .25f;
        }
        memcpy(smooth, next, (size_t)kept * sizeof *smooth);
        free(next);
    }

    gt4_course_surface *surface = gt4_course_surface_create(course);
    racing_line(kept, runs, run, smooth, surface);

    unsigned char *red = calloc((size_t)kept, 1), *blue = calloc((size_t)kept, 1);
    if (!red || !blue) {
        free(raw);
        free(run);
        free(keep);
        free(smooth);
        free(red);
        free(blue);
        gt4_course_surface_free(surface);
        return 0;
    }
    int start = 0;
    for (int r = 0; r <= runs; r++) {
        int end = start;
        while (end < kept && run[end] == r)
            end++;
        if (end - start >= 3)
            profile(end - start, smooth + start, red + start, blue + start);
        start = end;
    }
    /* The authored path sits lower than the road in places; the ribbon is
       0.30 m above the road surface, not above the path's own elevation. */
    for (int i = 0; i < kept; i++) {
        float road, normal[3];
        if (surface &&
            gt4_course_surface_at(surface, smooth[i][0], smooth[i][2], smooth[i][1] + 20.f, &road,
                                  normal) &&
            road > smooth[i][1])
            smooth[i][1] = road;
        smooth[i][1] += LINE_LIFT;
    }
    gt4_course_surface_free(surface);

    gt4_model *m = calloc(1, sizeof *m);
    if (!m) {
        free(raw);
        free(run);
        free(keep);
        free(smooth);
        free(red);
        free(blue);
        return 0;
    }
    m->capacity = (kept - runs) * 2;
    m->triangles = calloc((size_t)m->capacity, sizeof *m->triangles);
    if (!m->triangles) {
        free(m);
        free(raw);
        free(run);
        free(keep);
        free(smooth);
        free(red);
        free(blue);
        return 0;
    }
    for (int i = 0; i + 1 < kept; i++) {
        if (run[i] != run[i + 1])
            continue;
        float dx = smooth[i + 1][0] - smooth[i][0], dz = smooth[i + 1][2] - smooth[i][2];
        float len = sqrtf(dx * dx + dz * dz);
        if (len < 1e-4f)
            continue;
        dx /= len;
        dz /= len;
        float rx = dz * LINE_HALF_WIDTH, rz = -dx * LINE_HALF_WIDTH;
        float l0[3] = {smooth[i][0] - rx, smooth[i][1], smooth[i][2] - rz};
        float r0[3] = {smooth[i][0] + rx, smooth[i][1], smooth[i][2] + rz};
        float l1[3] = {smooth[i + 1][0] - rx, smooth[i + 1][1], smooth[i + 1][2] - rz};
        float r1[3] = {smooth[i + 1][0] + rx, smooth[i + 1][1], smooth[i + 1][2] + rz};
        gt4_triangle *t = &m->triangles[m->count];
        memcpy(t->v[0].p, l0, sizeof l0);
        memcpy(t->v[1].p, r0, sizeof r0);
        memcpy(t->v[2].p, r1, sizeof r1);
        colour(&t->v[0], red[i], blue[i]);
        colour(&t->v[1], red[i], blue[i]);
        colour(&t->v[2], red[i + 1], blue[i + 1]);
        t->texture = -1;
        /* FBMSK: a set bit masks that channel out. 0xffffffff masked every
           colour away and still wrote depth, so the line was an invisible
           occluder cutting through the car. Write the vertex colours and keep
           the ribbon out of the depth buffer (it must never hide the car). */
        t->state.color_mask = 0;
        t->state.depth_test = 1;
        t->state.depth_write = 0;
        t->state.depth_func = 1; /* ALWAYS with depth-write off: the ribbon
                                    shows on the road and can never cut the car */
        t->state.cull = 0;
        m->count++;
        t = &m->triangles[m->count];
        memcpy(t->v[0].p, l0, sizeof l0);
        memcpy(t->v[1].p, r1, sizeof r1);
        memcpy(t->v[2].p, l1, sizeof l1);
        colour(&t->v[0], red[i], blue[i]);
        colour(&t->v[1], red[i + 1], blue[i + 1]);
        colour(&t->v[2], red[i + 1], blue[i + 1]);
        t->texture = -1;
        t->state = m->triangles[0].state;
        m->count++;
    }
    free(run);
    free(keep);
    free(raw);
    if (!m->count) {
        gt4_model_free(m);
        free(smooth);
        free(red);
        free(blue);
        return 0;
    }
    out->model = m;
    out->count = kept;
    out->triangles = m->count;
    out->points = smooth;
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
