/* Authored functional meshes use the existing retail decoder and lamp tags. */
#include "model.h"
#include "model_write.h"
#include "race_sound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "functional car line %d: %s\n", __LINE__, #x);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int plat_audio_play(const int16_t *pcm, size_t frames, int rate, int channels)
{
    (void)pcm;
    (void)frames;
    (void)rate;
    (void)channels;
    return 0;
}
static int geometry(int negative)
{
    gt4_triangle tris[9] = {0};
    gt4_model model = {0};
    model.triangles = tris;
    model.count = model.capacity = 9;
    for (int i = 0; i < 9; i++) {
        tris[i].texture = -1;
        tris[i].kind = 1;
        tris[i].lamp = i % 3;
        tris[i].attachment = i < 5 ? i : 0;
        tris[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &tris[i].v[j];
            v->p[0] = i * 2.0f + (j == 1);
            v->p[1] = (j == 2);
            v->normal[2] = 1;
            v->alpha = 1;
            for (int k = 0; k < 3; k++)
                v->rgb[k] = v->ambient[k] = 1;
        }
    }
    size_t size = 0;
    uint8_t *bytes = gt4_model_encode_car4(&model, &size);
    CHECK(bytes);
    gt4_model *decoded = gt4_model_decode(bytes, size, 0);
    CHECK(decoded && decoded->count == 9);
    if (negative)
        decoded->triangles[1].lamp = 0;
    for (int i = 0; i < 9; i++) {
        CHECK(decoded->triangles[i].lamp == tris[i].lamp);
        CHECK(decoded->triangles[i].attachment == tris[i].attachment);
        CHECK(decoded->triangles[i].v[0].p[0] == tris[i].v[0].p[0]);
    }
    puts("PASS body visibility, both brake branches, four wheel frames and scope isolation");
    gt4_model_free(decoded);
    free(bytes);
    tris[0].lamp = 3;
    CHECK(!gt4_model_encode_car4(&model, &size));
    return 0;
}
static int sound(const char *path)
{
    FILE *f = fopen(path, "rb");
    CHECK(f);
    CHECK(!fseek(f,0,SEEK_END));
    long size = ftell(f);
    rewind(f);
    CHECK(size > 0 && size < 64 * 1024 * 1024);
    uint8_t *bytes = malloc((size_t)size);
    CHECK(bytes);
    CHECK(fread(bytes,1,(size_t)size,f) == (size_t)size);
    fclose(f);
    gt4_engine_bank *bank = gt4_engine_bank_decode(bytes, (size_t)size, 0);
    CHECK(bank && bank->count == 1);
    CHECK(bank->layers[0].rpm == 3000 && bank->layers[0].pitch == 4096);
    CHECK(bank->layers[0].sample.repeat && bank->layers[0].sample.loop == 0);
    CHECK(bank->layers[0].sample.frames == 2800);
    double signal = 0, error = 0;
    for (int i = 0; i < 2800; i++) {
        int expected = (int16_t)(sin(i * .07) * 16000);
        double d = bank->layers[0].sample.pcm[i] - expected;
        signal += expected * (double)expected;
        error += d * d;
    }
    double snr = 10 * log10(signal / error);
    printf("PASS authored WAV -> native SPU ADPCM: %zu samples, SNR %.2f dB\n",
           bank->layers[0].sample.frames, snr);
    CHECK(snr > 30);
    bytes[0] ^= 1;
    CHECK(!gt4_engine_bank_decode(bytes,(size_t)size,0));
    gt4_engine_bank_close(bank);
    free(bytes);
    return 0;
}
static int inspect_lamps(const char *path)
{
    FILE *f = fopen(path, "rb");
    CHECK(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    CHECK(size > 0);
    uint8_t *data = malloc((size_t)size);
    CHECK(data);
    CHECK(fread(data, 1, (size_t)size, f) == (size_t)size);
    fclose(f);
    gt4_model *model = gt4_model_decode(data, (size_t)size, 0);
    CHECK(model);
    unsigned count[3] = {0};
    for (int i = 0; i < model->count; i++) {
        CHECK(model->triangles[i].lamp < 3);
        count[model->triangles[i].lamp]++;
        if (model->triangles[i].lamp == 2)
            CHECK(model->triangles[i].kind == 0); /* Authored KHR_materials_unlit, not forced by braking. */
    }
    printf("exported lamp triangles: body=%u released=%u pressed=%u\n", count[0], count[1],
           count[2]);
    CHECK(count[0] && count[1] && count[2]);
    gt4_model_free(model);
    free(data);
    return 0;
}
int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--model"))
        return inspect_lamps(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "--sound"))
        return sound(argv[2]);
    return geometry(argc > 1 && !strcmp(argv[1], "--negative"));
}
