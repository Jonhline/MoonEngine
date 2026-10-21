/* CRPM geometry: 003FECB0 / 003C9E70 / 003C9DB0. All offsets stay
   relative to the borrowed CRPM; the original pointer fixups are not written. */
static unsigned photo_u16(const uint8_t *p)
{
    return p[0] | (unsigned)p[1] << 8;
}
static const uint8_t *photo_polygon(const gt4_photo_location *p, unsigned bundle, unsigned polygon)
{
    if (!p || bundle >= p->cameras || !table(p->size, p->camera_table, p->cameras, 16))
        return NULL;
    const uint8_t *set = p->data + p->camera_table + bundle * 16;
    unsigned count = photo_u16(set + 2);
    uint32_t list = u32(set + 4);
    if (polygon >= count || !table(p->size, list, count, 4))
        return NULL;
    uint32_t at = u32(p->data + list + polygon * 4);
    if (!table(p->size, at, 1, 4))
        return NULL;
    uint32_t n = u32(p->data + at);
    return table(p->size, at + 4, n, 8) ? p->data + at : NULL;
}
static int photo_polygon_contains(const uint8_t *poly, const float xz[2])
{
    if (!poly)
        return 0;
    unsigned n = u32(poly);
    int inside = 0;
    for (unsigned i = 0, j = n ? n - 1 : 0; i < n; j = i++) {
        const uint8_t *a = poly + 4 + j * 8, *b = poly + 4 + i * 8;
        float ax = f32(a), az = f32(a + 4), bx = f32(b), bz = f32(b + 4);
        if (az == bz)
            continue;
        if (bz < az) {
            float t = ax;
            ax = bx;
            bx = t;
            t = az;
            az = bz;
            bz = t;
        }
        if (az <= xz[1] && xz[1] < bz) {
            float lhs = (xz[0] - ax) * (bz - az), rhs = (xz[1] - az) * (bx - ax);
            if (lhs - rhs > 0)
                inside ^= 1;
        }
    }
    return inside;
}
int gt4_photo_region_contains(const gt4_photo_location *p, unsigned bundle, const float xz[2])
{
    if (!p || bundle >= p->cameras || !table(p->size, p->camera_table, p->cameras, 16))
        return 0;
    const uint8_t *set = p->data + p->camera_table + bundle * 16;
    int inside = 0;
    for (unsigned i = 0; i < photo_u16(set + 2); i++) {
        const uint8_t *poly = photo_polygon(p, bundle, i);
        if (!poly)
            return 0;
        inside ^= photo_polygon_contains(poly, xz);
    }
    return inside;
}
int gt4_photo_region(const gt4_photo_location *p, float xyz[3], float radius)
{
    if (!p || !xyz || !isfinite(radius) || radius < 0 ||
        !table(p->size, p->region_table, p->regions, 64))
        return -1;
    xyz[1] = 0;
    float xz[2] = {xyz[0], xyz[2]};
    for (unsigned i = 0; i < p->regions; i++) {
        const uint8_t *r = p->data + p->region_table + i * 64;
        unsigned bundle = photo_u16(r + 4);
        if (u32(r) & 1) {
            const uint8_t *poly = photo_polygon(p, bundle, 0);
            if (!poly)
                continue;
            unsigned count = u32(poly);
            float x = 0, z = 0;
            for (unsigned k = 0; k < count; k++) {
                x += f32(poly + 4 + k * 8);
                z += f32(poly + 8 + k * 8);
            }
            if (count) {
                x /= count;
                z /= count;
            }
            float dx = x - xyz[0], dz = z - xyz[2];
            if (dx * dx + dz * dz < radius * radius) {
                xyz[0] = x;
                xyz[2] = z;
                return (int)i;
            }
        } else if (gt4_photo_region_contains(p, bundle, xz))
            return (int)i;
    }
    return -1;
}
/* 003FFC68, returned degrees. Original trig library is the oracle boundary. */
void gt4_photo_distance_fov(float distance, float out[2])
{
    float a = atanf(1 / ((2 * distance) * .0625f));
    a += a;
    a /= .01745329052209854f;
    out[0] = fmaxf(2, fminf(83.19999694824219f, a));
    out[1] = 83.19999694824219f;
}

