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

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <in.png> <out.img> [W H]\n", argv[0]); return 2; }
    int tw = argc > 4 ? atoi(argv[3]) : 0, th = argc > 4 ? atoi(argv[4]) : 0;

    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *raw = malloc(n);
    if (!raw || fread(raw, 1, n, f) != (size_t)n) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    fclose(f);

    int iw = 0, ih = 0, ic = 0;
    unsigned char *px = stbi_load_from_memory(raw, (int)n, &iw, &ih, &ic, 4);
    free(raw);
    if (!px) { fprintf(stderr, "%s: not a supported image\n", argv[1]); return 1; }
    if (tw <= 0) tw = iw; if (th <= 0) th = ih;
    if (tw > 1024) tw = 1024; if (th > 1024) th = 1024;
    unsigned char *rgba = (tw == iw && th == ih) ? NULL : fit(px, iw, ih, tw, th);
    const unsigned char *use = rgba ? rgba : px;

    tex1_image im; memset(&im, 0, sizeof im);
    im.width = tw; im.height = th;
    im.tex_w = (int)next_pow2((unsigned)tw); im.tex_h = (int)next_pow2((unsigned)th);
    im.rgba = malloc((size_t)tw * th * 4);
    im.native_alpha = malloc((size_t)tw * th);
    if (!im.rgba || !im.native_alpha) { fprintf(stderr, "out of memory\n"); return 1; }
    memcpy(im.rgba, use, (size_t)tw * th * 4);
    for (int i = 0; i < tw * th; i++) im.native_alpha[i] = use[i * 4 + 3];
    im.color_component = 1; im.texture_function = 0; im.magnify = 1; im.minify = 1;
    im.wrap_u = im.wrap_v = 2;

    size_t size = 0;
    uint8_t *img = gt4_tex1_encode(&im, 1, &size);
    free(rgba); free(im.rgba); free(im.native_alpha); free(px);
    if (!img) { fprintf(stderr, "Tex1 writer refused the image\n"); return 1; }
    FILE *o = fopen(argv[2], "wb");
    if (!o || fwrite(img, 1, size, o) != size) { fprintf(stderr, "cannot write %s\n", argv[2]); return 1; }
    fclose(o);
    printf("%s: %s %dx%d -> %s (%zu bytes, Tex1)\n", argv[0], argv[1], tw, th, argv[2], size);
    free(img);
    return 0;
}
