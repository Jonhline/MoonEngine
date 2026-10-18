/*
 * moonmod_glbcar: turn a .glb (glTF 2.0 binary) into a GT4 CAR4 car model,
 * the file /car/<menu|lod|open>/<model> expects. This is the SDK's model
 * intake (docs/vulcan4/SDK-MODEL-INTAKE.md): POSITION + NORMAL + TEXCOORD_0
 * and one embedded PNG, converted to the MDLS/Tex1 stream the engine's own
 * reader accepts (src/core/model_write.c). The scale and origin are normalised
 * here; a node named in --rig is tagged as a wheel (attachment 1..4).
 *
 *   moonmod_glbcar <in.glb> <out-car4> [--length M] [--rig rig.json]
 *                  [--wheels-from <disc.iso> <donor label>]
 */
#include "model.h"
#include "model_write.h"
#include "tex1.h"
#include "specdb.h"
#include "vol.h"

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

/* --wheels-from reads a DONOR's rim/tyre off the disc: it must be the donor's
   own, never whatever an already-ticked mod currently shadows that path
   with. With no MOON_OVERLAY set, gt4_vol_read falls back to the user's own
   mods.ini (mod_manifest.c moon_mod_roots()) same as the real game -- a
   sentinel path that resolves to nothing forces every read through to the
   disc instead (see the longer version of this comment in moonmod_addcar.c,
   where the same silent pickup corrupted a string index). */
static void force_pristine_disc(void)
{
#ifdef _WIN32
    _putenv_s("MOON_OVERLAY", "\\\\.\\moonmod-tool-reads-the-disc-only");
#else
    setenv("MOON_OVERLAY", "/moonmod-tool-reads-the-disc-only", 1);
#endif
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

/* ---- node transforms (column-major 4x4, glTF convention: M = T * R * S) ----
   A mesh's own vertices are in the NODE's local space; the authored object
   transform (what Blender etc. write as the node's translation/rotation/scale
   or an explicit matrix) is NOT baked into the accessor data. Reading the
   mesh without composing this was the bug that broke a real donated model
   (the Escort): it rendered as a huge sliver because the node's scale/rotation
   was silently dropped. */
static void mat4_identity(float m[16]) { for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0); }
static void mat4_mul(float r[16], const float a[16], const float b[16])
{
    float t[16];
    for (int c = 0; c < 4; c++) for (int row = 0; row < 4; row++) {
        double s = 0;
        for (int k = 0; k < 4; k++) s += a[k*4+row] * b[c*4+k];
        t[c*4+row] = (float)s;
    }
    memcpy(r, t, sizeof t);
}
static void node_local_matrix(jv *node, float m[16])
{
    jv *mat = obj_get(node, "matrix");
    if (mat && mat->t == 4 && mat->n == 16) {
        for (int i = 0; i < 16; i++) m[i] = (float)num(arr_at(mat, i), i % 5 == 0);
        return;
    }
    double t[3] = {0,0,0}, s[3] = {1,1,1}, q[4] = {0,0,0,1};
    jv *tj = obj_get(node, "translation"), *sj = obj_get(node, "scale"), *qj = obj_get(node, "rotation");
    if (tj) for (int i = 0; i < 3; i++) t[i] = num(arr_at(tj, i), 0);
    if (sj) for (int i = 0; i < 3; i++) s[i] = num(arr_at(sj, i), 1);
    if (qj) for (int i = 0; i < 4; i++) q[i] = num(arr_at(qj, i), i == 3);
    double x=q[0],y=q[1],z=q[2],w=q[3];
    double r[9] = {
        1-2*(y*y+z*z),   2*(x*y+z*w),   2*(x*z-y*w),
          2*(x*y-z*w), 1-2*(x*x+z*z),   2*(y*z+x*w),
          2*(x*z+y*w),   2*(y*z-x*w), 1-2*(x*x+y*y)
    };
    mat4_identity(m);
    for (int c = 0; c < 3; c++) for (int row = 0; row < 3; row++) m[c*4+row] = (float)(r[c*3+row] * s[c]);
    m[12] = (float)t[0]; m[13] = (float)t[1]; m[14] = (float)t[2];
}
/* world matrix of node `target`, by walking up the parent chain (built from
   every node's "children") and composing root-to-leaf. No scene graph at all
   (a minimal/hand-built glb, or an out-of-range index) -> identity, honestly. */
static int node_world_matrix_of(jv *nodes, int target, float out[16])
{
    if (!nodes || nodes->t != 4 || target < 0 || target >= nodes->n) { mat4_identity(out); return 0; }
    int n = nodes->n;
    int *parent = calloc((size_t)n, sizeof *parent);
    for (int i = 0; i < n; i++) parent[i] = -1;
    for (int i = 0; i < n; i++) {
        jv *ch = obj_get(arr_at(nodes, i), "children");
        if (!ch || ch->t != 4) continue;
        for (int k = 0; k < ch->n; k++) { int c = (int)num(arr_at(ch, k), -1); if (c >= 0 && c < n) parent[c] = i; }
    }
    int chain[256], depth = 0;
    for (int i = target; i >= 0 && depth < 256; i = parent[i]) chain[depth++] = i;
    free(parent);
    mat4_identity(out);
    for (int i = depth - 1; i >= 0; i--) {
        float local[16]; node_local_matrix(arr_at(nodes, chain[i]), local);
        mat4_mul(out, out, local);
    }
    return 1;
}
static void mat4_point(const float m[16], float x, float y, float z, float o[3])
{
    for (int row = 0; row < 3; row++) o[row] = m[0*4+row]*x + m[1*4+row]*y + m[2*4+row]*z + m[3*4+row];
}
/* direction only (no translation); correct for rotation + uniform scale,
   which is what every exporter we have seen emits -- a non-uniform scale on
   the node would need the inverse-transpose instead. */
static void mat4_dir(const float m[16], float x, float y, float z, float o[3])
{
    for (int row = 0; row < 3; row++) o[row] = m[0*4+row]*x + m[1*4+row]*y + m[2*4+row]*z;
}

/* ---- rig.json: the SDK viewer's wheel assignments (schema SDK-PACKAGE-SCHEMA.md
   §3.5) -- plain text despite the name, one directive per line:
     wheel  wheel_LF  part=<mesh name>  radius=...  axis=X  steer=1  drive=1
   Attachment order 1..4 = LF, RF, LR, RR -- not a convention we chose: it is
   the engine's own (car_model.c add_wheels: slot = axle*2+side, attachment =
   (slot^1)+1 -- measured front-left=1, front-right=2, rear-left=3,
   rear-right=4, and the schema already names its four rows in that order). */
static const char *RIG_CORNERS[4] = { "wheel_LF", "wheel_RF", "wheel_LR", "wheel_RR" };
/* The SDK's modern viewer shows three.js's own Object3D.name, which three.js
   sanitises for use in animation paths (PropertyBinding.sanitizeNodeName):
   whitespace -> '_', and '[', ']', '.', ':', '/' dropped outright. So a node
   genuinely named "GEO_rim_SUB0.001" is what the author sees and clicks as
   "GEO_rim_SUB0001" in the GUI, and that's what rig.json's part= carries.
   Comparing raw glTF names against that would never match a source whose
   names collide only once sanitised (every ".001" Blender suffix does) --
   normalise both sides the same way instead of trusting either is "the"
   name. */