void gt4_photo_contact_reset(gt4_photo_contact *s, const float xz[2])
{
    memset(s, 0, sizeof *s);
    memcpy(s->point, xz, 8);
    s->polygon = s->edge = s->vertex = -1;
}
static float photo_distance2(const float a[2], const float b[2])
{
    float x = a[0] - b[0], z = a[1] - b[1];
    return x * x + z * z;
}
static float photo_cross(const float a[2], const float b[2], const float c[2])
{
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]);
}
static void photo_vertex(const uint8_t *poly, unsigned i, float v[2])
{
    v[0] = f32(poly + 4 + i * 8);
    v[1] = f32(poly + 8 + i * 8);
}
static float photo_cosine(const float a[2], const float b[2], const float c[2])
{
    float x = b[0] - a[0], z = b[1] - a[1], u = c[0] - a[0], v = c[1] - a[1];
    float den = sqrtf(x * x + z * z) * sqrtf(u * u + v * v);
    return den > 0 ? (x * u + z * v) / den : 0;
}
/* 003C9050: project the remaining movement along the nearer-directed edge;
   retain its vertex and fractional remainder for at most four corner passes. */
static float photo_slide(gt4_photo_contact *s, const float a[2], const float b[2], int ia, int ib,
                         const float origin[2], const float target[2], float cosine)
{
    const float *end = b, *start = a;
    int vertex = ib;
    if (photo_cosine(origin, target, b) < photo_cosine(origin, target, a)) {
        end = a;
        start = b;
        vertex = ia;
    }
    float dx = end[0] - start[0], dz = end[1] - start[1],
          length2 = photo_distance2(origin, target) * cosine * cosine;
    float move[2];
    if (fabsf(dz) < fabsf(dx)) {
        float slope = dz / dx;
        move[0] = sqrtf(length2 / (slope * slope + 1));
        if (dx < 0)
            move[0] = -move[0];
        move[1] = move[0] * dz / dx;
    } else {
        float slope = dx / dz;
        move[1] = sqrtf(length2 / (slope * slope + 1));
        if (dz < 0)
            move[1] = -move[1];
        move[0] = move[1] * dx / dz;
    }
    s->kind = 1;
    s->point[0] = origin[0] + move[0];
    s->point[1] = origin[1] + move[1];
    s->vertex = -1;
    float moved = move[0] * move[0] + move[1] * move[1], to_end = photo_distance2(origin, end);
    if (to_end <= moved) {
        s->kind = 2;
        memcpy(s->point, end, 8);
        s->vertex = vertex;
        return moved > 0 ? (moved - to_end) / moved : 0;
    }
    return 0;
}
static float photo_clip(const gt4_photo_location *p, unsigned bundle, const float from[2],
                        const float target[2], gt4_photo_contact *s)
{
    int inside = gt4_photo_region_contains(p, bundle, target);
    if (s->kind == 2 && s->polygon >= 0) {
        const uint8_t *poly = photo_polygon(p, bundle, s->polygon);
        if (!poly)
            return 0;
        unsigned n = u32(poly);
        if (s->vertex < 0 || (unsigned)s->vertex >= n)
            return 0;
        float vertex[2], previous[2], next[2];
        unsigned i = s->vertex;
        photo_vertex(poly, i, vertex);
        photo_vertex(poly, (i + n - 1) % n, previous);
        photo_vertex(poly, (i + 1) % n, next);
        if (photo_cosine(vertex, target, previous) < photo_cosine(vertex, target, next))
            s->edge = (i + 1) % n;
        else
            s->edge = i;
    }
    if (s->kind && s->polygon >= 0 && s->edge >= 0) {
        const uint8_t *poly = photo_polygon(p, bundle, s->polygon);
        if (!poly)
            return 0;
        unsigned n = u32(poly), i = (unsigned)s->edge;
        if (!n || i >= n)
            return 0;
        float a[2], b[2];
        photo_vertex(poly, (i + n - 1) % n, a);
        photo_vertex(poly, i, b);
        if (photo_cross(a, target, b) > 0) {
            if (inside)
                gt4_photo_contact_reset(s, target);
            return 0;
        }
        float dx = b[0] - a[0], dz = b[1] - a[1], mx = target[0] - from[0],
              mz = target[1] - from[1];
        float len = sqrtf(dx * dx + dz * dz) * sqrtf(mx * mx + mz * mz);
        if (!(len > 0))
            return 0;
        float c = fabsf((dx * mx + dz * mz) / len);
        if (c < .29999998211860657f)
            return 0;
        float remainder = photo_slide(s, a, b, (i + n - 1) % n, i, from, target, c);
        s->hit = 1;
        s->distance = 0;
        s->angle = (1.570796251296997f - asinf(c)) * 180 / 3.141592502593994f;
        return remainder;
    }
    const uint8_t *set = p->data + p->camera_table + bundle * 16;
    int hit = 0;
    float best = -1;
    gt4_photo_contact result = *s;
    for (unsigned k = 0; k < photo_u16(set + 2); k++) {
        const uint8_t *poly = photo_polygon(p, bundle, k);
        if (!poly)
            return 0;
        unsigned n = u32(poly);
        for (unsigned i = 0, j = n ? n - 1 : 0; i < n; j = i++) {
            float a[2], b[2];
            photo_vertex(poly, j, a);
            photo_vertex(poly, i, b);
            /* Original uses strict straddling: touching endpoints is not a new crossing. */
            if (photo_cross(a, from, b) * photo_cross(a, target, b) >= 0 ||
                photo_cross(from, a, target) * photo_cross(from, b, target) >= 0)
                continue;
            float ex = b[0] - a[0], ez = b[1] - a[1], mx = target[0] - from[0],
                  mz = target[1] - from[1], x, z;
            if (ex == 0) {
                if (mx == 0)
                    continue;
                x = a[0];
                z = (mz / mx) * x + from[1] - from[0] * (mz / mx);
            } else if (mx == 0) {
                x = from[0];
                z = x * (ez / ex) + a[1] - a[0] * (ez / ex);
            } else {
                float e = ez / ex, m = mz / mx;
                if (e == m)
                    continue;
                x = ((from[1] - a[1]) - from[0] * m + a[0] * e) / (e - m);
                z = x * e + a[1] - a[0] * e;
            }
            float point[2] = {x, z}, distance = sqrtf(photo_distance2(point, from));
            if (hit && best < distance)
                continue;
            float den = sqrtf(ex * ex + ez * ez) * sqrtf(mx * mx + mz * mz);
            if (!(den > 0))
                continue;
            float c = fabsf((ex * mx + ez * mz) / den);
            gt4_photo_contact candidate = *s;
            photo_slide(&candidate, a, b, j, i, point, target, c);
            candidate.hit = 1;
            candidate.polygon = k;
            candidate.edge = i;
            candidate.distance = distance;
            candidate.angle = (1.570796251296997f - asinf(c)) * 180 / 3.141592502593994f;
            result = candidate;
            hit = 1;
            best = distance;
        }
    }
    if (hit) {
        *s = result;
        return 0;
    }
    if (inside)
        gt4_photo_contact_reset(s, target);
    return 0;
}
void gt4_photo_region_move(const gt4_photo_location *p, unsigned bundle, const float delta[2],
                           gt4_photo_contact *s)
{
    if (!p || !s || bundle >= p->cameras)
        return;
    float from[2] = {s->point[0], s->point[1]},
          target[2] = {from[0] + delta[0], from[1] + delta[1]};
    for (int i = 0; i < 4; i++) {
        float remain = photo_clip(p, bundle, from, target, s);
        if (!(remain > 0))
            break;
        float dx = target[0] - from[0], dz = target[1] - from[1];
        from[0] = s->point[0];
        from[1] = s->point[1];
        target[0] = from[0] + dx * remain;
        target[1] = from[1] + dz * remain;
    }
}

