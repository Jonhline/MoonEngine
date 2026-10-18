/*
 * moonmod_img: turn a PNG into a GT4 "Tex1" .img, the per-car pictures the
 * menus load from /piece/car_mini/<label>.img and /piece/car_pict/<label>.img
 * (gtmode.adc CarScroll/SelectBox builds the path from MCarData.GetCarLabel;
 * ui.c image() maps "image/piece/....png" onto "/piece/....img").
 *
 *   moonmod_img <in.png> <out.img> [W H]
 *
 * Fits the picture into a WxH box (default: the source size). The engine's own
 * Tex1 writer builds the file, so it round-trips through the engine's reader.
 */
#include "model_write.h"
#include "tex1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO
#include "../../third_party/stb/stb_image.h"

static unsigned next_pow2(unsigned v) { unsigned p = 1; while (p < v) p <<= 1; return p; }

static uint8_t *fit(const unsigned char *src, int sw, int sh, int w, int h)
{
    uint8_t *out = malloc((size_t)w * h * 4);
    if (!out) return NULL;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        unsigned sy0 = (unsigned)y * sh / h, sy1 = (unsigned)(y + 1) * sh / h;
        unsigned sx0 = (unsigned)x * sw / w, sx1 = (unsigned)(x + 1) * sw / w;
        unsigned s[4] = {0, 0, 0, 0}, n = 0;
        for (unsigned sy = sy0; sy < sy1; sy++) for (unsigned sx = sx0; sx < sx1; sx++, n++)
            for (int c = 0; c < 4; c++) s[c] += src[((size_t)sy * sw + sx) * 4 + c];
        for (int c = 0; c < 4; c++) out[((size_t)y * w + x) * 4 + c] = (uint8_t)(n ? s[c] / n : 0);
    }
    return out;
}

static unsigned char *load_png(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *raw = n > 0 ? malloc(n) : NULL;
    if (!raw || fread(raw, 1, n, f) != (size_t)n) { fprintf(stderr, "cannot read %s\n", path); fclose(f); free(raw); return NULL; }
    fclose(f);
    int ic = 0;
    unsigned char *px = stbi_load_from_memory(raw, (int)n, w, h, &ic, 4);
    free(raw);
    if (!px) fprintf(stderr, "%s: not a supported image\n", path);
    return px;
}

/* --matte BLACK WHITE: the same frame rendered over black and over white (MoonEngine
   --preview with MOON_PICTURE). Where the two agree the car is opaque; where they differ by
   the full 255 it is the background: alpha = 255 - (white - black), colour = black / alpha.
   The result is cropped to the car with a small margin, as the disc's car_pict frames it. */
static unsigned char *matte(const char *black_path, const char *white_path, int *w, int *h)
{
    int bw = 0, bh = 0, ww = 0, wh = 0;
    unsigned char *b = load_png(black_path, &bw, &bh), *wt = load_png(white_path, &ww, &wh);
    if (!b || !wt || bw != ww || bh != wh) {
        if (b && wt) fprintf(stderr, "--matte: the two frames differ in size\n");
        stbi_image_free(b); stbi_image_free(wt);
        return NULL;
    }
    int x0 = bw, y0 = bh, x1 = -1, y1 = -1;
    for (int y = 0; y < bh; y++) for (int x = 0; x < bw; x++) {
        unsigned char *p = b + ((size_t)y * bw + x) * 4, *q = wt + ((size_t)y * bw + x) * 4;
        int d = ((q[0] - p[0]) + (q[1] - p[1]) + (q[2] - p[2])) / 3, a = 255 - d;
        if (a < 0) a = 0; if (a > 255) a = 255;
        for (int c = 0; c < 3; c++) {
            int v = a ? p[c] * 255 / a : 0;
            p[c] = (unsigned char)(v > 255 ? 255 : v);
        }
        p[3] = (unsigned char)a;
        if (a > 8) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    }
    stbi_image_free(wt);
    if (x1 < 0) { fprintf(stderr, "--matte: no car in the frames\n"); stbi_image_free(b); return NULL; }
    int m = (x1 - x0 + 1) * 3 / 20 + 2; /* the disc's car_pict leaves ~12% each side (160x100, car ~124 wide) */
    x0 = x0 > m ? x0 - m : 0; y0 = y0 > m ? y0 - m : 0;
    x1 = x1 + m < bw ? x1 + m : bw - 1; y1 = y1 + m < bh ? y1 + m : bh - 1;
    *w = x1 - x0 + 1; *h = y1 - y0 + 1;
    unsigned char *out = malloc((size_t)*w * *h * 4);
    for (int y = 0; out && y < *h; y++)
        memcpy(out + (size_t)y * *w * 4, b + ((size_t)(y + y0) * bw + x0) * 4, (size_t)*w * 4);
    stbi_image_free(b);
    return out;
}