static void sanitize_node_name(const char *src, char *out, size_t cap)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && n + 1 < cap; p++) {
        if (*p == '[' || *p == ']' || *p == '.' || *p == ':' || *p == '/') continue;
        out[n++] = (*p == ' ' || *p == '\t') ? '_' : (char)*p;
    }
    out[n] = 0;
}
static char rig_forward[8], rig_up[8]; /* the rig's "forward -Z" / "up +Y", "" when absent */
static void parse_rig(const char *path, char *wheel_part[4])
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "--rig %s: cannot open\n", path); return; }
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char directive[32], corner[32];
        if (sscanf(line, "%31s %31s", directive, corner) != 2) continue;
        if (!strcmp(directive, "forward") || !strcmp(directive, "up")) {
            snprintf(!strcmp(directive, "up") ? rig_up : rig_forward, 8, "%s", corner);
            continue;
        }
        if (strcmp(directive, "wheel")) continue;
        int slot = -1;
        for (int i = 0; i < 4; i++) if (!strcmp(corner, RIG_CORNERS[i])) slot = i;
        if (slot < 0) continue;
        const char *p = strstr(line, "part=");
        if (!p) continue;
        p += 5;
        char name[128]; int n = 0;
        while (p[n] && p[n] != ' ' && p[n] != '\t' && p[n] != '\n' && p[n] != '\r' && n < 127) { name[n] = p[n]; n++; }
        name[n] = 0;
        if (n) {
            char clean[128]; sanitize_node_name(name, clean, sizeof clean);
            free(wheel_part[slot]); wheel_part[slot] = strdup(clean);
        }
    }
    fclose(f);
}
/* which attachment (1..4), if any, a node/mesh name was assigned in the rig */
static int rig_attachment(char *wheel_part[4], const char *name)
{
    if (!name) return 0;
    char clean[128]; sanitize_node_name(name, clean, sizeof clean);
    for (int i = 0; i < 4; i++) if (wheel_part[i] && !strcmp(wheel_part[i], clean)) return i + 1;
    return 0;
}

/* --wheels-from <disc.iso> <donor label>: the same label -> model name chain
   moonmod_addcar uses (GENERIC_CAR.idi -> CAR_VARIATION_american.dbt ->
   VARIATIONamerican.dbt -> american_StrDB.sdb), so the donor's rim/tyre can be
   read from its own /car/menu/<model> without a second tool. */
static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint8_t *donor_car4(gt4_vol *v, const char *label, size_t *out_size)
{
    size_t idi_sz = 0, link_sz = 0, var_sz = 0, str_sz = 0;
    uint8_t *idi = gt4_vol_read(v, "/specdb/GT4_US2560/GENERIC_CAR.idi", &idi_sz);
    uint8_t *links = gt4_vol_read(v, "/specdb/GT4_US2560/CAR_VARIATION_american.dbt", &link_sz);
    uint8_t *variants = gt4_vol_read(v, "/specdb/GT4_US2560/VARIATIONamerican.dbt", &var_sz);
    uint8_t *strdb = gt4_vol_read(v, "/specdb/GT4_US2560/american_StrDB.sdb", &str_sz);
    uint8_t *result = NULL;
    uint32_t id; uint8_t link_row[256], var_row[256];
    if (idi && links && variants && strdb && gt4_specdb_label(idi, idi_sz, label, &id) &&
        gt4_specdb_row(links, link_sz, id, link_row, sizeof link_row) == 8 &&
        gt4_specdb_row(variants, var_sz, le32(link_row), var_row, sizeof var_row) == 52) {
        const char *model = gt4_specdb_string(strdb, str_sz, le32(var_row));
        if (model && strlen(model) < 24 && !strchr(model, '/') && !strchr(model, '\\')) {
            char path[96];
            snprintf(path, sizeof path, "/car/menu/%s", model);
            result = gt4_vol_read(v, path, out_size);
        }
    }
    free(idi); free(links); free(variants); free(strdb);
    return result;
}

/* The GS's alpha runs 0..0x80 (0x80 = 1.0): an image's 0..255 alpha written as is reads
   as up to 2.0, and the car's (Cs-Cd)*As+Cd blend then gives 2*Cs-Cd -- black over a light
   background, washed out over a dark one (the half-black banana). */
/* GT4's own cars wear dark tinted glass, the cabin only glimpsed: a source's clear glass
   at its own alpha showed the whole cabin. The car's draw state takes opacity from the
   texel alone (the vertex alpha is not opacity there), so the tint is written into it. */
#define GLASS_TEXEL(px) do { unsigned char *g_ = (px);     for (int c_ = 0; c_ < 3; c_++) g_[c_] = (unsigned char)(8 + g_[c_] / 16); g_[3] = 235; /* the disc cars' dark glass, matched by eye at Tsukuba: the cabin a silhouette */ } while (0)
#define GS_ALPHA(a) ((uint8_t)(((unsigned)(a) * 128u + 127u) / 255u))

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

/* one mesh-bearing node, read in its own local space and lifted into world
   space by its node transform; attachment is resolved from --rig before the
   global normalise pass (so wheel geometry is tagged like everything else). */
typedef struct {
    float *pos, *nor, *uv;
    int *idx;
    int nv, ni;
    int attachment;
    int image;   /* glTF image index this part's primitive uses, or -1 */
    int alpha_mode;      /* the material's alphaMode: 0 OPAQUE, 1 MASK, 2 BLEND */
    float cutoff;        /* MASK's alphaCutoff */
    float factor[4];     /* baseColorFactor */
    int wheelish;        /* named like a tyre/rim/wheel/hub: a wheel when the rig names none */
    int double_sided;    /* the material's doubleSided: both faces drawn */
    int glass;           /* a window: drawn as GT4's own cars draw theirs, dark */
    int paint;           /* bodywork paint: gets the disc cars' reflection layer */
    int drop;            /* left out of the car (interior, or a wheel the donor's replaces) */
    char name[128];
} part;

/* the material's alpha handling and colour factor (glTF 2.0 3.9.4: OPAQUE ignores the
   image's alpha, MASK cuts at alphaCutoff, BLEND keeps it) */
static void material_of(jv *materials, jv *prim, part *pt)
{
    jv *mt = arr_at(materials, (int)num(obj_get(prim, "material"), -1));
    const char *mode = strv(obj_get(mt, "alphaMode"));
    pt->alpha_mode = mode && !strcmp(mode, "MASK") ? 1 : mode && !strcmp(mode, "BLEND") ? 2 : 0;
    pt->cutoff = (float)num(obj_get(mt, "alphaCutoff"), 0.5);
    jv *f = obj_get(obj_get(mt, "pbrMetallicRoughness"), "baseColorFactor");
    for (int k = 0; k < 4; k++) pt->factor[k] = (float)num(arr_at(f, k), 1.0);
    jv *ds = obj_get(mt, "doubleSided");
    /* both faces only where it shows: glass and cut-outs seen from inside the car. A closed
       opaque shell flagged doubleSided (exporters set it everywhere) would only double the
       triangle count for faces nobody sees. */
    pt->double_sided = ds && ds->t == 1 && ds->num != 0 && pt->alpha_mode != 0;
}
static int has_word(const char *s, const char *w)
{
    char a[256], b[64]; size_t i;
    for (i = 0; s && s[i] && i + 1 < sizeof a; i++) a[i] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    a[i] = 0;
    for (i = 0; w[i] && i + 1 < sizeof b; i++) b[i] = w[i];
    b[i] = 0;
    return strstr(a, b) != NULL;
}
/* the cabin (kept by default: it shows through the glass, as on the disc's cars; --no-interior
   leaves it out for a lighter model).
   A material named INT_* (the usual exporter split) or a node of the cockpit's parts. */
static int is_interior(const char *node, const char *material)
{
    static const char *const words[] = {"cockpit", "seat", "dashboard", "gauge", "needle", "arrow",
                                        "cinture", "seatbelt", "gearshift", "shift_", "pedal",
                                        "steer_hr", "steering_wheel", "steeringwheel", "driver",
                                        "interior", "int_glass", "headliner"};
    if (material && (material[0] == 'I' || material[0] == 'i') &&
        (material[1] == 'N' || material[1] == 'n') && (material[2] == 'T' || material[2] == 't') &&
        material[3] == '_')
        return 1;
    for (size_t k = 0; k < sizeof words / sizeof *words; k++)
        if (has_word(node, words[k])) return 1;
    return 0;
}
static int is_wheelish(const char *node)
{
    if (!node || has_word(node, "steer") || has_word(node, "spare")) return 0;
    int rim = has_word(node, "rim") && !has_word(node, "trim") && !has_word(node, "prim");
    return has_word(node, "tire") || has_word(node, "tyre") || rim ||
           has_word(node, "wheel") || has_word(node, "hub") || has_word(node, "caliper") ||
           has_word(node, "brake") || has_word(node, "disk") || has_word(node, "rotor");
}
/* (the brakes count: they sit at the model's own hub, so they leave with its wheels) */
/* +1 a front part, -1 a rear one, 0 neither, by its name */
static int front_or_rear(const char *node)
{
    if (!node) return 0;
    static const char *const front[] = {"front", "head", "_lf", "_rf", "_fl", "_fr", "lf_", "rf_", "fl_", "fr_", "grille", "bumper_f"};
    static const char *const rear[] = {"rear", "tail", "realight", "_lr", "_rr", "_rl", "lr_", "rr_", "rl_", "exhaust", "bumper_r"};
    for (size_t k = 0; k < sizeof front / sizeof *front; k++) if (has_word(node, front[k])) return 1;
    for (size_t k = 0; k < sizeof rear / sizeof *rear; k++) if (has_word(node, rear[k])) return -1;
    return 0;
}