static float photo_wrap_angle(float a)
{
    a = fmodf(a, 360);
    return a < 0 ? a + 360 : a;
}
static float photo_angle_difference(float a, float b)
{
    float d = a - b;
    if (d > 180)
        d -= 360;
    if (d < -180)
        d += 360;
    return d;
}
/* 003FEF60 / 00370EF8: authored aiming window, and the whole field of
   view must fit inside it. Regions may include holes and several aim points. */
int gt4_photo_aim(const gt4_photo_location *p, unsigned region, const float eye[3], float *yaw,
                  float *fov, int *adjusted)
{
    if (!p || region >= p->regions || !yaw || !fov || !adjusted ||
        !table(p->size, p->region_table, p->regions, 64))
        return 0;
    const uint8_t *r = p->data + p->region_table + region * 64;
    unsigned n = photo_u16(r + 6), at = u32(r + 0x10);
    if (!table(p->size, at, n, 8))
        return 0;
    float centre = photo_wrap_angle(90 + atan2f(f32(r + 0x1c) - eye[2], f32(r + 0x18) - eye[0]) *
                                             57.2957763671875f),
          lo = -360, hi = 720;
    for (unsigned i = 0; i < n; i++) {
        float a = photo_wrap_angle(90 + atan2f(f32(p->data + at + i * 8 + 4) - eye[2],
                                               f32(p->data + at + i * 8) - eye[0]) *
                                            57.2957763671875f),
              b = a;
        if (centre < a)
            a -= 360;
        if (lo < a)
            lo = a;
        a = b;
        if (a < centre)
            a += 360;
        if (a < hi)
            hi = a;
    }
    float a = *yaw;
    if (a < centre && !(lo <= a && a <= hi))
        a += 360;
    if (centre < a && !(lo <= a && a <= hi))
        a -= 360;
    if (!(lo <= a && a <= hi))
        a = centre + photo_angle_difference(photo_wrap_angle(a), centre);
    *adjusted = 0;
    if (a - *fov * .5f < lo) {
        *fov = (a - lo) * 2;
        *adjusted = 1;
    }
    if (hi < a + *fov * .5f) {
        *fov = (hi - a) * 2;
        *adjusted = 1;
    }
    int valid = lo < a && a < hi && *fov >= .001f;
    if (!valid)
        *fov = .001f;
    *yaw = a;
    return valid;
}

