/* Native, bounded CAR4/MDLS/Tex1 writer -- the mirror of model.c/tex1.c.
   See model_write.h and docs/MODELS.md. The writer emits the simplest byte
   stream the readers accept rather than reproducing a disc file: one MDLS
   model, a shape per draw kind/texture definition carrying baked float
   positions/normals/UVs, a per-vertex colour packet, and a material per
   packet. Nothing here guesses: an unrepresentable model is refused. */
#include "model_write.h"
#include "tex1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- little-endian growable byte buffer ---------------------------------- */
typedef struct { uint8_t *p; size_t n, cap; int ok; } buf;
#define BUF_INIT { 0, 0, 0, 1 }
static void b_reserve(buf *b, size_t extra)
{
    if (b->n + extra <= b->cap) return;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->n + extra) cap *= 2;
    uint8_t *p = realloc(b->p, cap);
    if (!p) { b->ok = 0; return; }
    b->p = p; b->cap = cap;
}
static void b_put(buf *b, const void *d, size_t n)
{
    if (!b->ok) return;
    b_reserve(b, n); if (!b->ok) return;
    if (n) memcpy(b->p + b->n, d, n); b->n += n;
}
static void b_zeros(buf *b, size_t n)
{
    if (!b->ok) return;
    b_reserve(b, n); if (!b->ok) return;
    memset(b->p + b->n, 0, n); b->n += n;
}
static void b_u8(buf *b, unsigned v) { uint8_t x = (uint8_t)v; b_put(b, &x, 1); }
static void b_u16(buf *b, unsigned v) { uint8_t x[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; b_put(b, x, 2); }
static void b_u32(buf *b, uint32_t v) { uint8_t x[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; b_put(b, x, 4); }
static void b_f32(buf *b, float v) { b_put(b, &v, 4); }
static void put_u16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void put_u64(uint8_t *p, uint64_t v) { put_u32(p, (uint32_t)v); put_u32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t rd_u32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static float rd_f32(const uint8_t *p) { float f; memcpy(&f, p, 4); return f; }

/* A CAR4's camera table (header +0x80 count, +0x84 offset; 32 bytes each). The
   same layout the reader parses (car_model.c:412-419). */
int gt4_model_read_cameras(const uint8_t *bytes, size_t size, gt4_car_cameras *out)
{
    if (out) out->count = 0;
    if (!bytes || !out || size < 0x88 || memcmp(bytes, "CAR4", 4)) return 0;
    uint32_t count = rd_u32(bytes + 0x80), at = rd_u32(bytes + 0x84);
    if (count > GT4_CAR_CAMERA_MAX) count = GT4_CAR_CAMERA_MAX;
    int k = 0;
    for (; (uint32_t)k < count && at + (size_t)(k + 1) * 32 <= size; k++)
        for (int j = 0; j < 8; j++) out->point[k][j] = rd_f32(bytes + at + (size_t)k * 32 + j * 4);
    out->count = k;
    return k;
}

/* The length of the donor's blob at offsets[idx]: up to whichever other known
   offset comes next, or to the end of the donor file if none does. Works
   regardless of the three fields' order in a given car (0 = "none", never
   dereferenced). */
static size_t donor_blob_len(const uint32_t offsets[3], int idx, size_t donor_size)
{
    uint32_t off = offsets[idx];
    if (!off || off >= donor_size) return 0;
    uint32_t best = (uint32_t)donor_size;
    for (int i = 0; i < 3; i++)
        if (offsets[i] > off && offsets[i] < best) best = offsets[i];
    return best - off;
}

uint8_t *gt4_model_car4_attach_wheels(const uint8_t *car4, size_t car4_size,
                                      const uint8_t *donor, size_t donor_size, size_t *out_size)
{
    if (!car4 || car4_size < 0x40 || memcmp(car4, "CAR4", 4)) return NULL;
    if (!donor || donor_size < 0x40 || memcmp(donor, "CAR4", 4)) return NULL;
    uint32_t off[3] = { rd_u32(donor + 0x20), rd_u32(donor + 0x2c), rd_u32(donor + 0x30) };
    size_t len[3];
    size_t extra = 0;
    for (int i = 0; i < 3; i++) { len[i] = donor_blob_len(off, i, donor_size); extra += len[i]; }
    if (!extra) return NULL;   /* the donor itself has no rim/tyre to borrow */

    buf out = BUF_INIT;
    b_put(&out, car4, car4_size);
    if (!out.ok) { free(out.p); return NULL; }
    size_t at[3];
    for (int i = 0; i < 3; i++) {
        at[i] = out.n;
        if (len[i]) b_put(&out, donor + off[i], len[i]);
    }
    if (!out.ok) { free(out.p); return NULL; }
    const int field[3] = { 0x20, 0x2c, 0x30 };
    for (int i = 0; i < 3; i++)
        if (len[i]) put_u32(out.p + field[i], (uint32_t)at[i]);
    if (out_size) *out_size = out.n;
    return out.p;
}

/* ---- Tex1 (PSMCT32, one transfer per image + level) ---------------------- */
static unsigned next_pow2(unsigned v) { unsigned p = 1; while (p < v) p <<= 1; return p; }
static unsigned ilog2(unsigned v) { unsigned s = 0; while ((1u << s) < v) s++; return s; }

uint8_t *gt4_tex1_encode(const tex1_image *images, int count, size_t *size)
{
    if (!images || count <= 0 || count > 511) return NULL;
    unsigned ntr = 0;
    for (int i = 0; i < count; i++) ntr += 1 + images[i].mip_count;
    size_t ptex = 0x30, ptr = ptex + (size_t)0x28 * (unsigned)count;
    size_t data = ptr + 12u * ntr;

    /* assign each transfer a tbp block (256-byte units) and a data offset */
    typedef struct { uint32_t tbp, bw; unsigned w, h, level, image; size_t off; } xfer;
    xfer *x = calloc(ntr ? ntr : 1, sizeof *x);
    if (!x) return NULL;
    unsigned xi = 0, blocks = 0; size_t at = 0;
    for (int i = 0; i < count; i++) {
        const tex1_image *im = &images[i];
        if (!im->rgba || im->width <= 0 || im->height <= 0) { free(x); return NULL; }
        unsigned tw = next_pow2((unsigned)im->width), th = next_pow2((unsigned)im->height);
        if (tw > 1024 || th > 1024) { free(x); return NULL; }
        for (unsigned level = 0; level <= im->mip_count; level++) {
            unsigned lw, lh;
            if (level == 0) { lw = tw; lh = th; }
            else { if (!im->mip_rgba[level - 1]) { free(x); return NULL; }
                   lw = tw >> level; lh = th >> level; if (!lw) lw = 1; if (!lh) lh = 1; }
            unsigned w = level == 0 ? (unsigned)im->width : lw;
            unsigned h = level == 0 ? (unsigned)im->height : lh;
            unsigned bw = lw / 64; if (bw < 1) bw = 1;
            unsigned maxpage = ((lh / 32) ? (lh / 32) - 1 : 0) * bw + ((lw / 64) ? (lw / 64) - 1 : 0);
            unsigned need = (maxpage + 1) * 32;
            if (blocks + need > 16384) { free(x); return NULL; }   /* 4 MB of VRAM */
            x[xi].tbp = blocks; x[xi].bw = bw; x[xi].w = w; x[xi].h = h;
            x[xi].level = level; x[xi].image = (unsigned)i; x[xi].off = at;
            at += (size_t)w * h * 4;
            blocks += need;
            xi++;
        }
    }

    buf b = BUF_INIT;
    b_put(&b, "Tex1", 4); b_zeros(&b, 0x2c);
    put_u16(b.p + 0x14, (unsigned)count);
    put_u16(b.p + 0x16, ntr);
    put_u32(b.p + 0x18, (uint32_t)ptex);
    put_u32(b.p + 0x1c, (uint32_t)ptr);

    b_zeros(&b, (size_t)0x28 * (unsigned)count);
    uint8_t *desc = b.p + ptex;
    for (int i = 0; i < count; i++) {
        const tex1_image *im = &images[i];
        unsigned tw = next_pow2((unsigned)im->width), th = next_pow2((unsigned)im->height);
        int base = -1;
        for (unsigned k = 0; k < xi; k++) if (x[k].image == (unsigned)i && x[k].level == 0) { base = (int)k; break; }
        if (base < 0) { free(x); free(b.p); return NULL; }
        uint64_t tex0 = (uint64_t)(x[base].tbp & 0x3fff)
                      | ((uint64_t)(x[base].bw & 0x3f) << 14)
                      | ((uint64_t)(ilog2(tw) & 0xf) << 26)
                      | ((uint64_t)(ilog2(th) & 0xf) << 30)
                      | ((uint64_t)(im->color_component & 1) << 34)
                      | ((uint64_t)(im->texture_function & 3) << 35);
        unsigned levels = im->mip_count; if (levels > 6) levels = 6;
        uint64_t tex1 = ((uint64_t)levels << 2) | ((uint64_t)(im->magnify & 1) << 5) | ((uint64_t)(im->minify & 7) << 6);
        uint64_t m1 = 0, m2 = 0;
        for (unsigned L = 1; L <= levels; L++) {
            int k = -1;
            for (unsigned q = 0; q < xi; q++) if (x[q].image == (unsigned)i && x[q].level == L) { k = (int)q; break; }
            if (k < 0) { free(x); free(b.p); return NULL; }
            uint64_t reg = (uint64_t)(x[k].tbp & 0x3fff) | ((uint64_t)(x[k].bw & 0x3f) << 14);
            if (L <= 3) m1 |= reg << ((L - 1) * 20); else m2 |= reg << ((L - 4) * 20);
        }
        /* REGION_CLAMP: the image's own size independent of its power-of-two
           texture. The reader takes w/h from here (tex1.c:157). */
        uint64_t clamp = 2 | (2ull << 2)
                       | ((uint64_t)((im->width - 1) & 0x3ff) << 14)
                       | ((uint64_t)((im->height - 1) & 0x3ff) << 34);
        /* an authored image that tiles (wrap 0, its own size a power of two): REPEAT,
           which the reader and both renderers honour; the size is then the texture's */
        if (im->wrap_u == 0 && im->wrap_v == 0 && (unsigned)im->width == tw &&
            (unsigned)im->height == th)
            clamp = 0;
        uint8_t *d = desc + (size_t)0x28 * i;
        put_u64(d + 0, tex0); put_u64(d + 8, tex1); put_u64(d + 16, m1); put_u64(d + 24, m2); put_u64(d + 32, clamp);
    }

    b_zeros(&b, 12u * ntr);
    uint8_t *tr = b.p + ptr;
    for (unsigned k = 0; k < xi; k++) {
        uint8_t *d = tr + 12u * k;
        put_u32(d + 0, (uint32_t)(data + x[k].off));
        put_u16(d + 4, x[k].tbp);
        d[6] = (uint8_t)x[k].bw; d[7] = 0;
        put_u16(d + 8, x[k].w); put_u16(d + 10, x[k].h);
    }

    for (unsigned k = 0; k < xi; k++) {
        const tex1_image *im = &images[x[k].image];
        size_t n = (size_t)x[k].w * x[k].h;
        if (x[k].level == 0) {
            /* store native alpha in the 4th byte so the reader's PSMCT32
               conversion (>=0x80 -> 255, else *2) returns rgba unchanged and
               native_alpha exact (tex1.c:207). */
            uint8_t *px = malloc(n * 4);
            if (!px) { free(x); free(b.p); return NULL; }
            for (size_t p = 0; p < n; p++) {
                memcpy(px + p * 4, im->rgba + p * 4, 3);
                px[p * 4 + 3] = im->native_alpha ? im->native_alpha[p] : im->rgba[p * 4 + 3];
            }
            b_put(&b, px, n * 4); free(px);
        } else {
        b_put(&b, im->mip_rgba[x[k].level - 1], n * 4);
        }
    }
    free(x);
    if (!b.ok) { free(b.p); return NULL; }
    if (size) *size = b.n;
    return b.p;
}

/* ---- materials ----------------------------------------------------------- */
typedef struct { float rgb[3], amb[3], spec[3], shine; } material;
static int same_material(const material *a, const float D[3], const gt4_vertex *v)
{
    /* rgb travels in the per-vertex colour packet (byte/128); the material
       carries the diffuse base D and the ambient/specular/shine the reader
       applies to every vertex of the packet. */
    return a->rgb[0] == D[0] && a->rgb[1] == D[1] && a->rgb[2] == D[2]
        && a->amb[0] == v->ambient[0] && a->amb[1] == v->ambient[1] && a->amb[2] == v->ambient[2]
        && a->spec[0] == v->specular[0] && a->spec[1] == v->specular[1] && a->spec[2] == v->specular[2]
        && a->shine == v->shine;
}

/* One vertex stream per triangle (a three-vertex strip), float positions,
   normals and UVs so the reader returns them unchanged. */
#define PACKET_TRIS 85
static void packet_add(buf *b, const gt4_triangle *tris, int first, int count, const float D[3])
{
    unsigned nv = (unsigned)count * 3, nr = (unsigned)count + 1;
    if (count && tris[first].blend_enabled) {
        /* the packet's GIF tag (0xc0c0): PRIM's ABE (bit 21 of its second word) turns the
           GS's alpha blending on, which is how the disc's glass is see-through
           (model.c reads only that bit) */
        b_u16(b, 0xc0c0); b_u8(b, 1); b_u8(b, 0x6c);
        b_u32(b, 0); b_u32(b, 1u << 21); b_u32(b, 0);
    }
    b_u16(b, 0xc040); b_u8(b, (unsigned)nr); b_u8(b, 0x62);   /* reset: one strip per triangle */
    b_u8(b, 0); for (int i = 0; i < count; i++) b_u8(b, 3);
    while (b->n & 3) b_u8(b, 0);
    b_u16(b, 0xc000); b_u8(b, (unsigned)nv); b_u8(b, 0x68);   /* positions, V3-32 float */
    for (int i = 0; i < count; i++) for (int k = 0; k < 3; k++) {
        const gt4_vertex *v = &tris[first + i].v[k];
        b_f32(b, v->p[0]); b_f32(b, v->p[1]); b_f32(b, v->p[2]);
    }
    b_u16(b, 0xc040); b_u8(b, (unsigned)nv); b_u8(b, 0x64);   /* UVs, V2-32 float */
    for (int i = 0; i < count; i++) for (int k = 0; k < 3; k++) {
        const gt4_vertex *v = &tris[first + i].v[k];
        b_f32(b, v->uv[0]); b_f32(b, v->uv[1]);
    }
    b_u16(b, 0x8080); b_u8(b, (unsigned)nv); b_u8(b, 0x68);   /* normals, V3-32 float */
    for (int i = 0; i < count; i++) for (int k = 0; k < 3; k++) {
        const gt4_vertex *v = &tris[first + i].v[k];
        b_f32(b, v->normal[0]); b_f32(b, v->normal[1]); b_f32(b, v->normal[2]);
    }
    /* colour/alpha, V4-8 (0xc080): rgb = material diffuse * byte/128, the 4th
       byte the authored alpha at 1/128 (model.c:523). */
    b_u16(b, 0xc080); b_u8(b, (unsigned)nv); b_u8(b, 0x6e);
    for (int i = 0; i < count; i++) for (int k = 0; k < 3; k++) {
        const gt4_vertex *v = &tris[first + i].v[k];
        for (int c = 0; c < 3; c++) {
            int ib = D[c] != 0 ? (int)lroundf(v->rgb[c] / D[c] * 128.0f) : 0;
            if (ib < 0) ib = 0; if (ib > 255) ib = 255;
            b_u8(b, (unsigned)ib);
        }
        int ia = (int)lroundf(v->alpha * 128.0f); if (ia < 0) ia = 0; if (ia > 255) ia = 255;
        b_u8(b, (unsigned)ia);
    }
    while (b->n & 15) { b_u16(b, 0); b_u8(b, 0); b_u8(b, 11); }   /* opcode 11 = a 4-byte no-op */
}

/* Append one shape (16-byte header, packet descriptors, packet bodies) for the
   triangle range [first,last): one draw kind, one texture definition. The
   packet texture index is relative to the definition's first image. Returns the
   packet count, or -1 on refusal. */
static int append_shape(buf *out, const gt4_model *m, const int *tri_mat, const material *mats,
                        int tex_base, int first, int last, unsigned kind)
{
    buf pkt = BUF_INIT;
    size_t off[8192]; unsigned len[8192], flag[8192]; int np = 0;
    for (int i = first; i < last; ) {
        int mat = tri_mat[i];
        int tex = (m->triangles[i].texture >= 0 && m->triangles[i].texture < m->ntextures)
                ? m->triangles[i].texture - tex_base + 1 : 0;
        int j = i + 1;
        while (j < last && j - i < PACKET_TRIS && tri_mat[j] == mat) {
            int t2 = (m->triangles[j].texture >= 0 && m->triangles[j].texture < m->ntextures)
                   ? m->triangles[j].texture - tex_base + 1 : 0;
            if (t2 != tex) break;
            j++;
        }
        if (np == 8192) { free(pkt.p); return -1; }
        off[np] = pkt.n;
        packet_add(&pkt, m->triangles, i, j - i, mats[mat].rgb);
        len[np] = (unsigned)((pkt.n - off[np]) / 16);
        flag[np] = (unsigned)(((mat + 1) << 9) | tex);
        np++;
        i = j;
    }
    if (!pkt.ok) { free(pkt.p); return -1; }
    size_t desc_bytes = (size_t)np * 8;
    size_t at = out->n;
    b_zeros(out, 16);
    out->p[at + 8] = (uint8_t)(kind & 31);
    put_u16(out->p + at + 10, (unsigned)np);
    for (int k = 0; k < np; k++) {
        b_u32(out, (uint32_t)(16 + desc_bytes + off[k]));   /* shape-relative */
        b_u16(out, len[k]);
        b_u16(out, flag[k]);
    }
    b_put(out, pkt.p, pkt.n);
    free(pkt.p);
    return out->ok ? np : -1;
}

/* ---- texture packaging (one Tex1 definition per 4 MB VRAM image) --------- */
static unsigned image_blocks(const tex1_image *im)
{
    unsigned tw = next_pow2((unsigned)im->width), th = next_pow2((unsigned)im->height), tot = 0;
    for (unsigned level = 0; level <= im->mip_count; level++) {
        unsigned lw = level ? tw >> level : tw, lh = level ? th >> level : th;
        if (!lw) lw = 1; if (!lh) lh = 1;
        unsigned bw = lw / 64; if (bw < 1) bw = 1;
        unsigned maxpage = ((lh / 32) ? (lh / 32) - 1 : 0) * bw + ((lw / 64) ? (lw / 64) - 1 : 0);
        tot += (maxpage + 1) * 32;
    }
    return tot;
}

/* Pack the images into as few Tex1 definitions as fit in 4 MB of VRAM each,
   then encode one Tex1 blob per definition. Returns the definition count. */
static int pack_definitions(const gt4_model *m, uint8_t *blob[], size_t size[], int first_of[], int *def_of)
{
    int nt = 0, i = 0;
    while (i < m->ntextures) {
        int j = i; unsigned used = 0;
        while (j < m->ntextures) {
            unsigned b = image_blocks(&m->textures[j]);
            if (b > 16384) return -2;
            if (used && used + b > 16384) break;
            used += b; j++;
        }
        if (nt == 128) return -3;
        size_t sz = 0;
        uint8_t *p = gt4_tex1_encode(m->textures + i, j - i, &sz);
        if (!p) return -1;
        blob[nt] = p; size[nt] = sz; first_of[nt] = i;
        for (int k = i; k < j; k++) def_of[k] = nt;
        nt++; i = j;
    }
    return nt;
}

/* Original: func_00453698 +0x100. Callbacks 11..14 retain the road-wheel
   frame; callback 0 selects released/pressed brake-lamp geometry. */
static void shape_command(buf *cmd, unsigned shape, unsigned attachment, unsigned lamp)
{
    if (attachment) {
        b_u8(cmd, 12);
        b_u8(cmd, 6);
        b_u16(cmd, 10 + attachment);
        b_u8(cmd, 0);
    }
    if (!lamp) {
        b_u8(cmd, 5);
        b_u16(cmd, shape);
    } else {
        size_t branch = cmd->n;
        b_u8(cmd, 6);
        b_u16(cmd, 0);
        b_u8(cmd, 2);
        b_u16(cmd, 4);
        b_u16(cmd, 0);
        if (lamp == 1) {
            b_u8(cmd, 5);
            b_u16(cmd, shape);
        } else
            b_u8(cmd, 11);
        b_u8(cmd, 9);
        size_t jump = cmd->n;
        b_u16(cmd, 0);
        size_t pressed = cmd->n;
        if (lamp == 2) {
            b_u8(cmd, 5);
            b_u16(cmd, shape);
        } else
            b_u8(cmd, 11);
        size_t join = cmd->n;
        b_u8(cmd, 11);
        if (cmd->ok) {
            put_u16(cmd->p + branch + 6, (unsigned)(pressed - branch - 6));
            put_u16(cmd->p + jump, (unsigned)(join - jump));
        }
    }
    if (attachment)
        b_u8(cmd, 13);
}

/* The MDLS a CAR4 wraps -- also the exact format gt4_model_decode_part /
   gt4_model_decode_wheel_part read directly, unwrapped, at a rim/tyre offset
   (car_model.c:163-164: data+off straight into the decoder, no CAR4 magic
   expected there). One encoder, two uses: the body goes out through the CAR4
   wrapper below; a rim or tyre goes out through gt4_model_encode_wheel_part
   and is appended raw by gt4_model_car4_attach_wheel_parts. */
static uint8_t *encode_mdls(const gt4_model *m, size_t *size)
{
    if (!m || m->count <= 0) return NULL;

    /* A material carries the diffuse base D of a packet and its
       ambient/specular/shine. Every vertex colour must be D * (integer/128):
       first try D = 1 (rgb already a multiple of 1/128), else D = the triangle's
       own uniform rgb. A triangle that is neither cannot be written exactly,
       and is refused rather than approximated. */
    material mats[128]; int nm = 0;
    int *tri_mat = malloc((size_t)m->count * sizeof *tri_mat);
    if (!tri_mat) return NULL;
    for (int i = 0; i < m->count; i++) {
        const gt4_triangle *t = &m->triangles[i];
        const gt4_vertex *v = &t->v[0];
        float D[3];
        int multiple = 1;
        for (int k = 0; k < 3 && multiple; k++) for (int c = 0; c < 3; c++) {
            float x = t->v[k].rgb[c] * 128.0f, r = roundf(x);
            if (fabsf(x - r) > 1e-3f || r < 0 || r > 255) { multiple = 0; break; }
        }
        if (multiple) { D[0] = D[1] = D[2] = 1.0f; }
        else {
            int uniform = 1;
            for (int k = 1; k < 3; k++)
                for (int c = 0; c < 3; c++) if (t->v[k].rgb[c] != v->rgb[c]) uniform = 0;
            if (!uniform) { free(tri_mat); return NULL; }
            D[0] = v->rgb[0]; D[1] = v->rgb[1]; D[2] = v->rgb[2];
        }
        int k = 0; for (; k < nm; k++) if (same_material(&mats[k], D, v)) break;
        if (k == nm) {
            if (nm == 128) { free(tri_mat); return NULL; }
            mats[nm].rgb[0] = D[0]; mats[nm].rgb[1] = D[1]; mats[nm].rgb[2] = D[2];
            mats[nm].amb[0] = v->ambient[0]; mats[nm].amb[1] = v->ambient[1]; mats[nm].amb[2] = v->ambient[2];
            mats[nm].spec[0] = v->specular[0]; mats[nm].spec[1] = v->specular[1]; mats[nm].spec[2] = v->specular[2];
            mats[nm].shine = v->shine; nm++;
        }
        tri_mat[i] = k;
    }

    /* textures: as many Tex1 definitions as 4 MB VRAM needs */
    uint8_t *blob[128]; size_t bsize[128]; int first_of[128];
    int *def_of = malloc((m->ntextures ? (size_t)m->ntextures : 1) * sizeof *def_of);
    if (!def_of) { free(tri_mat); return NULL; }
    int nt = 0;
    if (m->ntextures) {
        nt = pack_definitions(m, blob, bsize, first_of, def_of);
        if (nt <= 0) { free(tri_mat); free(def_of); return NULL; }
    }

    /* Shapes: one per contiguous run of equal draw kind and texture definition,
       so triangle order and the shadow pass (kind 2 under table -1, decoded as
       texture -2) survive. */
    buf shapes = BUF_INIT, cmd = BUF_INIT;
    size_t sh_off[4096]; int sh_table[4096]; int ns = 0;
    /* The reader starts every model's own command stream already assuming
       table 0 is bound (model.c:1405, "table = 0" -- no command required to
       get there). That is wrong for an untextured model (nt == 0: table 0
       does not exist, nothing to bind) -- opcode 4/5's own bounds check
       (model.c:396, "*table >= r->nt") then fails on the very first shape.
       Measured on a hand-authored, textureless wheel part. Start from
       whatever table actually exists instead of assuming 0. */
    if (nt > 0) { b_u8(&cmd, 43); b_u8(&cmd, 0); }     /* bind texture table 0 */
    else { b_u8(&cmd, 81); b_u8(&cmd, 0); b_u8(&cmd, 0); } /* bind "no table" (-1) */
    int table = nt > 0 ? 0 : -1;
    for (int i = 0; i < m->count; ) {
        unsigned kind = m->triangles[i].kind & 31;
        unsigned attachment = m->triangles[i].attachment, lamp = m->triangles[i].lamp;
        if (attachment > 4 || lamp > 2) {
            free(tri_mat); free(def_of); free(shapes.p); free(cmd.p); goto fail_blobs;
        }
        int tab1 = (m->triangles[i].texture == -2);
        int def = (!tab1 && m->triangles[i].texture >= 0 && m->triangles[i].texture < m->ntextures) ? def_of[m->triangles[i].texture] : 0;
        int j = i + 1;
        while (j < m->count && (m->triangles[j].kind & 31) == kind && (m->triangles[j].texture == -2) == tab1) {
            int d2 = (!tab1 && m->triangles[j].texture >= 0 && m->triangles[j].texture < m->ntextures) ? def_of[m->triangles[j].texture] : 0;
            if (d2 != def || m->triangles[j].attachment != attachment || m->triangles[j].lamp != lamp) break;
            j++;
        }
        if (ns == 4096) { free(tri_mat); free(def_of); free(shapes.p); free(cmd.p); goto fail_blobs; }
        sh_off[ns] = shapes.n; sh_table[ns] = tab1 ? -1 : def;
        if (append_shape(&shapes, m, tri_mat, mats, m->ntextures ? first_of[def] : 0, i, j, kind) < 0) {
            free(tri_mat); free(def_of); free(shapes.p); free(cmd.p); goto fail_blobs;
        }
        { int want = (tab1 || !nt) ? -1 : def;   /* no texture at all -> no real table to switch to */
          if (want != table) {
              if (want < 0) { b_u8(&cmd, 81); b_u8(&cmd, 0); b_u8(&cmd, 0); }
              else { b_u8(&cmd, 43); b_u8(&cmd, (unsigned)want); }
              table = want;
          } }
        /* a reflection layer (kind 2 over the environment slot) draws as the disc's paint
           does: blend 0x58 (Cs * Ad + Cd, added where the body wrote its alpha), depth not
           written (measured on bmw_0014's 1431-triangle layer), then the default back */
        int reflect = kind == 2 && tab1;
        if (reflect) { b_u8(&cmd, 32); b_u8(&cmd, 0x58); b_u8(&cmd, 0); b_u8(&cmd, 37); }
        shape_command(&cmd, (unsigned)ns, attachment, lamp);
        if (reflect) { b_u8(&cmd, 32); b_u8(&cmd, 0x44); b_u8(&cmd, 0); b_u8(&cmd, 38); }
        ns++;
        i = j;
    }
    b_u8(&cmd, 0);                                 /* end of the command stream */
    free(tri_mat);
    if (!shapes.ok || !cmd.ok) { free(def_of); free(shapes.p); free(cmd.p); goto fail_blobs; }

    /* MDLS layout: header, shape array, model array, materials, list pointer,
       texture-definition array, shape blobs, tex1 blobs, command stream. */
    enum { HDR = 0x60 };
    size_t sh_arr = HDR;
    size_t mo_arr = sh_arr + (size_t)ns * 4;
    size_t mat_at = mo_arr + 40;
    size_t listp = mat_at + (size_t)nm * 80;
    size_t defarr = listp + 4;
    size_t shape_at = defarr + (size_t)nt * 4;
    size_t tex_at = shape_at + shapes.n;
    size_t cmd_at = tex_at; for (int d = 0; d < nt; d++) cmd_at += bsize[d];
    size_t total = cmd_at + cmd.n;

    buf d = BUF_INIT;
    b_zeros(&d, HDR);
    for (int s = 0; s < ns; s++) b_u32(&d, (uint32_t)(shape_at + sh_off[s]));
    b_zeros(&d, 40);                                  /* model 0 descriptor */
    b_zeros(&d, (size_t)nm * 80);                     /* materials */
    b_u32(&d, (uint32_t)defarr);                      /* list pointer contents */
    b_zeros(&d, (size_t)nt * 4);                      /* definition offset table */
    { size_t off = tex_at; for (int t = 0; t < nt; t++) { put_u32(d.p + defarr + (size_t)t * 4, (uint32_t)off); off += bsize[t]; } }
    b_put(&d, shapes.p, shapes.n);
    for (int t = 0; t < nt; t++) b_put(&d, blob[t], bsize[t]);
    b_put(&d, cmd.p, cmd.n);
    for (int t = 0; t < nt; t++) free(blob[t]);
    free(shapes.p); free(cmd.p); free(def_of);
    if (!d.ok) { free(d.p); goto fail_blobs; }

    /* MDLS header fields */
    uint8_t *h = d.p;
    memcpy(h, "MDLS", 4);
    put_u32(h + 16, (uint32_t)total);
    put_u16(h + 22, 1);                               /* models */
    put_u16(h + 24, (unsigned)ns);                    /* ns */
    put_u16(h + 26, (unsigned)nm);                    /* nm */
    put_u16(h + 28, (unsigned)nt);                    /* nt */
    put_u32(h + 56, (uint32_t)mo_arr);
    put_u32(h + 60, (uint32_t)sh_arr);
    put_u32(h + 64, (uint32_t)mat_at);
    put_u32(h + 68, (uint32_t)listp);
    put_u32(h + mo_arr + 24, (uint32_t)cmd_at);       /* model 0 command stream */
    for (int i = 0; i < nm; i++) {
        uint8_t *mp = h + mat_at + (size_t)i * 80;
        memcpy(mp + 0, mats[i].amb, 12);
        memcpy(mp + 16, mats[i].rgb, 12);
        float one = 1.0f; memcpy(mp + 28, &one, 4);
        memcpy(mp + 32, mats[i].spec, 12);
        memcpy(mp + 64, &mats[i].shine, 4);
    }
    if (size) *size = total;
    return d.p;

fail_blobs:
    for (int t = 0; t < nt; t++) free(blob[t]);
    return NULL;
}

uint8_t *gt4_model_encode_wheel_part(const gt4_model *m, size_t *size)
{
    return encode_mdls(m, size);
}

/* Give an already-encoded CAR4 its OWN rim and tyre (no donor): each of
   `rim`/`tyre` is encoded as its own MDLS (encode_mdls, the same format the
   reader expects raw at +0x20/+0x2c/+0x30 -- car_model.c:163-164) and
   appended; `tyre` is used for both the front (+0x2c) and rear (+0x30)
   offset, the common case (the engine stores them twice regardless of
   whether they differ). Either argument may be NULL to leave that slot as
   the body-only "none" sentinel. Mirrors gt4_model_car4_attach_wheels (the
   donor-copy path) field for field; the two are not meant to be combined in
   one export. */
uint8_t *gt4_model_car4_attach_wheel_parts(const uint8_t *car4, size_t car4_size,
                                           const gt4_model *rim, const gt4_model *tyre,
                                           size_t *out_size)
{
    if (!car4 || car4_size < 0x40 || memcmp(car4, "CAR4", 4)) return NULL;
    if (!rim && !tyre) return NULL;
    size_t rim_sz = 0, tyre_sz = 0;
    uint8_t *rim_b = rim ? encode_mdls(rim, &rim_sz) : NULL;
    uint8_t *tyre_b = tyre ? encode_mdls(tyre, &tyre_sz) : NULL;
    if ((rim && !rim_b) || (tyre && !tyre_b)) { free(rim_b); free(tyre_b); return NULL; }

    buf out = BUF_INIT;
    b_put(&out, car4, car4_size);
    size_t rim_at = 0, tyre_at = 0;
    if (rim_b) { rim_at = out.n; b_put(&out, rim_b, rim_sz); }
    if (tyre_b) { tyre_at = out.n; b_put(&out, tyre_b, tyre_sz); }
    free(rim_b); free(tyre_b);
    if (!out.ok) { free(out.p); return NULL; }
    if (rim_b) put_u32(out.p + 0x20, (uint32_t)rim_at);
    if (tyre_b) { put_u32(out.p + 0x2c, (uint32_t)tyre_at); put_u32(out.p + 0x30, (uint32_t)tyre_at); }
    if (out_size) *out_size = out.n;
    return out.p;
}

uint8_t *gt4_model_encode_car4_cameras(const gt4_model *m, const gt4_car_cameras *cameras, size_t *size)
{
    size_t total = 0;
    uint8_t *mdls = encode_mdls(m, &total);
    if (!mdls) return NULL;

    /* CAR4 container: header, then the MDLS. With a camera table the header
       grows to hold it at the disc's own offset (+0x488, the Ka's), so +0x80/
       +0x84 are header and the MDLS follows; without one the header stays 0x40
       and the output is byte-for-byte as before. +0x20/+0x2c/+0x30 (rim,
       front/rear tyre) are put past the end so the loader reads no wheels --
       this model has none by design. +0x10 (CARINFO) stays 0. */
    int cam_count = cameras && cameras->count > 0 ? cameras->count : 0;
    if (cam_count > GT4_CAR_CAMERA_MAX) cam_count = GT4_CAR_CAMERA_MAX;
    size_t cam_at = 0x488;                              /* the table's own offset */
    size_t hdr = cam_count ? cam_at + (size_t)cam_count * 32 : 0x40;
    size_t car4_size = hdr + total;
    buf out = BUF_INIT;
    b_zeros(&out, hdr);
    memcpy(out.p, "CAR4", 4);
    put_u32(out.p + 8, (uint32_t)car4_size);
    put_u32(out.p + 0x18, (uint32_t)hdr);               /* base MDLS */
    put_u32(out.p + 0x1c, 0);                           /* no Pat0 */
    put_u32(out.p + 0x20, (uint32_t)car4_size);
    put_u32(out.p + 0x2c, (uint32_t)car4_size);
    put_u32(out.p + 0x30, (uint32_t)car4_size);
    if (cam_count) {
        put_u32(out.p + 0x80, (uint32_t)cam_count);     /* +0x80 count, +0x84 offset */
        put_u32(out.p + 0x84, (uint32_t)cam_at);
        for (int i = 0; i < cam_count; i++)
            for (int j = 0; j < 8; j++) {
                float f = cameras->point[i][j];
                uint32_t bits;
                memcpy(&bits, &f, 4);
                put_u32(out.p + cam_at + (size_t)i * 32 + j * 4, bits);
            }
    }
    b_put(&out, mdls, total);
    free(mdls);
    if (!out.ok || out.n != car4_size) { free(out.p); return NULL; }
    if (size) *size = out.n;
    return out.p;
}

uint8_t *gt4_model_encode_car4(const gt4_model *m, size_t *size)
{
    return gt4_model_encode_car4_cameras(m, NULL, size);
}

/* ---- car/hires/<model>: the authored-size textures of a mod car ----------- */
#include "miniz.h"
static int pow2(int v) { return v > 0 && !(v & (v - 1)); }
uint8_t *gt4_hires_encode(const tex1_image *images, const int *index, int count, size_t *size)
{
    if (!images || !index || count <= 0 || count > 4096) return NULL;
    buf b = BUF_INIT;
    b_put(&b, "MHT1", 4); b_u32(&b, (uint32_t)count);
    for (int i = 0; i < count && b.ok; i++) {
        const tex1_image *im = &images[i];
        if (!im->rgba || im->width <= 0 || im->height <= 0 || im->width > 8192 || im->height > 8192) { b.ok = 0; break; }
        mz_ulong raw = (mz_ulong)im->width * im->height * 4, packed = mz_compressBound(raw);
        uint8_t *z = malloc(packed);
        if (!z || mz_compress2(z, &packed, im->rgba, raw, 6) != MZ_OK) { free(z); b.ok = 0; break; }
        b_u32(&b, (uint32_t)index[i]); b_u32(&b, (uint32_t)im->width); b_u32(&b, (uint32_t)im->height);
        b_u32(&b, (uint32_t)packed); b_put(&b, z, packed);
        free(z);
    }
    if (!b.ok) { free(b.p); return NULL; }
    if (size) *size = b.n;
    return b.p;
}
int gt4_hires_apply(const uint8_t *data, size_t size, tex1_image *images, int count)
{
    if (!data || size < 8 || memcmp(data, "MHT1", 4) || !images || count <= 0) return 0;
    uint32_t n = rd_u32(data + 4), done = 0;
    size_t at = 8;
    for (uint32_t i = 0; i < n && at + 16 <= size; i++) {
        uint32_t idx = rd_u32(data + at), w = rd_u32(data + at + 4), h = rd_u32(data + at + 8),
                 packed = rd_u32(data + at + 12);
        at += 16;
        if (packed > size - at) break;
        const uint8_t *z = data + at;
        at += packed;
        if (idx >= (uint32_t)count || !pow2((int)w) || !pow2((int)h) || w > 8192 || h > 8192) continue;
        tex1_image *im = &images[idx];
        if (im->width != im->tex_w || im->height != im->tex_h || !pow2(im->tex_w) || !pow2(im->tex_h)) continue;
        mz_ulong raw = (mz_ulong)w * h * 4;
        uint8_t *px = malloc(raw), *na = malloc((size_t)w * h);
        if (!px || !na || mz_uncompress(px, &raw, z, packed) != MZ_OK || raw != (mz_ulong)w * h * 4) {
            free(px); free(na); continue;
        }
        for (size_t p = 0; p < (size_t)w * h; p++) na[p] = (uint8_t)((px[p * 4 + 3] * 128u + 127u) / 255u);
        free(im->rgba); free(im->native_alpha);
        for (int L = 0; L < 6; L++) { free(im->mip_rgba[L]); im->mip_rgba[L] = NULL; }
        im->mip_count = 0;
        im->rgba = px; im->native_alpha = na;
        im->width = im->tex_w = (int)w; im->height = im->tex_h = (int)h;
        im->min_u = im->min_v = 0; im->max_u = (uint16_t)(w - 1); im->max_v = (uint16_t)(h - 1);
        done++;
    }
    return (int)done;
}
