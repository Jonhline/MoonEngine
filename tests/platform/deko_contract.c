#include "../../src/platform/switch/deko_contract.h"
#include "../../src/platform/switch/deko_shader_blob.h"
#include "../../src/platform/switch/deko_draw_range.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "deko contract line %d: %s\n", __LINE__, #c);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    const uint32_t course_vertices = 528798u * 3u;
    uint32_t course_bytes = course_vertices * 108u;
    if (argc > 1 && !strcmp(argv[1], "--draw-negative"))
        course_bytes -= 108;
    CHECK(nd_draw_range_valid(course_bytes, 108, course_vertices - 384, 384));
    CHECK(nd_draw_range_valid(6 * 32, 32, 0, 6));
    CHECK(!nd_draw_range_valid(6 * 32 - 1, 32, 0, 6));
    CHECK(!nd_draw_range_valid(UINT32_MAX, 108, UINT32_MAX - 1, 3));
    CHECK(!nd_draw_range_valid(108, 108, 0, UINT32_MAX));
    CHECK(!nd_draw_range_valid(108, 0, 0, 1));
    CHECK(!nd_draw_range_valid(108, 108, 0, 0));
    unsigned probe_w = ND_UI_PROBE_WIDTH, probe_h = ND_UI_PROBE_HEIGHT;
    if (argc > 1 && !strcmp(argv[1], "--probe-negative"))
        probe_w = probe_h = 16; /* Restore the former letterboxed square target. */
    CHECK(probe_w * 9 == probe_h * 16);
    for (unsigned i = 0; i < 4; i++) {
        unsigned x = nd_ui_probe_xy[i][0], y = nd_ui_probe_xy[i][1];
        CHECK(x < probe_w && y < probe_h);
        /* Pixel centers in the outer quarters clamp to the 2x2 corner texels.
           This guards both banding and interpolation false negatives. */
        CHECK((i & 1) ? (x * 4 + 2 >= probe_w * 3) : (x * 4 + 2 <= probe_w));
        CHECK((i & 2) ? (y * 4 + 2 >= probe_h * 3) : (y * 4 + 2 <= probe_h));
    }
    /* Read-only embedded DKSH input. The SDK patches two program-header
       offsets at initialization, so the control supplied to it must be a copy. */
    static const unsigned char shader[512] = {'D', 'K', 'S', 'H', 24, 0, 0, 0, 0, 1, 0, 0,
                                              0,   1,   0,   0,   24, 0, 0, 0, 1, 0, 0, 0,
                                              0,   0,   0,   0,   48, 0, 0, 0, 8, 0, 0, 0};
    nd_shader_blob blob;
    CHECK(nd_shader_blob_copy(&blob, shader, sizeof shader));
    void *allocated_control = blob.control;
    if (argc > 1 && !strcmp(argv[1], "--shader-negative"))
        blob.control = (void *)shader;
    CHECK(blob.control != shader);
    CHECK(blob.code == shader + 256 && blob.code_size == 256);
    CHECK(!memcmp(blob.control, shader, 256));
    unsigned char *control = (unsigned char *)blob.control;
    control[28] = 80;
    control[32] = 40;
    CHECK(shader[28] == 48 && shader[32] == 8);
    free(allocated_control);
    CHECK(nd_shader_blob_copy(&blob, shader, sizeof shader));
    CHECK(((unsigned char *)blob.control)[28] == 48);
    free(blob.control);
    CHECK(!nd_shader_blob_copy(&blob, shader, sizeof shader - 1));
    CHECK(!blob.control && !blob.code && !blob.code_size);
    CHECK(!nd_shader_blob_copy(&blob, NULL, sizeof shader));
    unsigned char invalid[512];
    memcpy(invalid, shader, sizeof invalid);
    invalid[16] = 255;
    CHECK(!nd_shader_blob_copy(&blob, invalid, sizeof invalid));
    CHECK(sizeof(nd_uniform)==2304 && offsetof(nd_uniform,bones)==1024);
    uint8_t expected[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255},
            pixels[16];
    memcpy(pixels, expected, sizeof pixels);
    if (argc > 1 && !strcmp(argv[1], "--negative"))
        pixels[6] = 127;
    CHECK(nd_rgba_equal(expected,pixels,sizeof pixels,1));
    for (unsigned byte = 0; byte < sizeof pixels; byte++) {
        memcpy(pixels, expected, sizeof pixels);
        pixels[byte] ^= 128;
        CHECK(!nd_rgba_equal(expected,pixels,sizeof pixels,1));
    }
    memcpy(pixels, expected, sizeof pixels);
    pixels[0]--;
    CHECK(nd_rgba_equal(expected,pixels,sizeof pixels,1));
    CHECK(!nd_rgba_equal(expected,pixels,sizeof pixels,0));
    CHECK(!nd_rgba_equal(NULL,pixels,sizeof pixels,0));
    puts("deko shader ownership, uniform ABI, 16:9 probe and boot comparator: passed");
    return 0;
}