/* 00380C50 -> 003FDD60: original camera rejection in the car's local frame. */
int gt4_photo_car_clear(const float local[3], const float size[4], float fov, float near_clip,
                        float distance)
{
    float margin = .5f + near_clip * .700207531452179f / tanf(fov * .5f * .01745329052209854f);
    float length = sqrtf(local[0] * local[0] + local[1] * local[1] + local[2] * local[2]);
    int collision = length - margin <= 5 && fabsf(local[0]) <= size[2] * .5f + margin &&
                    local[1] >= 0 && local[1] <= size[3] + margin &&
                    local[2] >= -size[0] - margin && local[2] <= size[1] + margin;
    float minimum =
        atanf(1 / ((distance + distance) / .699999988079071f)) * 2 / .01745329052209854f;
    return !collision && fov >= minimum;
}

/* 003FDC50 -> 003DE268: ordered active REP4 points, including lap wrap.
   003DE198's partition base is explicitly zero in this executable. */
unsigned gt4_photo_replay_points(const uint8_t *rep, size_t size, float length, float distance,
                                 unsigned *offsets, unsigned capacity)
{
    if (!rep || !offsets || !capacity || size < 0x34 || !(length > 0) || !isfinite(distance))
        return 0;
    unsigned count = u32(rep + 0x2c), at = u32(rep + 0x30);
    if (!count || !table(size, at, count, 48))
        return 0;
    unsigned lo = 0, hi = count;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if (distance < f32(rep + at + mid * 48 + 0x10))
            hi = mid;
        else
            lo = mid + 1;
    }
    if (!lo)
        return 0;
    unsigned centre = lo - 1, n = 0;
    float range = fminf(500, length * .25f), origin = f32(rep + at + centre * 48 + 0x10),
          d = distance;
    int i = (int)centre;
    for (unsigned budget = 0; budget < count && n < capacity; budget++) {
        if (--i < 0) {
            i += count;
            d += length;
            origin += length;
        }
        const uint8_t *point = rep + at + (unsigned)i * 48;
        if (f32(point + 0x10) <= d && d < f32(point + 0x14))
            offsets[n++] = at + (unsigned)i * 48;
        if (!(origin - f32(point + 0x10) < range))
            break;
    }
    i = (int)centre;
    d = distance;
    for (unsigned budget = 0; budget < count && n < capacity; budget++) {
        const uint8_t *point = rep + at + (unsigned)i * 48;
        if (!(f32(point + 0x10) <= d))
            break;
        if (d < f32(point + 0x14))
            offsets[n++] = at + (unsigned)i * 48;
        if (++i >= (int)count) {
            i -= count;
            d -= length;
        }
    }
    return n;
}