/* a primitive's own base-colour image, via its material -> baseColorTexture
   -> texture -> source chain -- a multi-material source (interior, glass,
   paint, tyres...) must not all be flattened onto one image. -1 when the
   primitive names no material/texture at all. */
static int primitive_image_index(jv *materials, jv *textures_arr, jv *prim)
{
    int mat_idx = (int)num(obj_get(prim, "material"), -1);
    jv *pbr = obj_get(arr_at(materials, mat_idx), "pbrMetallicRoughness");
    jv *bct = obj_get(pbr, "baseColorTexture");
    int tex_idx = (int)num(obj_get(bct, "index"), -1);
    return (int)num(obj_get(arr_at(textures_arr, tex_idx), "source"), -1);
}
/* decode one glTF image (its PNG bufferView) to RGBA, downscaled to fit the
   GS's 1024-texel ceiling -- the same box filter for every image, not just
   the first. NULL on anything unreadable (not fatal: that part is untextured). */
/* box-filter an RGBA buffer down to tw x th (no-op, same pointer, if it
   already fits); used both for the per-image 1024 cap and for the whole
   car's VRAM budget pass below. */
static unsigned char *box_resize(unsigned char *px, int iw, int ih, int tw, int th)
{
    if (tw == iw && th == ih) return px;
    if (tw < 1) tw = 1; if (th < 1) th = 1;
    unsigned char *n = malloc((size_t)tw * th * 4);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
        unsigned sy0 = (unsigned)y * ih / th, sy1 = (unsigned)(y + 1) * ih / th;
        unsigned sx0 = (unsigned)x * iw / tw, sx1 = (unsigned)(x + 1) * iw / tw;
        if (sy1 <= sy0) sy1 = sy0 + 1; if (sx1 <= sx0) sx1 = sx0 + 1;
        unsigned s[4] = {0,0,0,0}, n2 = 0;
        for (unsigned sy = sy0; sy < sy1 && sy < (unsigned)ih; sy++)
            for (unsigned sx = sx0; sx < sx1 && sx < (unsigned)iw; sx++, n2++)
                for (int c = 0; c < 4; c++) s[c] += px[((size_t)sy * iw + sx) * 4 + c];
        for (int c = 0; c < 4; c++) n[((size_t)y * tw + x) * 4 + c] = (unsigned char)(n2 ? s[c] / n2 : 0);
    }
    free(px);
    return n;
}
static int image_cap = 1024; /* the GS's side; the PC copy (car/hires) reads up to 4096 */
static unsigned char *load_image_rgba(jv *images, jv *bufviews, const uint8_t *bin, int image_index,
                                      int *out_w, int *out_h, int *out_c)
{
    jv *imgj = arr_at(images, image_index);
    int pv = (int)num(obj_get(imgj, "bufferView"), -1);
    if (pv < 0) return NULL;
    jv *b = arr_at(bufviews, pv);
    const uint8_t *png = bin + (size_t)num(obj_get(b, "byteOffset"), 0);
    size_t png_len = (size_t)num(obj_get(b, "byteLength"), 0);
    int iw = 0, ih = 0, ic = 0;
    unsigned char *px = stbi_load_from_memory(png, (int)png_len, &iw, &ih, &ic, 4);
    if (!px) return NULL;
    int tw = iw, th = ih;
    while (tw > image_cap) tw >>= 1; while (th > image_cap) th >>= 1;
    px = box_resize(px, iw, ih, tw, th);
    iw = tw; ih = th;
    *out_w = iw; *out_h = ih; *out_c = ic;
    return px;
}

/* --rim / --tire: a SEPARATE, small .glb (mesh[0], one primitive), read with
   none of the main body's normalise pass -- model_write.h says why: a rim is
   authored at radius 1.0 around local X and the engine scales it uniformly;
   a tyre's own absolute scale does not matter at all, only its radial shape,
   so re-centring or rescaling it here would only throw away the author's own
   units for no reason. Node transform is still applied (an object is rarely
   authored sitting exactly at its mesh origin). NULL (with a message on
   stderr) if the file cannot be read as a single-mesh glb. */
