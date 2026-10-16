/*
 * moonmod_glbcar: turn a .glb (glTF 2.0 binary) into a GT4 CAR4 car model,
 * the file /car/<menu|lod|open>/<model> expects. This is the SDK's model
 * intake (docs/vulcan4/SDK-MODEL-INTAKE.md): POSITION + NORMAL + TEXCOORD_0
 * and one embedded PNG, converted to the MDLS/Tex1 stream the engine's own
 * reader accepts (src/core/model_write.c). The scale and origin are normalised
 * here; the body is written with attachment id 0 and no wheels.
 *
 *   moonmod_glbcar <in.glb> <out-car4> [--length M]
 */
#include "model.h"
#include "model_write.h"
#include "tex1.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the core stack's stb_image is JPEG-only (STBI_ONLY_JPEG); this tool carries
   its own static copy so it can read the PNG an asset embeds. */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#include "../../third_party/stb/stb_image.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(p) mkdir((p), 0777)
#endif
static void mkdir_p(const char *path)
{
    char b[1200]; snprintf(b, sizeof b, "%s", path);
    for (char *q = b + 1; *q; q++) if (*q == '/' || *q == '\\') { *q = 0; MKDIR(b); *q = '/'; }
}

/* ---- a small JSON reader (objects, arrays, numbers, strings, literals) ---- */
typedef struct jv jv;
struct jv {
    int t;                 /* 0 null, 1 bool, 2 number, 3 string, 4 array, 5 object */
    double num;
    char *str;
    jv **items; char **keys; int n;
};
static const char *P;
static void skipws(void) { while (*P == ' ' || *P == '\t' || *P == '\n' || *P == '\r') P++; }
static jv *jnew(int t) { jv *v = calloc(1, sizeof *v); v->t = t; return v; }
static void push(jv *v, char *key, jv *item)
{
    v->items = realloc(v->items, (v->n + 1) * sizeof *v->items);
    v->keys = realloc(v->keys, (v->n + 1) * sizeof *v->keys);
    v->items[v->n] = item; v->keys[v->n] = key; v->n++;
}
static jv *parse_value(void);
static char *parse_string(void)
{
    if (*P != '"') return NULL; P++;
    const char *s = P; size_t cap = 64, n = 0; char *out = malloc(cap);
    while (*P && *P != '"') {
        if (n + 2 > cap) { cap *= 2; out = realloc(out, cap); }
        if (*P == '\\' && P[1]) { P++; out[n++] = *P++; }
        else out[n++] = *P++;
    }
    out[n] = 0; if (*P == '"') P++; (void)s;
    return out;
}
static jv *parse_value(void)
{
    skipws();
    if (*P == '{') {
        P++; jv *o = jnew(5); skipws();
        while (*P && *P != '}') {
            char *k = parse_string(); skipws(); if (*P == ':') P++; jv *val = parse_value();
            push(o, k, val); skipws(); if (*P == ',') P++; skipws();
        }
        if (*P == '}') P++; return o;
    }
    if (*P == '[') {
        P++; jv *a = jnew(4); skipws();
        while (*P && *P != ']') {
            push(a, NULL, parse_value()); skipws(); if (*P == ',') P++; skipws();
        }
        if (*P == ']') P++; return a;
    }
    if (*P == '"') { jv *v = jnew(3); v->str = parse_string(); return v; }
    if (!strncmp(P, "true", 4)) { P += 4; jv *v = jnew(1); v->num = 1; return v; }
    if (!strncmp(P, "false", 5)) { P += 5; jv *v = jnew(1); v->num = 0; return v; }
    if (!strncmp(P, "null", 4)) { P += 4; return jnew(0); }
    char *end; double d = strtod(P, &end);
    if (end != P) { P = end; jv *v = jnew(2); v->num = d; return v; }
    return jnew(0);
}
static jv *obj_get(jv *o, const char *k)
{
    if (!o || o->t != 5) return NULL;
    for (int i = 0; i < o->n; i++) if (o->keys[i] && !strcmp(o->keys[i], k)) return o->items[i];
    return NULL;
}
static jv *arr_at(jv *a, int i) { return a && a->t == 4 && i >= 0 && i < a->n ? a->items[i] : NULL; }
static double num(jv *v, double d) { return v && v->t == 2 ? v->num : d; }
static const char *strv(jv *v) { return v && v->t == 3 ? v->str : NULL; }