int main(int argc, char **argv)
{
    int matting = argc > 1 && !strcmp(argv[1], "--matte");
    if (argc < (matting ? 5 : 3)) {
        fprintf(stderr, "usage: %s <in.png> <out.img> [W H]\n"
                        "       %s --matte <black.png> <white.png> <out.img> [W H]\n", argv[0], argv[0]);
        return 2;
    }
    const char *in = matting ? argv[3] : argv[1], *out_path = matting ? argv[4] : argv[2];
    int tw = argc > (matting ? 6 : 4) ? atoi(argv[matting ? 5 : 3]) : 0,
        th = argc > (matting ? 6 : 4) ? atoi(argv[matting ? 6 : 4]) : 0;
    int iw = 0, ih = 0;
    unsigned char *px = matting ? matte(argv[2], argv[3], &iw, &ih) : load_png(in, &iw, &ih);
    if (!px) return 1;
    if (tw <= 0) tw = iw; if (th <= 0) th = ih;
    if (tw > 1024) tw = 1024; if (th > 1024) th = 1024;
    /* into the box at the picture's own proportions, the rest transparent (a stretched car
       reads as the wrong car) */
    unsigned char *rgba = NULL;
    if (tw != iw || th != ih) {
        int fw = tw, fh = (int)((long long)ih * tw / iw);
        if (fh > th) { fh = th; fw = (int)((long long)iw * th / ih); }
        if (fw < 1) fw = 1; if (fh < 1) fh = 1;
        unsigned char *scaled = fit(px, iw, ih, fw, fh);
        rgba = scaled ? calloc((size_t)tw * th, 4) : NULL;
        if (rgba)
            for (int y = 0; y < fh; y++)
                memcpy(rgba + ((size_t)(y + (th - fh) / 2) * tw + (tw - fw) / 2) * 4,
                       scaled + (size_t)y * fw * 4, (size_t)fw * 4);
        free(scaled);
        if (!rgba) { fprintf(stderr, "out of memory\n"); return 1; }
    }
    const unsigned char *use = rgba ? rgba : px;

    tex1_image im; memset(&im, 0, sizeof im);
    im.width = tw; im.height = th;
    im.tex_w = (int)next_pow2((unsigned)tw); im.tex_h = (int)next_pow2((unsigned)th);
    im.rgba = malloc((size_t)tw * th * 4);
    im.native_alpha = malloc((size_t)tw * th);
    if (!im.rgba || !im.native_alpha) { fprintf(stderr, "out of memory\n"); return 1; }
    memcpy(im.rgba, use, (size_t)tw * th * 4);
    /* the GS's alpha runs 0..0x80 (0x80 = 1.0); the reader doubles it back for the menus */
    for (int i = 0; i < tw * th; i++) im.native_alpha[i] = (uint8_t)((use[i * 4 + 3] * 128u + 127u) / 255u);
    im.color_component = 1; im.texture_function = 0; im.magnify = 1; im.minify = 1;
    im.wrap_u = im.wrap_v = 2;

    size_t size = 0;
    uint8_t *img = gt4_tex1_encode(&im, 1, &size);
    free(rgba); free(im.rgba); free(im.native_alpha); free(px);
    if (!img) { fprintf(stderr, "Tex1 writer refused the image\n"); return 1; }
    FILE *o = fopen(out_path, "wb");
    if (!o || fwrite(img, 1, size, o) != size) { fprintf(stderr, "cannot write %s\n", out_path); return 1; }
    fclose(o);
    printf("%s: %s %dx%d -> %s (%zu bytes, Tex1)\n", argv[0], in, tw, th, out_path, size);
    free(img);
    return 0;
}
