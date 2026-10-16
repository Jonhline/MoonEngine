/*
 * gt4model_write: the round-trip proof for the CAR4/MDLS/Tex1 writer.
 *
 *   gt4model_write <disc.iso> /car/menu/<model> [--color N] [--out file]
 *
 * Decodes a disc car with the engine's own reader, re-encodes it with
 * src/core/model_write.c, decodes the result again, and requires the same
 * triangle count, the same vertices (position, normal, UV, colour, material,
 * bit for bit) and the same texture bytes. A "close" round trip is a failure.
 */
#include "model.h"
#include "model_write.h"
#include "tex1.h"
#include "vol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int eqf(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }

static int vtx_eq(const gt4_vertex *a, const gt4_vertex *b, const char **what)
{
    for (int k = 0; k < 3; k++) {
        if (!eqf(a->p[k], b->p[k])) { *what = "position"; return 0; }
        if (!eqf(a->normal[k], b->normal[k])) { *what = "normal"; return 0; }
        if (!eqf(a->rgb[k], b->rgb[k])) { *what = "colour"; return 0; }
        if (!eqf(a->ambient[k], b->ambient[k])) { *what = "ambient"; return 0; }
        if (!eqf(a->specular[k], b->specular[k])) { *what = "specular"; return 0; }
    }
    for (int k = 0; k < 2; k++) if (!eqf(a->uv[k], b->uv[k])) { *what = "uv"; return 0; }
    if (!eqf(a->shine, b->shine)) { *what = "shine"; return 0; }
    if (!eqf(a->alpha, b->alpha)) { *what = "alpha"; return 0; }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <disc.iso> /car/menu/<model> [--color N] [--out file]\n", argv[0]); return 2; }
    const char *car = argv[2], *out = NULL; int color = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--color") && i + 1 < argc) color = atoi(argv[++i]);
        else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
    }
    const char *err = NULL; gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "volume: %s\n", err ? err : "failed"); return 1; }
    size_t size = 0; uint8_t *bytes = gt4_vol_read(v, car, &size);
    gt4_vol_close(v);
    if (!bytes) { fprintf(stderr, "cannot read %s\n", car); return 1; }

    gt4_model_stats st; memset(&st, 0, sizeof st);
    gt4_model *a = gt4_model_decode_stats(bytes, size, color, 0, &st);
    if (!a) { fprintf(stderr, "decode failed (op %d at 0x%zx, line %d)\n", st.fail_op, st.fail_offset, st.fail_line); free(bytes); return 1; }
    printf("%s: %zu bytes -> %d triangles, %d textures, %d unsupported\n", car, size, a->count, a->ntextures, a->unsupported);

    size_t size2 = 0; uint8_t *bytes2 = gt4_model_encode_car4(a, &size2);
    if (!bytes2) { fprintf(stderr, "FAIL: writer refused the model\n"); gt4_model_free(a); free(bytes); return 1; }
    printf("re-encoded: %zu bytes\n", size2);
    if (out) { FILE *f = fopen(out, "wb"); if (f) { fwrite(bytes2, 1, size2, f); fclose(f); printf("wrote %s\n", out); } }

    gt4_model_stats st2; memset(&st2, 0, sizeof st2);
    gt4_model *b = gt4_model_decode_stats(bytes2, size2, color, 0, &st2);
    if (!b) { fprintf(stderr, "FAIL: re-encoded file does not decode (op %d at 0x%zx, line %d)\n", st2.fail_op, st2.fail_offset, st2.fail_line); gt4_model_free(a); free(bytes); free(bytes2); return 1; }

    int fail = 0, shown = 0;
    if (a->count != b->count) { printf("FAIL: triangles %d -> %d\n", a->count, b->count); fail = 1; }
    for (int i = 0; i < a->count && i < b->count; i++) {
        for (int k = 0; k < 3; k++) {
            const char *what = NULL;
            if (!vtx_eq(&a->triangles[i].v[k], &b->triangles[i].v[k], &what)) {
                fail = 1;
                if (shown++ < 8) {
                    const gt4_vertex *x = &a->triangles[i].v[k], *y = &b->triangles[i].v[k];
                    printf("FAIL tri %d v %d %s: p %a %a %a vs %a %a %a | n %a %a %a vs %a %a %a | uv %a %a vs %a %a | rgb %a %a %a vs %a %a %a | amb %a vs %a | spec %a vs %a | shine %a vs %a\n",
                        i, k, what, x->p[0], x->p[1], x->p[2], y->p[0], y->p[1], y->p[2],
                        x->normal[0], x->normal[1], x->normal[2], y->normal[0], y->normal[1], y->normal[2],
                        x->uv[0], x->uv[1], y->uv[0], y->uv[1],
                        x->rgb[0], x->rgb[1], x->rgb[2], y->rgb[0], y->rgb[1], y->rgb[2],
                        x->ambient[0], y->ambient[0], x->specular[0], y->specular[0], x->shine, y->shine);
                }
            }
        }
        if (a->triangles[i].texture != b->triangles[i].texture) {
            fail = 1; if (shown++ < 8) printf("FAIL tri %d texture %d -> %d\n", i, a->triangles[i].texture, b->triangles[i].texture);
        }
    }
    if (a->ntextures != b->ntextures) { printf("FAIL: textures %d -> %d\n", a->ntextures, b->ntextures); fail = 1; }
    for (int t = 0; t < a->ntextures && t < b->ntextures && !fail; t++) {
        const tex1_image *x = &a->textures[t], *y = &b->textures[t];
        if (x->width != y->width || x->height != y->height) { printf("FAIL texture %d size %dx%d -> %dx%d\n", t, x->width, x->height, y->width, y->height); fail = 1; break; }
        size_t n = (size_t)x->width * x->height;
        if (memcmp(x->rgba, y->rgba, n * 4)) { printf("FAIL texture %d rgba differs\n", t); fail = 1; break; }
        if (x->native_alpha && y->native_alpha && memcmp(x->native_alpha, y->native_alpha, n)) { printf("FAIL texture %d native alpha differs\n", t); fail = 1; break; }
        if (x->mip_count != y->mip_count) { printf("FAIL texture %d mip_count %d -> %d\n", t, x->mip_count, y->mip_count); fail = 1; break; }
        for (unsigned L = 0; L < x->mip_count && !fail; L++) {
            unsigned mw = (unsigned)x->tex_w >> (L + 1), mh = (unsigned)x->tex_h >> (L + 1); if (!mw) mw = 1; if (!mh) mh = 1;
            if (!x->mip_rgba[L] || !y->mip_rgba[L] || memcmp(x->mip_rgba[L], y->mip_rgba[L], (size_t)mw * mh * 4)) { printf("FAIL texture %d mip %u differs\n", t, L + 1); fail = 1; }
        }
    }

    printf("%s: triangles %d, textures %d, vertices %s, texture bytes %s\n",
           fail ? "FAIL" : "PASS", b->count, b->ntextures, fail ? "differ" : "identical", fail ? "differ" : "identical");
    gt4_model_free(a); gt4_model_free(b); free(bytes); free(bytes2);
    return fail;
}