/* GL component sizes and counts */
static int comp_size(int ct) { return ct == 5120 || ct == 5121 ? 1 : ct == 5122 || ct == 5123 ? 2 : 4; }
static int type_comps(const char *t)
{
    if (!t) return 0;
    if (!strcmp(t, "SCALAR")) return 1; if (!strcmp(t, "VEC2")) return 2;
    if (!strcmp(t, "VEC3")) return 3;   if (!strcmp(t, "VEC4")) return 4;
    return 0;
}
static double rd(const uint8_t *p, int ct)
{
    if (ct == 5126) { float f; memcpy(&f, p, 4); return f; }
    if (ct == 5125) { uint32_t u; memcpy(&u, p, 4); return u; }
    if (ct == 5123) { uint16_t u; memcpy(&u, p, 2); return u; }
    if (ct == 5121) return p[0];
    int16_t s; memcpy(&s, p, 2); return s;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <in.glb> <out-car4> [--length M]\n", argv[0]); return 2; }
    const char *out = argv[2]; double target_len = 4.4;
    for (int i = 3; i < argc; i++) if (!strcmp(argv[i], "--length") && i + 1 < argc) target_len = atof(argv[++i]);

    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long gl = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *glb = malloc(gl);
    if (!glb || fread(glb, 1, gl, f) != (size_t)gl) { fprintf(stderr, "cannot read %s\n", argv[1]); fclose(f); return 1; }
    fclose(f);
    if (gl < 12 || memcmp(glb, "glTF", 4)) { fprintf(stderr, "not a .glb\n"); return 1; }
    uint32_t jlen = 0, blen = 0; const uint8_t *json = NULL, *bin = NULL;
    for (long o = 12; o + 8 <= gl; ) {
        uint32_t clen, ctype; memcpy(&clen, glb + o, 4); memcpy(&ctype, glb + o + 4, 4); o += 8;
        if (o + clen > (uint32_t)gl) break;
        if (ctype == 0x4E4F534A) { json = glb + o; jlen = clen; }
        else if (ctype == 0x004E4942) { bin = glb + o; blen = clen; }
        o += clen;
    }
    if (!json || !bin) { fprintf(stderr, "glb has no JSON or BIN chunk\n"); return 1; }
    char *jtxt = malloc(jlen + 1); memcpy(jtxt, json, jlen); jtxt[jlen] = 0;
    P = jtxt; jv *root = parse_value();
    jv *accessors = obj_get(root, "accessors"), *bufviews = obj_get(root, "bufferViews");
    jv *mess = obj_get(root, "meshes"), *images = obj_get(root, "images");
    if (!accessors || !bufviews || !mess || !images) { fprintf(stderr, "glb missing arrays\n"); return 1; }
    jv *prim = arr_at(obj_get(arr_at(mess, 0), "primitives"), 0);
    jv *attrs = obj_get(prim, "attributes");

    struct { int bv, ct, comps; size_t off; } A[4];   /* 0 pos, 1 normal, 2 uv, 3 indices */
    const char *names[3] = { "POSITION", "NORMAL", "TEXCOORD_0" };
    int have[3] = {0,0,0};
    for (int k = 0; k < 3; k++) {
        int ai = (int)num(obj_get(attrs, names[k]), -1);
        if (ai < 0) { if (k < 2) { fprintf(stderr, "glb has no %s\n", names[k]); return 1; } continue; }
        jv *ac = arr_at(accessors, ai);
        A[k].bv = (int)num(obj_get(ac, "bufferView"), -1);
        A[k].ct = (int)num(obj_get(ac, "componentType"), 0);
        A[k].comps = type_comps(strv(obj_get(ac, "type")));
        A[k].off = (size_t)num(obj_get(ac, "byteOffset"), 0);
        have[k] = 1;
    }
    int ii = (int)num(obj_get(prim, "indices"), -1);
    if (ii >= 0) { jv *ac = arr_at(accessors, ii); A[3].bv = (int)num(obj_get(ac, "bufferView"), -1); A[3].ct = (int)num(obj_get(ac, "componentType"), 0); A[3].comps = 1; A[3].off = (size_t)num(obj_get(ac, "byteOffset"), 0); }

    int nv = 0, ni = 0;
    { int ai = (int)num(obj_get(attrs, "POSITION"), -1); nv = (int)num(obj_get(arr_at(accessors, ai), "count"), 0); }
    if (ii >= 0) ni = (int)num(obj_get(arr_at(accessors, ii), "count"), 0);
    if (!nv || !ni || ni % 3) { fprintf(stderr, "glb: no triangles\n"); return 1; }

    /* element base pointer and stride for an accessor slot */
    size_t stride[4];
    for (int k = 0; k < 4; k++) {
        jv *bvj = arr_at(bufviews, A[k].bv);
        size_t stride_bytes = (size_t)num(obj_get(bvj, "byteStride"), 0);
        stride[k] = stride_bytes ? stride_bytes : (size_t)A[k].comps * comp_size(A[k].ct);
    }
    #define PTR(a,i) (bin + (size_t)num(obj_get(arr_at(bufviews,(a).bv),"byteOffset"),0) + (a).off + (size_t)(i)*stride[(&(a)-A)])

    /* read vertices, then normalise: centre X/Z, floor at Y=0, length -> target */
    float *pos = malloc((size_t)nv * 3 * sizeof *pos), *nor = have[1] ? malloc((size_t)nv * 3 * sizeof *nor) : NULL;
    float *uv = have[2] ? malloc((size_t)nv * 2 * sizeof *uv) : NULL;
    for (int i = 0; i < nv; i++) {
        const uint8_t *p = PTR(A[0], i);
        float x = (float)rd(p, A[0].ct), y = (float)rd(p + comp_size(A[0].ct), A[0].ct), z = (float)rd(p + 2 * comp_size(A[0].ct), A[0].ct);
        /* mesh X is the banana's long axis -> world Z; keep Y up */
        pos[i*3+0] = -z; pos[i*3+1] = y; pos[i*3+2] = x;
        if (nor) { const uint8_t *q = PTR(A[1], i); nor[i*3+0] = -(float)rd(q + 2*comp_size(A[1].ct), A[1].ct); nor[i*3+1] = (float)rd(q + comp_size(A[1].ct), A[1].ct); nor[i*3+2] = (float)rd(q, A[1].ct); }
        if (uv) { const uint8_t *q = PTR(A[2], i); uv[i*2+0] = (float)rd(q, A[2].ct); uv[i*2+1] = 1.0f - (float)rd(q + comp_size(A[2].ct), A[2].ct); }
    }
    float lo[3] = {1e30f,1e30f,1e30f}, hi[3] = {-1e30f,-1e30f,-1e30f};
    for (int i = 0; i < nv; i++) for (int c = 0; c < 3; c++) { if (pos[i*3+c]<lo[c])lo[c]=pos[i*3+c]; if(pos[i*3+c]>hi[c])hi[c]=pos[i*3+c]; }
    float len = hi[2]-lo[2];            /* world length is along Z */
    float s = len > 1e-6f ? (float)(target_len / len) : 1.0f;
    float cx = (lo[0]+hi[0])*0.5f, cz = (lo[2]+hi[2])*0.5f, ymin = lo[1];
    for (int i = 0; i < nv; i++) {
        pos[i*3+0] = (pos[i*3+0]-cx)*s; pos[i*3+1] = (pos[i*3+1]-ymin)*s; pos[i*3+2] = (pos[i*3+2]-cz)*s;
    }

    /* the embedded PNG */
    int pv = (int)num(obj_get(arr_at(images, 0), "bufferView"), -1);
    const uint8_t *png = NULL; size_t png_len = 0;
    if (pv >= 0) { jv *b = arr_at(bufviews, pv); png = bin + (size_t)num(obj_get(b,"byteOffset"),0); png_len = (size_t)num(obj_get(b,"byteLength"),0); }
    int iw=0, ih=0, ic=0; unsigned char *px = png ? stbi_load_from_memory(png, (int)png_len, &iw, &ih, &ic, 4) : NULL;
    if (px) {   /* the GS has no texture beyond 1024; box-downscale to fit */
        int tw = iw, th = ih;
        while (tw > 1024) tw >>= 1; while (th > 1024) th >>= 1;
        if (tw != iw || th != ih) {
            unsigned char *n = malloc((size_t)tw * th * 4);
            for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
                unsigned sy0 = (unsigned)y * ih / th, sy1 = (unsigned)(y + 1) * ih / th;
                unsigned sx0 = (unsigned)x * iw / tw, sx1 = (unsigned)(x + 1) * iw / tw;
                unsigned s[4] = {0,0,0,0}, n2 = 0;
                for (unsigned sy = sy0; sy < sy1; sy++) for (unsigned sx = sx0; sx < sx1; sx++, n2++)
                    for (int c = 0; c < 4; c++) s[c] += px[((size_t)sy * iw + sx) * 4 + c];
                for (int c = 0; c < 4; c++) n[((size_t)y * tw + x) * 4 + c] = (unsigned char)(n2 ? s[c] / n2 : 0);
            }
            free(px); px = n; iw = tw; ih = th;
        }
    }

    gt4_model *m = calloc(1, sizeof *m);
    m->count = m->capacity = ni / 3;
    m->triangles = malloc((size_t)m->count * sizeof *m->triangles);
    gt4_draw_state st = {0,0x44,0,1,1,1,6,32,0,1,1,3,0};
    for (int t = 0; t < m->count; t++) {
        gt4_triangle *tr = &m->triangles[t];
        for (int k = 0; k < 3; k++) {
            const uint8_t *p = PTR(A[3], t*3+k); int vi = (int)rd(p, A[3].ct);
            gt4_vertex *v = &tr->v[k];
            v->p[0]=pos[vi*3+0]; v->p[1]=pos[vi*3+1]; v->p[2]=pos[vi*3+2];
            if (nor) { v->normal[0]=nor[vi*3+0]; v->normal[1]=nor[vi*3+1]; v->normal[2]=nor[vi*3+2]; }
            if (uv)  { v->uv[0]=uv[vi*2+0]; v->uv[1]=uv[vi*2+1]; }
            v->rgb[0]=v->rgb[1]=v->rgb[2]=1.0f;
            v->ambient[0]=v->ambient[1]=v->ambient[2]=1.0f;
            v->specular[0]=v->specular[1]=v->specular[2]=0.0f;
            v->shine=16.0f; v->alpha=1.0f;
        }
        tr->texture = px ? 0 : -1; tr->kind = 1; tr->attachment = 0; tr->state = st;
    }
    if (px) {
        m->textures = calloc(1, sizeof *m->textures);
        m->ntextures = 1;
        tex1_image *im = &m->textures[0];
        im->width = iw; im->height = ih; im->tex_w = 1; while (im->tex_w < iw) im->tex_w <<= 1; im->tex_h = 1; while (im->tex_h < ih) im->tex_h <<= 1;
        im->rgba = malloc((size_t)iw*ih*4); memcpy(im->rgba, px, (size_t)iw*ih*4);
        im->native_alpha = malloc((size_t)iw*ih);
        for (int i = 0; i < iw*ih; i++) im->native_alpha[i] = px[i*4+3];
        im->color_component = 1; im->texture_function = 0; im->magnify = 1; im->minify = 1;
        im->wrap_u = im->wrap_v = 2;
    }
    printf("%s: %d vertices, %d triangles, PNG %dx%d (%d ch)%s; length %.2f m (scale %.4f)\n",
           argv[1], nv, m->count, iw, ih, ic, px ? "" : " [unreadable: untextured]", target_len, s);

    size_t osz = 0; uint8_t *car = gt4_model_encode_car4(m, &osz);
    if (!car) { fprintf(stderr, "writer refused the model\n"); return 1; }
    mkdir_p(out);
    FILE *o = fopen(out, "wb");
    if (!o || fwrite(car, 1, osz, o) != osz) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fclose(o);
    printf("wrote %s (%zu bytes)\n", out, osz);
    return 0;
}