static gt4_model *load_glb_as_model(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long gl = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *glb = malloc(gl > 0 ? (size_t)gl : 1);
    if (!glb || gl <= 0 || fread(glb, 1, (size_t)gl, f) != (size_t)gl) {
        fprintf(stderr, "cannot read %s\n", path); fclose(f); free(glb); return NULL;
    }
    fclose(f);
    if (gl < 12 || memcmp(glb, "glTF", 4)) { fprintf(stderr, "%s: not a .glb\n", path); free(glb); return NULL; }
    uint32_t jlen = 0; const uint8_t *json = NULL, *bin = NULL;
    for (long o = 12; o + 8 <= gl; ) {
        uint32_t clen, ctype; memcpy(&clen, glb + o, 4); memcpy(&ctype, glb + o + 4, 4); o += 8;
        if (o + clen > (uint32_t)gl) break;
        if (ctype == 0x4E4F534A) { json = glb + o; jlen = clen; }
        else if (ctype == 0x004E4942) bin = glb + o;
        o += clen;
    }
    if (!json || !bin) { fprintf(stderr, "%s: glb has no JSON or BIN chunk\n", path); free(glb); return NULL; }
    char *jtxt = malloc((size_t)jlen + 1); memcpy(jtxt, json, jlen); jtxt[jlen] = 0;
    P = jtxt; jv *root = parse_value();
    jv *accessors = obj_get(root, "accessors"), *bufviews = obj_get(root, "bufferViews");
    jv *mess = obj_get(root, "meshes"), *images = obj_get(root, "images");
    jv *nodes = obj_get(root, "nodes");
    jv *materials = obj_get(root, "materials"), *gltf_textures = obj_get(root, "textures");
    if (!accessors || !bufviews || !mess || !mess->n) { fprintf(stderr, "%s: glb has no mesh\n", path); free(glb); return NULL; }
    jv *prim = arr_at(obj_get(arr_at(mess, 0), "primitives"), 0);
    jv *attrs = obj_get(prim, "attributes");

    struct { int bv, ct, comps; size_t off; } A[4];
    const char *names[3] = { "POSITION", "NORMAL", "TEXCOORD_0" };
    int have[3] = {0,0,0};
    for (int k = 0; k < 3; k++) {
        int ai = (int)num(obj_get(attrs, names[k]), -1);
        if (ai < 0) { if (k < 2) { fprintf(stderr, "%s: no %s\n", path, names[k]); free(glb); return NULL; } continue; }
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
    if (!nv || !ni || ni % 3) { fprintf(stderr, "%s: no triangles\n", path); free(glb); return NULL; }

    size_t stride[4];
    for (int k = 0; k < 4; k++) {
        jv *bvj = arr_at(bufviews, A[k].bv);
        size_t stride_bytes = (size_t)num(obj_get(bvj, "byteStride"), 0);
        stride[k] = stride_bytes ? stride_bytes : (size_t)A[k].comps * comp_size(A[k].ct);
    }
    #define PTR(a,i) (bin + (size_t)num(obj_get(arr_at(bufviews,(a).bv),"byteOffset"),0) + (a).off + (size_t)(i)*stride[(&(a)-A)])
    float xf[16];
    node_world_matrix_of(nodes, 0, xf);

    float *pos = malloc((size_t)nv * 3 * sizeof *pos);
    float *nor = have[1] ? malloc((size_t)nv * 3 * sizeof *nor) : NULL;
    float *uv = have[2] ? malloc((size_t)nv * 2 * sizeof *uv) : NULL;
    for (int v = 0; v < nv; v++) {
        const uint8_t *p = PTR(A[0], v);
        float x = (float)rd(p, A[0].ct), y = (float)rd(p + comp_size(A[0].ct), A[0].ct), z = (float)rd(p + 2*comp_size(A[0].ct), A[0].ct);
        mat4_point(xf, x, y, z, &pos[v*3]);
        if (nor) {
            const uint8_t *q = PTR(A[1], v);
            float nx = (float)rd(q, A[1].ct), ny = (float)rd(q + comp_size(A[1].ct), A[1].ct), nz = (float)rd(q + 2*comp_size(A[1].ct), A[1].ct);
            mat4_dir(xf, nx, ny, nz, &nor[v*3]);
        }
        if (uv) { const uint8_t *q = PTR(A[2], v); uv[v*2+0] = (float)rd(q, A[2].ct); float vv = (float)rd(q + comp_size(A[2].ct), A[2].ct); /* glTF's V runs down, as the GS's T does */ uv[v*2+1] = vv - floorf(vv); }
    }
    int *idx = malloc((size_t)ni * sizeof *idx);
    for (int t = 0; t < ni; t++) { const uint8_t *p = PTR(A[3], t); idx[t] = (int)rd(p, A[3].ct); }
    #undef PTR

    int img_idx = primitive_image_index(materials, gltf_textures, prim);
    if (img_idx < 0 && images && images->n == 1) img_idx = 0;
    unsigned char *px = NULL; int iw = 0, ih = 0, ic = 0;
    if (img_idx >= 0) px = load_image_rgba(images, bufviews, bin, img_idx, &iw, &ih, &ic);

    gt4_model *m = calloc(1, sizeof *m);
    m->count = m->capacity = ni / 3;
    m->triangles = malloc((size_t)m->count * sizeof *m->triangles);
    gt4_draw_state st = {0,0x44,0,1,1,1,6,32,0,1,1,3,0};
    for (int t = 0; t < m->count; t++) {
        gt4_triangle *tr = &m->triangles[t];
        for (int k = 0; k < 3; k++) {
            int vi = idx[t*3+k];
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
        im->rgba = px;
        im->native_alpha = malloc((size_t)iw*ih);
        for (int p = 0; p < iw*ih; p++) im->native_alpha[p] = GS_ALPHA(px[p*4+3]);
        im->color_component = 1; im->texture_function = 0; im->magnify = 1; im->minify = 1;
        im->wrap_u = im->wrap_v = 2;
    }
    printf("%s: %d vertices, %d triangles%s\n", path, nv, m->count, px ? ", textured" : "");
    free(pos); free(nor); free(uv); free(idx); free(glb); free(jtxt);
    return m;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <in.glb> <out-car4> [--length M] [--rig rig.json]\n", argv[0]);
        return 2;
    }
    force_pristine_disc();
    const char *out = argv[2]; double target_len = 4.4; const char *rig_path = NULL;
    const char *wheels_disc = NULL, *wheels_label = NULL;
    const char *rim_path = NULL, *tire_path = NULL, *forward_arg = NULL;
    int keep_interior = 1; /* the cabin shows through the glass (--no-interior drops it) */
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--length") && i + 1 < argc) target_len = atof(argv[++i]);
        else if (!strcmp(argv[i], "--rig") && i + 1 < argc) rig_path = argv[++i];
        else if (!strcmp(argv[i], "--wheels-from") && i + 2 < argc) { wheels_disc = argv[++i]; wheels_label = argv[++i]; }
        else if (!strcmp(argv[i], "--rim") && i + 1 < argc) rim_path = argv[++i];
        else if (!strcmp(argv[i], "--tire") && i + 1 < argc) tire_path = argv[++i];
        else if (!strcmp(argv[i], "--forward") && i + 1 < argc) forward_arg = argv[++i];
        else if (!strcmp(argv[i], "--keep-interior")) keep_interior = 1;
        else if (!strcmp(argv[i], "--no-interior")) keep_interior = 0;
    }
    if ((rim_path || tire_path) && wheels_disc) {
        fprintf(stderr, "--rim/--tire and --wheels-from are two different wheels (authored vs. borrowed) -- pick one\n");
        return 2;
    }
    char *wheel_part[4] = {0,0,0,0};
    if (rig_path) parse_rig(rig_path, wheel_part);

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
    jv *nodes = obj_get(root, "nodes");
    jv *materials = obj_get(root, "materials"), *gltf_textures = obj_get(root, "textures");
    if (!accessors || !bufviews || !mess || !images) { fprintf(stderr, "glb missing arrays\n"); return 1; }

    /* Every node that carries a mesh becomes a part, read in its own local
       space then lifted by its node transform (SCALE/ROTATION/TRANSLATION,
       composed through any parent chain -- the bug that broke a real donated
       model: the converter used to read mesh[0] raw and ignore this). A node
       with no entry in the scene graph at all (a minimal/hand-built glb)
       still works: its world matrix is just identity. */
    int node_count = nodes && nodes->t == 4 ? nodes->n : 0;
    int part_count = 0;
    for (int i = 0; i < node_count; i++) if (obj_get(arr_at(nodes, i), "mesh")) part_count++;
    if (!part_count && mess->n) part_count = 1;   /* no "nodes" array at all: fall back to mesh[0] */
    part *parts = calloc((size_t)part_count, sizeof *parts);
    int pi = 0;

    const char *only_node = getenv("MOONMOD_ONLY_NODE");
    for (int i = 0; i < (node_count ? node_count : 1); i++) {
        jv *node = node_count ? arr_at(nodes, i) : NULL;
        if (node_count && !obj_get(node, "mesh")) continue;
        if (only_node) {
            const char *nm = strv(obj_get(node, "name"));
            if (!nm || strcmp(nm, only_node)) continue;
        }
        int mesh_index = node ? (int)num(obj_get(node, "mesh"), 0) : 0;
        jv *mesh = arr_at(mess, mesh_index);
        jv *prim = arr_at(obj_get(mesh, "primitives"), 0);
        jv *attrs = obj_get(prim, "attributes");
        const char *part_name = node ? strv(obj_get(node, "name")) : NULL;
        if (!part_name) part_name = strv(obj_get(mesh, "name"));

        /* --wheels-from already supplies a complete rim+tyre at the hub: the
           source model's OWN wheel mesh, tagged via --rig, would draw a SECOND
           wheel at that same hub (the attachment-tagged triangles ride along as
           "brake" dressing, car_model.c add_wheels) -- measured as a visibly
           doubled wheel. Until a model's own wheel can survive the engine's
           tyre-fit deformation (docs/vulcan4/research/wheels-and-cameras.md
           §A2/A3, still unmeasured), a tagged part is dropped here rather than
           exported to collide with the donor's. */
        const char *material_name = strv(obj_get(arr_at(materials, (int)num(obj_get(prim, "material"), -1)), "name"));
        int interior = !keep_interior && is_interior(part_name, material_name);

        struct { int bv, ct, comps; size_t off; } A[4];   /* 0 pos, 1 normal, 2 uv, 3 indices */
        const char *names[3] = { "POSITION", "NORMAL", "TEXCOORD_0" };
        int have[3] = {0,0,0};
        for (int k = 0; k < 3; k++) {
            int ai = (int)num(obj_get(attrs, names[k]), -1);
            if (ai < 0) { if (k < 2) { fprintf(stderr, "glb: %s has no %s\n", part_name ? part_name : "(mesh)", names[k]); return 1; } continue; }
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
        if (!nv || !ni || ni % 3) { fprintf(stderr, "glb: %s has no triangles\n", part_name ? part_name : "(mesh)"); return 1; }

        size_t stride[4];
        for (int k = 0; k < 4; k++) {
            jv *bvj = arr_at(bufviews, A[k].bv);
            size_t stride_bytes = (size_t)num(obj_get(bvj, "byteStride"), 0);
            stride[k] = stride_bytes ? stride_bytes : (size_t)A[k].comps * comp_size(A[k].ct);
        }
        #define PTR(a,i) (bin + (size_t)num(obj_get(arr_at(bufviews,(a).bv),"byteOffset"),0) + (a).off + (size_t)(i)*stride[(&(a)-A)])

        float xf[16];
        int had_xf = node_count ? node_world_matrix_of(nodes, i, xf) : (mat4_identity(xf), 0);
        if (had_xf)
            printf("%s: %s: applying node transform (object was not authored at the origin)\n",
                   argv[1], part_name ? part_name : "(mesh)");

        part *pt = &parts[pi++];
        pt->nv = nv; pt->ni = ni;
        pt->pos = malloc((size_t)nv * 3 * sizeof *pt->pos);
        pt->nor = have[1] ? malloc((size_t)nv * 3 * sizeof *pt->nor) : NULL;
        pt->uv = have[2] ? malloc((size_t)nv * 2 * sizeof *pt->uv) : NULL;
        pt->idx = malloc((size_t)ni * sizeof *pt->idx);
        pt->attachment = rig_attachment(wheel_part, part_name);
        pt->wheelish = is_wheelish(part_name);
        pt->drop = interior;
        snprintf(pt->name, sizeof pt->name, "%s", part_name ? part_name : "(mesh)");
        material_of(materials, prim, pt);
        pt->glass = pt->alpha_mode == 2 &&
                    (has_word(material_name, "window") || has_word(part_name, "window") ||
                     has_word(material_name, "windscreen") || has_word(part_name, "windscreen"));
        /* windows only: lenses (whatever their spelling) and gauge glass keep their colour */
        pt->paint = !interior && pt->alpha_mode == 0 &&
                    (has_word(material_name, "paint") || has_word(material_name, "body"));
        pt->image = primitive_image_index(materials, gltf_textures, prim);
        if (pt->image < 0 && images->n == 1) pt->image = 0;   /* one image, no material graph: assume it */
        for (int v = 0; v < nv; v++) {
            const uint8_t *p = PTR(A[0], v);
            float x = (float)rd(p, A[0].ct), y = (float)rd(p + comp_size(A[0].ct), A[0].ct), z = (float)rd(p + 2 * comp_size(A[0].ct), A[0].ct);
            mat4_point(xf, x, y, z, &pt->pos[v*3]);
            if (pt->nor) {
                const uint8_t *q = PTR(A[1], v);
                float nx = (float)rd(q, A[1].ct), ny = (float)rd(q + comp_size(A[1].ct), A[1].ct), nz = (float)rd(q + 2*comp_size(A[1].ct), A[1].ct);
                mat4_dir(xf, nx, ny, nz, &pt->nor[v*3]);
            }
            /* UVs outside [0,1) (a source authored to tile, or a DDS-flipped one): a
               power-of-two image is written REPEAT (model_write.c), and each triangle
               is moved into [0,1) whole when it is emitted, never vertex by vertex. */
            if (pt->uv) { /* raw: a triangle is shifted into [0,1) whole, when it is emitted */
                const uint8_t *q = PTR(A[2], v);
                pt->uv[v*2+0] = (float)rd(q, A[2].ct);
                /* glTF's V runs down from the image's first row, as the GS's T does:
                   no flip (1 - v drew every image upside down: mirrored lettering, the
                   body sampling the atlas's black gaps) */
                pt->uv[v*2+1] = (float)rd(q + comp_size(A[2].ct), A[2].ct);
            }
        }
        for (int t = 0; t < ni; t++) { const uint8_t *p = PTR(A[3], t); pt->idx[t] = (int)rd(p, A[3].ct); }
        #undef PTR
        if (!node_count) break;   /* the mesh[0]-only fallback above */
    }
    part_count = pi;

    /* Which way the source faces. The engine's cars face -Z with +X on their right (a
       disc car's front hubs sit at z < 0: gt4car_survey --wheels m5_05 gives -1.44); glTF
       2.0 faces +Z (3.4). The parts' own names say it best (lights, tyres, bumpers):
       their front-minus-rear centre picks the axis; then --forward, then the rig's
       "forward", then glTF's +Z. */
    char forward[8] = "+Z";
    const char *forward_from = "glTF's own convention";
    {
        double fsum[3] = {0, 0, 0}, rsum[3] = {0, 0, 0};
        int fn = 0, rn = 0;
        for (int pj = 0; pj < part_count; pj++) {
            int fr = front_or_rear(parts[pj].name);
            if (!fr || !parts[pj].nv) continue;
            double c[3] = {0, 0, 0};
            for (int i = 0; i < parts[pj].nv; i++)
                for (int k = 0; k < 3; k++) c[k] += parts[pj].pos[i * 3 + k] / parts[pj].nv;
            for (int k = 0; k < 3; k++) (fr > 0 ? fsum : rsum)[k] += c[k];
            if (fr > 0) fn++; else rn++;
        }
        if (fn && rn) {
            double dx = fsum[0] / fn - rsum[0] / rn, dz = fsum[2] / fn - rsum[2] / rn;
            snprintf(forward, sizeof forward, "%s",
                     fabs(dz) >= fabs(dx) ? (dz > 0 ? "+Z" : "-Z") : (dx > 0 ? "+X" : "-X"));
            forward_from = "the parts' names (front vs rear)";
        }
        const char *asked = forward_arg ? forward_arg : rig_forward[0] ? rig_forward : NULL;
        if (asked && fn && rn && strcmp(asked, forward))
            printf("%s: WARNING: %s says the car faces %s, but its own parts face %s -- using %s\n",
                   argv[1], forward_arg ? "--forward" : "rig.json", asked, forward, forward);
        else if (asked && !(fn && rn)) {
            snprintf(forward, sizeof forward, "%s", asked);
            forward_from = forward_arg ? "--forward" : "rig.json";
        }
        if (rig_up[0] && strcmp(rig_up, "+Y"))
            printf("%s: WARNING: rig.json says up is %s; glTF is always +Y up, so it is ignored\n", argv[1], rig_up);
    }
    printf("%s: the car faces %s (%s); turned to the engine's -Z\n", argv[1], forward, forward_from);
    for (int pj = 0; pj < part_count; pj++)
        for (int i = 0; i < parts[pj].nv; i++)
            for (int k = 0; k < 2; k++) {
                float *v = k ? (parts[pj].nor ? &parts[pj].nor[i * 3] : NULL) : &parts[pj].pos[i * 3];
                if (!v) continue;
                float x = v[0], z = v[2];
                if (!strcmp(forward, "+Z")) { v[0] = -x; v[2] = -z; }   /* a half turn about Y */
                else if (!strcmp(forward, "+X")) { v[0] = z; v[2] = -x; }
                else if (!strcmp(forward, "-X")) { v[0] = -z; v[2] = x; }
            }

    /* Wheels by name when the rig names none: each tyre/rim/hub part goes to the corner
       its centre sits in (front z < 0, right x > 0; attachments 1..4 = LF, RF, LR, RR). */
    int rig_named = 0;
    for (int i = 0; i < 4; i++) rig_named |= wheel_part[i] != NULL;
    if (!rig_named) {
        int tagged = 0;
        for (int pj = 0; pj < part_count; pj++) {
            if (!parts[pj].wheelish || parts[pj].drop || !parts[pj].nv) continue;
            double cx = 0, cz = 0;
            for (int i = 0; i < parts[pj].nv; i++) { cx += parts[pj].pos[i * 3]; cz += parts[pj].pos[i * 3 + 2]; }
            int front = cz < 0, right = cx > 0;
            parts[pj].attachment = front ? (right ? 2 : 1) : (right ? 4 : 3);
            tagged++;
        }
        if (tagged)
            printf("%s: %d wheel part(s) found by name (tyre/rim/hub) and given their corner\n", argv[1], tagged);
    }
    /* the donor's rim and tyre replace the model's own: a tagged part would draw a SECOND
       wheel at the same hub (the attachment-tagged triangles ride along as "brake" dressing,
       car_model.c add_wheels) -- measured as a visibly doubled wheel */
    int interior_parts = 0, replaced = 0;
    for (int pj = 0; pj < part_count; pj++) {
        if (parts[pj].drop) interior_parts++;
        else if (wheels_disc && parts[pj].attachment) { parts[pj].drop = 1; replaced++; }
    }
    if (interior_parts)
        printf("%s: %d interior part(s) left out (--no-interior)\n", argv[1], interior_parts);
    if (replaced)
        printf("%s: %d wheel part(s) of the model left out -- the donor's rim/tyre take their place\n", argv[1], replaced);

    /* normalise across EVERY part together: centre X/Z, floor at Y=0, scale
       so the combined bounding box's length (world Z) matches --length. A
       wheel placed off to one side must not get re-centred on its own. */
    float lo[3] = {1e30f,1e30f,1e30f}, hi[3] = {-1e30f,-1e30f,-1e30f};
    for (int pj = 0; pj < part_count; pj++)
        for (int i = 0; i < (parts[pj].drop && !parts[pj].attachment ? 0 : parts[pj].nv); i++)
            for (int c = 0; c < 3; c++) {
                float v = parts[pj].pos[i*3+c];
                if (v < lo[c]) lo[c] = v;
                if (v > hi[c]) hi[c] = v;
            }
    float len = hi[2]-lo[2];            /* world length is along Z (the documented -Z-forward contract) */
    float s = len > 1e-6f ? (float)(target_len / len) : 1.0f;
    float cx = (lo[0]+hi[0])*0.5f, cz = (lo[2]+hi[2])*0.5f, ymin = lo[1];
    for (int pj = 0; pj < part_count; pj++)
        for (int i = 0; i < parts[pj].nv; i++) {
            float *p = &parts[pj].pos[i*3];
            p[0] = (p[0]-cx)*s; p[1] = (p[1]-ymin)*s; p[2] = (p[2]-cz)*s;
        }

    /* The model's own wheels, measured (each corner's tyre, else everything at that corner):
       centre, radius, width. The engine draws a car's wheels at +-wheelbase/2 about its
       origin (car_model.c add_wheels), so the body is moved to centre on the axles, and the
       numbers go to car/wheels/<model> for the drawing only -- the physics keeps the
       donor's tables. */
    float wheel_c[4][3], wheel_r[4], wheel_w[4];
    int wheels_known = 0;
    {
        int have = 0;
        for (int corner = 1; corner <= 4; corner++) {
            float lo3[3] = {1e30f, 1e30f, 1e30f}, hi3[3] = {-1e30f, -1e30f, -1e30f};
            for (int pass = 0; pass < 2 && lo3[0] > hi3[0]; pass++)
                for (int pj = 0; pj < part_count; pj++) {
                    if (parts[pj].attachment != corner) continue;
                    if (!pass && !has_word(parts[pj].name, "tire") && !has_word(parts[pj].name, "tyre")) continue;
                    for (int i = 0; i < parts[pj].nv; i++)
                        for (int k = 0; k < 3; k++) {
                            float q = parts[pj].pos[i * 3 + k];
                            if (q < lo3[k]) lo3[k] = q;
                            if (q > hi3[k]) hi3[k] = q;
                        }
                }
            if (lo3[0] > hi3[0]) continue;
            for (int k = 0; k < 3; k++) wheel_c[corner - 1][k] = (lo3[k] + hi3[k]) * .5f;
            wheel_r[corner - 1] = (hi3[1] - lo3[1]) * .5f;
            wheel_w[corner - 1] = hi3[0] - lo3[0];
            have++;
        }
        if (have == 4) {
            float mid = (wheel_c[0][2] + wheel_c[1][2] + wheel_c[2][2] + wheel_c[3][2]) * .25f;
            for (int pj = 0; pj < part_count; pj++)
                for (int i = 0; i < parts[pj].nv; i++) parts[pj].pos[i * 3 + 2] -= mid;
            for (int c = 0; c < 4; c++) wheel_c[c][2] -= mid;
            wheels_known = 1;
            printf("%s: wheels measured: wheelbase %.3f m, track %.3f/%.3f m, radius %.3f/%.3f m; body centred on the axles\n",
                   argv[1], (wheel_c[2][2] + wheel_c[3][2] - wheel_c[0][2] - wheel_c[1][2]) * .5f,
                   wheel_c[1][0] - wheel_c[0][0], wheel_c[3][0] - wheel_c[2][0],
                   (wheel_r[0] + wheel_r[1]) * .5f, (wheel_r[2] + wheel_r[3]) * .5f);
        }
    }
    int nv = 0, ni = 0;
    int ni_out = 0; /* triangles written: a double-sided part twice */
    for (int pj = 0; pj < part_count; pj++)
        if (!parts[pj].drop) {
            nv += parts[pj].nv; ni += parts[pj].ni;
            ni_out += parts[pj].ni * (parts[pj].double_sided ? 2 : 1) + (parts[pj].paint ? parts[pj].ni : 0);
        }

    /* one image per PART (its own material's baseColorTexture), not one PNG
       for the whole car: a multi-material source (body paint, glass, tyres,
       interior...) flattened onto a single image is why a real donated model
       came out a flat colour. Each distinct glTF image is decoded once and
       shared by every part that uses it. */
    /* a slot per image AND alpha handling: one image drawn OPAQUE by one material and
       BLEND by another is two textures */
    int img_total = images->n * 4; /* + a tinted copy for glass */
    int *slot_of_image = malloc((size_t)(img_total ? img_total : 1) * sizeof *slot_of_image);
    for (int i = 0; i < img_total; i++) slot_of_image[i] = -2;   /* -2 untried, -1 failed, >=0 texture slot */
    tex1_image *tex_list = NULL; int tex_n = 0, tex_cap = 0;
    tex1_image *hires = NULL; /* each slot at the image's own size (car/hires/<model>) */
    int *part_slot = malloc((size_t)(part_count ? part_count : 1) * sizeof *part_slot);
    int loaded_w = 0, loaded_h = 0, untextured = 0;
    for (int pj = 0; pj < part_count; pj++) {
        int ii = parts[pj].image;
        if (parts[pj].drop) { part_slot[pj] = -1; continue; }
        if (ii < 0 || ii >= images->n) { part_slot[pj] = -1; untextured++; continue; }
        int key = ii * 4 + (parts[pj].glass ? 3 : parts[pj].alpha_mode);
        if (slot_of_image[key] == -2) {
            int iw = 0, ih = 0, ic = 0;
            unsigned char *px = load_image_rgba(images, bufviews, bin, ii, &iw, &ih, &ic);
            if (!px) { slot_of_image[key] = -1; }
            else {
                if (tex_n == tex_cap) { tex_cap = tex_cap ? tex_cap * 2 : 4; tex_list = realloc(tex_list, (size_t)tex_cap * sizeof *tex_list); }
                tex1_image *im = &tex_list[tex_n];
                memset(im, 0, sizeof *im);
                im->width = iw; im->height = ih; im->tex_w = 1; while (im->tex_w < iw) im->tex_w <<= 1; im->tex_h = 1; while (im->tex_h < ih) im->tex_h <<= 1;
                im->rgba = malloc((size_t)iw*ih*4); memcpy(im->rgba, px, (size_t)iw*ih*4);
                for (int p = 0; p < iw*ih; p++) { /* the material's alpha rule */
                    unsigned char *a = &im->rgba[p*4+3];
                    if (parts[pj].glass) GLASS_TEXEL(a - 3);
                    else if (parts[pj].alpha_mode == 0) *a = 255;
                    else if (parts[pj].alpha_mode == 1) *a = *a >= parts[pj].cutoff * 255.0f ? 255 : 0;
                }
                im->native_alpha = malloc((size_t)iw*ih);
                for (int p = 0; p < iw*ih; p++) im->native_alpha[p] = GS_ALPHA(im->rgba[p*4+3]);
                im->color_component = 1; im->texture_function = 0; im->magnify = 1; im->minify = 1;
                /* a power-of-two image repeats (UVs that tile stay right); any other clamps */
                im->wrap_u = im->wrap_v = (iw == im->tex_w && ih == im->tex_h) ? 0 : 2;
                {   /* a body texture that is nearly black is the usual sign of an exporter that
                       put a detail/mask map where the colour belongs */
                    double lum = 0;
                    for (int p = 0; p < iw*ih; p += 7) lum += (0.3 * im->rgba[p*4] + 0.59 * im->rgba[p*4+1] + 0.11 * im->rgba[p*4+2]);
                    lum /= (iw * ih + 6) / 7;
                    const char *nm = parts[pj].name;
                    if (lum < 20 && (has_word(nm, "body") || has_word(nm, "chassis") ||
                                     has_word(nm, "paint") || has_word(nm, "hood") || has_word(nm, "door")))
                        printf("%s: WARNING: the colour image of \"%s\" is nearly black (mean %.0f/255) -- check that the material's base colour is the paint, not a mask\n",
                               argv[1], parts[pj].name, lum);
                }
                free(px);
                if (getenv("MOONMOD_DUMP_TEX")) {
                    char dp[256]; snprintf(dp, sizeof dp, "%s/img%d.ppm", getenv("MOONMOD_DUMP_TEX"), ii);
                    FILE *df = fopen(dp, "wb");
                    if (df) {
                        fprintf(df, "P6\n%d %d\n255\n", iw, ih);
                        for (int p = 0; p < iw*ih; p++) fwrite(im->rgba + p*4, 1, 3, df);
                        fclose(df);
                    }
                }
                {   /* the PC copy: the same image at its own size, the same alpha rule */
                    hires = realloc(hires, (size_t)tex_cap * sizeof *hires);
                    memset(&hires[tex_n], 0, sizeof *hires);
                    int fw = 0, fh = 0, fc = 0;
                    image_cap = 4096;
                    unsigned char *full = load_image_rgba(images, bufviews, bin, ii, &fw, &fh, &fc);
                    image_cap = 1024;
                    if (full) {
                        for (int p = 0; p < fw * fh; p++) {
                            unsigned char *a = &full[p*4+3];
                            if (parts[pj].glass) GLASS_TEXEL(a - 3);
                            else if (parts[pj].alpha_mode == 0) *a = 255;
                            else if (parts[pj].alpha_mode == 1) *a = *a >= parts[pj].cutoff * 255.0f ? 255 : 0;
                        }
                        hires[tex_n].rgba = full; hires[tex_n].width = fw; hires[tex_n].height = fh;
                    }
                }
                slot_of_image[key] = tex_n++;
                loaded_w = iw; loaded_h = ih; (void)ic;
            }
        }
        part_slot[pj] = slot_of_image[key];
        if (part_slot[pj] < 0) untextured++;
    }
    free(slot_of_image);

    /* The whole car's textures share ONE 4 MB VRAM budget at runtime (also
       the framebuffer's; tex1.c VRAM_SIZE) -- not 4 MB each. A modern source
       with many large per-part textures (one per material) blows straight
       through it; the symptom measured here is not a missing texture but a
       wrong-looking one (the GS wrapping into whatever was there before).
       Shrink every texture by the same factor so the total fits, same box
       filter as the per-image cap -- this is also, not by accident, the
       disc's own look: small textures are why GT4 cars read as GT4 cars. */
    if (tex_n) {
        size_t total = 0;
        for (int i = 0; i < tex_n; i++) total += (size_t)tex_list[i].width * tex_list[i].height * 4;
        const size_t budget = 4u * 1024 * 1024;
        if (total > budget)
            printf("%s: %d texture(s) need %.1f MB, over the GS's 4 MB budget -- halving the largest until they fit\n",
                   argv[1], tex_n, total / (1024.0*1024.0));
        while (total > budget) { /* halve the largest: a power of two stays one (and repeats) */
            int big = 0;
            for (int i = 1; i < tex_n; i++)
                if (tex_list[i].width * tex_list[i].height > tex_list[big].width * tex_list[big].height) big = i;
            if (tex_list[big].width < 2 && tex_list[big].height < 2) break;
            {
                tex1_image *im = &tex_list[big];
                total -= (size_t)im->width * im->height * 4;
                int tw = im->width > 1 ? im->width / 2 : 1, th = im->height > 1 ? im->height / 2 : 1;
                total += (size_t)tw * th * 4;
                im->rgba = box_resize(im->rgba, im->width, im->height, tw, th);
                im->width = tw; im->height = th;
                im->tex_w = 1; while (im->tex_w < tw) im->tex_w <<= 1;
                im->tex_h = 1; while (im->tex_h < th) im->tex_h <<= 1;
                free(im->native_alpha);
                im->native_alpha = malloc((size_t)tw * th);
                for (int p = 0; p < tw * th; p++) im->native_alpha[p] = GS_ALPHA(im->rgba[p*4+3]);
            }
        }
    }

    gt4_model *m = calloc(1, sizeof *m);
    m->count = m->capacity = ni_out / 3;
    m->triangles = malloc((size_t)m->count * sizeof *m->triangles);
    gt4_draw_state st = {0,0x44,0,1,1,1,6,32,0,1,1,3,0};
    int wheel_parts = 0, out_t = 0;
    /* the opaque parts first, the see-through ones (BLEND: glass) last: the engine draws a
       car's shapes in file order, and a pane drawn before the cabin hid it by depth */
    /* ... and between them the paint's reflection layer (kind 2 over the environment, as
       the disc's cars wear it: bmw_0014 carries 1431 such triangles, vertex colour 0.5) */
    for (int pk = 0; pk < part_count * 3; pk++) {
        int pj = pk % part_count, pass = pk / part_count;
        part *pt = &parts[pj];
        if (pt->drop || (pt->alpha_mode == 2) != (pass == 2) || (pass == 1 && !pt->paint)) continue;
        if (pt->attachment && pass != 1) wheel_parts++;
        for (int tf = 0; tf < pt->ni / 3 * (pt->double_sided && pass != 1 ? 2 : 1); tf++, out_t++) {
            /* the engine winds a front face as glTF does (measured: the reversed order
               showed the far side's inner face); a double-sided material's second copy is
               reversed, its normal flipped */
            int t = tf % (pt->ni / 3), back = tf >= pt->ni / 3;
            static const int order[2][3] = {{0, 1, 2}, {0, 2, 1}};
            gt4_triangle *tr = &m->triangles[out_t];
            /* the triangle moved into [0,1) WHOLE: shifting each vertex on its own tore
               every triangle that crosses a tile edge across the whole image */
            float su = 0, sv = 0;
            if (pt->uv) {
                su = sv = 1e30f;
                for (int k = 0; k < 3; k++) {
                    int vi = pt->idx[t*3+k];
                    if (pt->uv[vi*2] < su) su = pt->uv[vi*2];
                    if (pt->uv[vi*2+1] < sv) sv = pt->uv[vi*2+1];
                }
                su = floorf(su); sv = floorf(sv);
            }
            for (int k = 0; k < 3; k++) {
                int vi = pt->idx[t*3+order[back][k]];
                gt4_vertex *v = &tr->v[k];
                v->p[0]=pt->pos[vi*3+0]; v->p[1]=pt->pos[vi*3+1]; v->p[2]=pt->pos[vi*3+2];
                if (pt->nor) {
                    float nx = pt->nor[vi*3+0], ny = pt->nor[vi*3+1], nz = pt->nor[vi*3+2];
                    float l = sqrtf(nx*nx + ny*ny + nz*nz);   /* a scaled node scales its normals */
                    if (l > 1e-8f) { nx /= l; ny /= l; nz /= l; }
                    if (back) { nx = -nx; ny = -ny; nz = -nz; }
                    v->normal[0]=nx; v->normal[1]=ny; v->normal[2]=nz;
                }
                if (pt->uv)  { v->uv[0]=pt->uv[vi*2+0]-su; v->uv[1]=pt->uv[vi*2+1]-sv; }
                /* the material's colour factor (glTF baseColorFactor) */
                v->rgb[0]=pt->factor[0]; v->rgb[1]=pt->factor[1]; v->rgb[2]=pt->factor[2];
                v->ambient[0]=v->ambient[1]=v->ambient[2]=1.0f;
                v->specular[0]=v->specular[1]=v->specular[2]=0.0f;
                v->shine=16.0f; v->alpha=pt->alpha_mode == 2 ? pt->factor[3] : 1.0f;
                /* GT4's cars wear dark, nearly opaque glass (the cabin only glimpsed): a
                   modern source's clear glass at its own alpha showed the whole cabin */
                /* glass: its texel carries the tint (GLASS_TEXEL), the vertex leaves it whole --
                   the GS multiplies the two, and the source's 0.28 left the pane clear */
                if (pt->glass) v->alpha = 1.0f;
            }
            tr->texture = part_slot[pj]; tr->kind = 1; tr->attachment = pt->attachment; tr->state = st;
            tr->blend_enabled = pt->alpha_mode == 2; /* see-through: the packet turns blending on */
            if (pass == 1) { /* the reflection: the environment slot, added over the paint */
                tr->texture = -2;
                tr->kind = 2;
                tr->blend_enabled = 1;
                for (int k = 0; k < 3; k++) {
                    tr->v[k].rgb[0] = tr->v[k].rgb[1] = tr->v[k].rgb[2] = .5f;
                    tr->v[k].alpha = 1.0f;
                }
            }
        }
    }
    free(part_slot);
    if (rig_path && !wheel_parts)
        fprintf(stderr, "--rig %s: named no mesh the model actually has -- exported with no wheels tagged\n", rig_path);
    m->textures = tex_list;
    m->ntextures = tex_n;
    printf("%s: %d vertices, %d triangles, %d texture(s) (last %dx%d)%s; length %.2f m (scale %.4f)\n",
           argv[1], nv, m->count, tex_n, loaded_w, loaded_h,
           untextured ? " [some parts untextured]" : "", target_len, s);

    size_t osz = 0; uint8_t *car = gt4_model_encode_car4(m, &osz);
    if (!car) { fprintf(stderr, "writer refused the model\n"); return 1; }
    if (wheels_disc) {
        const char *err = NULL;
        gt4_vol *v = gt4_vol_open(wheels_disc, &err);
        if (!v) { fprintf(stderr, "--wheels-from %s: %s\n", wheels_disc, err ? err : "cannot open"); free(car); return 1; }
        size_t donor_sz = 0;
        uint8_t *donor = donor_car4(v, wheels_label, &donor_sz);
        gt4_vol_close(v);
        if (!donor) { fprintf(stderr, "--wheels-from: cannot resolve donor \"%s\"\n", wheels_label); free(car); return 1; }
        size_t car2_sz = 0;
        uint8_t *car2 = gt4_model_car4_attach_wheels(car, osz, donor, donor_sz, &car2_sz);
        free(donor);
        if (!car2) { fprintf(stderr, "--wheels-from: donor \"%s\" has no rim/tyre of its own\n", wheels_label); free(car); return 1; }
        free(car); car = car2; osz = car2_sz;
        printf("%s: wheels borrowed from \"%s\" (rim/tyre, %zu bytes)\n", argv[1], wheels_label, osz);
    }
    if (rim_path || tire_path) {
        gt4_model *rim = rim_path ? load_glb_as_model(rim_path) : NULL;
        gt4_model *tire = tire_path ? load_glb_as_model(tire_path) : NULL;
        if ((rim_path && !rim) || (tire_path && !tire)) { gt4_model_free(rim); gt4_model_free(tire); free(car); return 1; }
        size_t car2_sz = 0;
        uint8_t *car2 = gt4_model_car4_attach_wheel_parts(car, osz, rim, tire, &car2_sz);
        gt4_model_free(rim); gt4_model_free(tire);
        if (!car2) { fprintf(stderr, "--rim/--tire: writer refused\n"); free(car); return 1; }
        free(car); car = car2; osz = car2_sz;
        printf("%s: wheels authored (rim=%s, tire=%s, %zu bytes)\n", argv[1],
               rim_path ? "yes" : "no", tire_path ? "yes" : "no", osz);
    }
    mkdir_p(out);
    FILE *o = fopen(out, "wb");
    if (!o || fwrite(car, 1, osz, o) != osz) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fclose(o);
    printf("wrote %s (%zu bytes)\n", out, osz);
    if (wheels_known) {
        /* .../car/<menu|lod|open>/<model> -> .../car/wheels/<model> (the engine reads it
           through the mod's folder; the disc has none) */
        char side[1200];
        snprintf(side, sizeof side, "%s", out);
        char *name = strrchr(side, '/'), *bs = strrchr(side, '\\');
        if (bs > name) name = bs;
        char *slot = NULL;
        if (name) {
            *name = 0;
            slot = strrchr(side, '/');
            bs = strrchr(side, '\\');
            if (bs > slot) slot = bs;
        }
        if (slot && strstr(side, "car")) {
            char model[256];
            snprintf(model, sizeof model, "%s", name + 1);
            snprintf(slot + 1, sizeof side - (size_t)(slot + 1 - side), "wheels/%s", model);
            mkdir_p(side);
            FILE *w = fopen(side, "wb");
            if (w) {
                fprintf(w, "# MOON ENGINE car wheels -- written by moonmod_glbcar, read by the engine to\n"
                           "# DRAW the wheels where the model has them. The physics keeps the donor's tables.\n");
                fprintf(w, "wheelbase %.4f\n", (wheel_c[2][2] + wheel_c[3][2] - wheel_c[0][2] - wheel_c[1][2]) * .5f);
                fprintf(w, "tread %.4f %.4f\n", wheel_c[1][0] - wheel_c[0][0], wheel_c[3][0] - wheel_c[2][0]);
                fprintf(w, "radius %.4f %.4f\n", (wheel_r[0] + wheel_r[1]) * .5f, (wheel_r[2] + wheel_r[3]) * .5f);
                fprintf(w, "width %.4f %.4f\n", (wheel_w[0] + wheel_w[1]) * .5f, (wheel_w[2] + wheel_w[3]) * .5f);
                fclose(w);
                printf("wrote %s\n", side);
            }
        }
    }
    {   /* car/hires/<model>: every texture the GS had to shrink, at its own size (both
           powers of two, so the UVs mean the same at either size) */
        tex1_image *keep = calloc((size_t)(tex_n ? tex_n : 1), sizeof *keep);
        int *index = calloc((size_t)(tex_n ? tex_n : 1), sizeof *index), nkeep = 0;
        size_t bytes = 0;
        for (int i = 0; i < tex_n && keep && index; i++) {
            const tex1_image *small = &tex_list[i], *big = hires ? &hires[i] : NULL;
            int p2 = big && big->rgba && !(big->width & (big->width - 1)) && !(big->height & (big->height - 1));
            if (!p2 || small->width != small->tex_w || small->height != small->tex_h ||
                (big->width <= small->width && big->height <= small->height))
                continue;
            keep[nkeep] = *big;
            index[nkeep++] = i;
            bytes += (size_t)big->width * big->height * 4;
        }
        char side[1200];
        snprintf(side, sizeof side, "%s", out);
        char *name = strrchr(side, '/'), *bs = strrchr(side, '\\');
        if (bs > name) name = bs;
        char *slot = NULL;
        if (name) {
            *name = 0;
            slot = strrchr(side, '/');
            bs = strrchr(side, '\\');
            if (bs > slot) slot = bs;
        }
        if (nkeep && slot && strstr(side, "car")) {
            char model[256];
            snprintf(model, sizeof model, "%s", name + 1);
            snprintf(slot + 1, sizeof side - (size_t)(slot + 1 - side), "hires/%s", model);
            size_t hsz = 0;
            uint8_t *h = gt4_hires_encode(keep, index, nkeep, &hsz);
            mkdir_p(side);
            FILE *w = h ? fopen(side, "wb") : NULL;
            if (w && fwrite(h, 1, hsz, w) == hsz)
                printf("wrote %s: %d texture(s) at their own size (%.1f MB unpacked, %zu bytes)\n",
                       side, nkeep, bytes / 1048576.0, hsz);
            if (w) fclose(w);
            free(h);
        }
        free(keep);
        free(index);
    }
    return 0;
}
