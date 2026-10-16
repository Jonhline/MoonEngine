/* Same-console regression diagnostic. No disc, profile or game configuration reads. */
#include "../../src/platform/switch/deko_video.h"
#include "../../src/platform/switch/deko_blend.h"
#include <switch.h>
#include <algorithm>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace {
struct Vertex {
    float value[27];
};
unsigned cases, failures;
bool compare(const uint8_t *expected, const uint8_t *actual, size_t size, unsigned tolerance)
{
    /* Prove that the comparator rejects a deliberate single-channel error. */
    uint8_t corrupt = expected[0] ^ 128;
    if (nd_rgba_equal(&corrupt, expected, 1, tolerance))
        return false;
    return nd_rgba_equal(expected, actual, size, tolerance);
}
void image_copies()
{
    /* Real captures span many block-linear GOBs; the old 32x18 test did not. */
    const unsigned sizes[][2]{{32, 18}, {256, 128}, {960, 540}, {1280, 720}};
    for (auto &size : sizes) {
        unsigned width = size[0], height = size[1];
        std::vector<uint8_t> expected(width * height * 4), actual(expected.size());
        for (unsigned y = 0; y < height; y++)
            for (unsigned x = 0; x < width; x++) {
                auto *pixel = expected.data() + (y * width + x) * 4;
                pixel[0] = x * 7 + y * 3;
                pixel[1] = y * 13 + x * 5;
                pixel[2] = (x ^ y) * 11;
                pixel[3] = (x * 17 + y * 19) & 255;
            }
        nd_image *source = nd_image_create(width, height);
        nd_image *destination = nd_image_create(width, height);
        nd_image_upload(source, expected.data());
        bool upload = nd_image_read(source, actual.data()) &&
                      compare(expected.data(), actual.data(), actual.size(), 0);
        nd_copy(source, destination, 0, 0, width, height);
        bool copy = nd_image_read(destination, actual.data()) &&
                    compare(expected.data(), actual.data(), actual.size(), 0);
        std::fprintf(stderr, "GPU compare image %ux%u upload=%s copy=%s\n", width, height,
                     upload ? "PASS" : "FAIL", copy ? "PASS" : "FAIL");
        cases += 2;
        failures += !upload + !copy;
        if (!copy) {
            size_t mismatch = 0;
            while (mismatch < actual.size() && actual[mismatch] == expected[mismatch])
                mismatch++;
            if (mismatch < actual.size())
                std::fprintf(
                    stderr,
                    "GPU compare first copy mismatch: x=%zu y=%zu channel=%zu actual=%u expected=%u\n",
                    mismatch / 4 % width, mismatch / 4 / width, mismatch % 4, actual[mismatch],
                    expected[mismatch]);
        }
        nd_image_destroy(source);
        nd_image_destroy(destination);
    }
}
void mesh_matrix()
{
    auto saved = nv_get();
    nd_image *target = nd_image_create(32, 18);
    nv_target(target, nullptr);
    nd_viewport(0, 0, 32, 18);
    nd_scissor(0, 0, 32, 18);
    nd_uniform u{};
    u.v[0][0] = u.v[1][1] = u.v[2][2] = 1;
    u.v[5][0] = u.v[6][1] = u.v[7][2] = 1;
    u.v[8][0] = u.v[8][1] = 1;
    u.v[8][2] = .2f;
    u.v[8][3] = 20000;
    u.v[9][0] = u.v[9][1] = u.v[9][2] = 1;
    u.v[10][1] = 128.f / 255;
    u.v[13][0] = 1;
    u.v[46][2] = 1;
    const float positions[3][3]{{-2, -2, 2}, {2, -2, 2}, {0, 2, 2}};
    Vertex vertices[3]{};
    for (unsigned i = 0; i < 3; i++) {
        std::memcpy(vertices[i].value, positions[i], 12);
        vertices[i].value[5] = -1;
        vertices[i].value[8] = 1;
    }
    DkRasterizerState raster;
    dkRasterizerStateDefaults(&raster);
    raster.cullMode = DkFace_None;
    DkDepthStencilState depth;
    dkDepthStencilStateDefaults(&depth);
    depth.depthTestEnable = false;
    depth.depthWriteEnable = false;
    DkColorWriteState mask;
    dkColorWriteStateDefaults(&mask);
    unsigned images[4]{nv_white(), nv_white(), nv_white(), nv_white()};
    uint8_t pixels[32 * 18 * 4];
    /* Bits 0/1 select the first vertex/fragment program independently.
       Bit 4 restores the proven-broken vertex shader as a negative.
       Bit 2 restores the SDK blend encoder; bit 3 overrides colour MME state
       with explicit equivalent methods. No override is enabled in the game. */
    for (unsigned shader : {0u, 1u, 2u, 3u, 16u, 18u})
        for (unsigned encoder : {0u, 4u, 8u})
            for (unsigned blend_enable = 0; blend_enable < 2; blend_enable++)
                for (unsigned sample = 1; sample <= 3; sample++) {
                    float alpha = sample / 4.f;
                    for (auto &vertex : vertices)
                        vertex.value[18] = alpha;
                    nd_buffer buffer{};
                    buffer.size = sizeof vertices;
                    DkGpuAddr address = nd_stream(vertices, sizeof vertices);
                    DkColorState color;
                    DkBlendState blend;
                    nd_material_blend(blend_enable != 0, 0x44, &color, &blend);
                    nd_clear(0, 0, 0, 1, true, false);
                    nd_pipeline(ND_MESH, u, buffer, 0, 3, images, raster, depth, color, mask, blend,
                                0, 0, address, shader | encoder);
                    uint8_t wanted[4]{(uint8_t)(blend_enable ? 128 * alpha : 128), 0, 0,
                                      (uint8_t)(255 * alpha + .5f)};
                    bool ok = nd_image_read(target, pixels) &&
                              compare(wanted, pixels + (9 * 32 + 16) * 4, 4, 1);
                    const uint8_t *got = pixels + (9 * 32 + 16) * 4;
                    std::fprintf(
                        stderr,
                        "GPU compare mesh vs=%s fs=%s state=%s blend=%u alpha=%.2f %s rgba=%u,%u,%u,%u expected=%u,0,0,%u\n",
                        shader & 16  ? "broken"
                        : shader & 1 ? "first"
                                     : "fixed",
                        shader & 2 ? "first" : "current",
                        encoder == 4   ? "SDK"
                        : encoder == 8 ? "direct-colour"
                                       : "current",
                        blend_enable, alpha, ok ? "PASS" : "FAIL", got[0], got[1], got[2], got[3],
                        wanted[0], wanted[3]);
                    cases++;
                    failures += !ok;
                }
    /* UI uses a separate shader and vertex layout. Check its fractional
       alpha too, so a common state failure is not mislabeled a mesh bug. */
    struct UiVertex {
        float x, y, u, v, color[4];
    };
    UiVertex ui[3]{{-1, -1, 0, 0, {128.f / 255, 0, 0, 1}},
                   {3, -1, 0, 0, {128.f / 255, 0, 0, 1}},
                   {-1, 3, 0, 0, {128.f / 255, 0, 0, 1}}};
    nd_uniform ui_uniform{};
    ui_uniform.v[0][1] = 1;
    for (unsigned encoder : {0u, 4u, 8u})
        for (unsigned enabled = 0; enabled < 2; enabled++)
            for (unsigned sample = 1; sample <= 3; sample++) {
                float alpha = sample / 4.f;
                for (auto &vertex : ui)
                    vertex.color[3] = alpha;
                nd_buffer buffer{};
                buffer.size = sizeof ui;
                DkGpuAddr address = nd_stream(ui, sizeof ui);
                DkColorState color;
                DkBlendState blend;
                nd_material_blend(enabled != 0, 0x44, &color, &blend);
                nd_clear(0, 0, 0, 1, true, false);
                nd_pipeline(ND_UI, ui_uniform, buffer, 0, 3, images, raster, depth, color, mask,
                            blend, 0, 0, address, encoder);
                uint8_t wanted[4]{(uint8_t)(enabled ? 128 * alpha : 128), 0, 0,
                                  (uint8_t)(255 * alpha + .5f)};
                bool ok = nd_image_read(target, pixels) &&
                          compare(wanted, pixels + (9 * 32 + 16) * 4, 4, 1);
                const uint8_t *got = pixels + (9 * 32 + 16) * 4;
                std::fprintf(
                    stderr,
                    "GPU compare UI state=%s blend=%u alpha=%.2f %s rgba=%u,%u,%u,%u expected=%u,0,0,%u\n",
                    encoder == 4   ? "SDK"
                    : encoder == 8 ? "direct-colour"
                                   : "current",
                    enabled, alpha, ok ? "PASS" : "FAIL", got[0], got[1], got[2], got[3], wanted[0],
                    wanted[3]);
                cases++;
                failures += !ok;
            }
    nv_set(saved);
    nd_image_destroy(target);
}
} // namespace
int main(int, char **)
{
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/moonengine", 0777);
    if (!std::freopen("sdmc:/switch/moonengine/gpu-compare.log", "w", stderr))
        return 1;
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(
        stderr,
        "Moon Engine GPU comparison v2: first=7715ae8 broken=36fe696 fixed=arithmetic-alpha; no disc or saves\n");
    std::fesetround(FE_TOWARDZERO);
    if (plat_video_init(nullptr) < 0) {
        std::fprintf(stderr, "GPU comparison initialization FAIL\n");
        return 1;
    }
    image_copies();
    mesh_matrix();
    std::fprintf(stderr,
                 "GPU comparison finished: %u cases, %u failures; diagnostic, not a graphics fix\n",
                 cases, failures);
    nd_bind(nd_screen(), nullptr);
    nd_scissor(0, 0, nd_screen()->width, nd_screen()->height);
    nd_clear(failures ? .3f : 0, failures ? 0 : .3f, 0, 1, true, false);
    nd_present();
    svcSleepThread(1000000000);
    plat_video_shutdown();
    return 0;
}
