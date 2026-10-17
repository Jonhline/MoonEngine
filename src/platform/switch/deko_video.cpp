/* Switch-native 2D presentation. PC keeps video_gl.c. */
#include "deko_video.h"
#include "deko_events.h"
#include "deko_composition.h"
#include "deko_ui_clip.h"
extern "C" {
#include "platform/switch_profile.h"
}
#include <switch.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
namespace {
using Vertex = nd_ui_vertex;
std::vector<Vertex> batch;
unsigned batch_texture;
int batch_kind;
float batch_fixed_alpha = -1;
nd_image *white, *black, *page, *scene, *scene_depth, *license_lut;
uint8_t cached_lut[256][3];
bool lut_valid;
nv_state current{}, scene_saved{};
unsigned display_width, display_height;
float view_w = 640, view_h = 480, scale_x = 1, scale_y = 1, off_x, off_y, side_margin, content_x,
      content_width;
bool menu_projection = true, menu_pacing = true, scene_active = false, initialized = false;
bool panel_on;
int panel[4];
uint64_t previous;
int frame_limit;
bool fill_window;
void layout_view()
{
    unsigned fb_w = current.color ? current.color->width : display_width,
             fb_h = current.color ? current.color->height : display_height;
    if (panel_on) {
        side_margin = view_w * ((16.f / 9) / (4.f / 3) - 1) * .5f;
        scale_x = panel[2] / (view_w + 2 * side_margin);
        scale_y = panel[3] / view_h;
        content_x = panel[0];
        content_width = panel[2];
        off_x = panel[0] + side_margin * scale_x;
        off_y = panel[1];
        return;
    }
    if (fill_window) {
        scale_x = scale_y = (float)fb_w / view_w;
        content_width = (float)fb_w;
        content_x = off_x = side_margin = 0;
        off_y = ((float)fb_h - view_h * scale_y) * .5f;
        return;
    }
    float height = std::min((float)fb_h, fb_w / (16.f / 9)), width = height * (16.f / 9);
    scale_x = scale_y = height / view_h;
    content_width = width;
    content_x = (fb_w - width) * .5f;
    side_margin = (width / scale_x - view_w) * .5f;
    off_x = content_x + side_margin * scale_x;
    off_y = (fb_h - height) * .5f;
    if (menu_projection) {
        scale_x = width / view_w;
        side_margin = 0;
        off_x = content_x;
    }
}
void draw_batch(nd_program program = ND_UI, const float *weights = nullptr,
                const unsigned *sources = nullptr, const nd_uniform *override_uniform = nullptr)
{
    if (batch.empty())
        return;
    DkRasterizerState raster;
    dkRasterizerStateDefaults(&raster);
    raster.cullMode = DkFace_None;
    DkDepthStencilState depth;
    dkDepthStencilStateDefaults(&depth);
    depth.depthTestEnable = depth.depthWriteEnable = false;
    DkColorState color;
    dkColorStateDefaults(&color);
    color.blendEnableMask = program == ND_UI ? 1 : 0;
    DkColorWriteState mask;
    dkColorWriteStateDefaults(&mask);
    DkBlendState blend;
    dkBlendStateDefaults(&blend);
    /* Match video_gl.c: transparent UI retains the destination alpha used
       by subsequent DATE masks, including captures of the showroom. */
    blend.dstAlphaBlendFactor = DkBlendFactor_InvSrcAlpha;
    if (batch_fixed_alpha >= 0) {
        blend.srcColorBlendFactor = DkBlendFactor_ConstAlpha;
        blend.dstColorBlendFactor = DkBlendFactor_InvConstAlpha;
    }
    nd_uniform uniforms{};
    uniforms.v[0][0] = batch_kind;
    uniforms.v[0][1] = 1;
    if (weights)
        std::memcpy(uniforms.v[1], weights, 16);
    if (override_uniform)
        uniforms = *override_uniform;
    unsigned images[4]{batch_texture ? batch_texture : white->slot, white->slot, white->slot,
                       white->slot};
    if (sources)
        std::memcpy(images, sources, sizeof images);
    nd_bind(current.color, current.depth);
    DkGpuAddr address = nd_stream(batch.data(), batch.size() * sizeof(Vertex));
    nd_buffer buffer{};
    buffer.size = batch.size() * sizeof(Vertex);
    nd_viewport(0, 0, current.color->width, current.color->height);
    nd_scissor(current.clip[0], current.clip[1], current.clip[2], current.clip[3]);
    nd_pipeline(program, uniforms, buffer, 0, batch.size(), images, raster, depth, color, mask,
                blend, std::max(0.f, batch_fixed_alpha), 0, address);
    batch.clear();
}
void quad(unsigned texture, int kind, const float x[4], const float y[4], const float u[4],
          const float v[4], const uint8_t colors[4][4])
{
    float left = *std::min_element(x, x + 4), right = *std::max_element(x, x + 4),
          top = *std::min_element(y, y + 4), bottom = *std::max_element(y, y + 4);
    if (right < -side_margin || left > view_w + side_margin || bottom < 0 || top > view_h)
        return;
    if (texture != batch_texture || kind != batch_kind || batch.size() + 6 > 16384) {
        draw_batch();
        batch_texture = texture;
        batch_kind = kind;
    }
    static const unsigned order[6]{0, 1, 2, 2, 1, 3};
    for (unsigned k : order) {
        float px = off_x + x[k] * scale_x;
        if (kind == 0 && std::fabs(x[0]) < .01f && std::fabs(x[2]) < .01f &&
            std::fabs(x[1] - view_w) < .01f && std::fabs(x[3] - view_w) < .01f)
            px += (k == 0 || k == 2) ? -side_margin * scale_x : side_margin * scale_x;
        Vertex out{px / current.color->width * 2 - 1,
                   1 - (off_y + y[k] * scale_y) / current.color->height * 2,
                   u[k],
                   v[k],
                   {}};
        for (unsigned c = 0; c < 4; c++)
            out.color[c] = colors[k][c] / 255.f;
        batch.push_back(out);
    }
}
} // namespace
nv_state nv_get()
{
    video_flush2d();
    return current;
}
void nv_set(const nv_state &s)
{
    video_flush2d();
    current = s;
    nd_bind(s.color, s.depth);
    nd_viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    nd_scissor(s.clip[0], s.clip[1], s.clip[2], s.clip[3]);
    layout_view();
}
void nv_target(nd_image *color, nd_image *depth)
{
    video_flush2d();
    current.color = color;
    current.depth = depth;
    current.viewport[0] = current.viewport[1] = current.clip[0] = current.clip[1] = 0;
    current.viewport[2] = current.clip[2] = color->width;
    current.viewport[3] = current.clip[3] = color->height;
    nv_set(current);
}
void nv_view(int x, int y, int w, int h)
{
    video_flush2d();
    current.viewport[0] = x;
    current.viewport[1] = y;
    current.viewport[2] = w;
    current.viewport[3] = h;
    nd_viewport(x, y, w, h);
}
unsigned nv_white()
{
    return white->slot;
}
unsigned nv_black()
{
    return black->slot;
}
void nv_snapshot(nd_image *source, nd_image *target, unsigned x, unsigned y, unsigned w, unsigned h)
{
    auto saved = nv_get();
    /* Both producer and consumer are now fragments on the 3D engine.
       General 2D/copy-engine transfers retain their Full barriers. */
    dkCmdBufBarrier(nd_commands(), DkBarrier_Fragments, DkInvalidateFlags_Image);
    nv_target(target, nullptr);
    current.clip[0] = x;
    current.clip[1] = y;
    current.clip[2] = w;
    current.clip[3] = h;
    Vertex vertices[6];
    nd_full_image_quad(vertices);
    batch.assign(vertices, vertices + 6);
    batch_texture = source->slot;
    batch_kind = 0;
    draw_batch(target->alpha_mask ? ND_MASK : ND_SNAPSHOT);
    /* Restoring a different target emits a fragment completion/cache barrier
       before the saved alpha is sampled by the next mesh draw. */
    nv_set(saved);
}
void nv_draw_environment(nd_image *target, nd_image *atlas, int face_size, const float r[3],
                         const float u[3], const float f[3])
{
    auto saved = nv_get();
    nv_target(target, nullptr);
    Vertex vertices[6];
    nd_full_image_quad(vertices);
    batch.assign(vertices, vertices + 6);
    batch_texture = batch_kind = 0;
    nd_uniform params{};
    params.v[0][0] = face_size;
    params.v[0][1] = target->width;
    std::memcpy(params.v[1], r, 3 * sizeof(float));
    std::memcpy(params.v[2], u, 3 * sizeof(float));
    std::memcpy(params.v[3], f, 3 * sizeof(float));
    unsigned sources[4]{atlas->slot, white->slot, white->slot, white->slot};
    draw_batch(ND_ENV, nullptr, sources, &params);
    nv_set(saved);
}
void nv_draw_mix(nd_image *target, nd_image *const layers[4], const float weights[4])
{
    auto saved = nv_get();
    nv_target(target, nullptr);
    Vertex vertices[6];
    nd_full_image_quad(vertices);
    batch.assign(vertices, vertices + 6);
    batch_texture = 0;
    batch_kind = 0;
    unsigned sources[4];
    for (unsigned i = 0; i < 4; i++)
        sources[i] = layers[i]->slot;
    draw_batch(ND_MIX, weights, sources);
    nv_set(saved);
}
extern "C" {
int gl_reversed_z = 0;
unsigned gl_program_count(void)
{
    return nd_program_count();
}
SDL_Window *plat_sdl_window(void)
{
    return nullptr;
}
int plat_video_init(const plat_video_desc *)
{
    std::fprintf(stderr, "Switch deko3d init: SDL\n");
    std::fflush(stderr);
    if (nd_events_init() < 0)
        return -1;
    display_width = appletGetOperationMode() == AppletOperationMode_Console ? 1920 : 1280;
    display_height = display_width == 1920 ? 1080 : 720;
    /* SDK SDL_switchvideo.c SWITCH_CreateWindow unconditionally creates an EGL
       surface, even without SDL_WINDOW_OPENGL. Leave the NWindow exclusively
       to deko3d; Switch SDL event pumping does not require an SDL window. */
    std::fprintf(stderr, "Switch deko3d init: SDL events ready, no SDL/EGL window\n");
    std::fflush(stderr);
    if (!nd_init(display_width, display_height))
        return -1;
    initialized = true;
    page = nd_image_create(display_width, display_height);
    nv_target(page, nullptr);
    nd_clear(0, 0, 0, 1, true, false);
    white = nd_image_create(1, 1);
    uint8_t rgba[4]{255, 255, 255, 255};
    nd_image_upload(white, rgba);
    black = nd_image_create(1, 1);
    const uint8_t zero[4]{0, 0, 0, 255};
    nd_image_upload(black, zero);
    batch.reserve(16384);
    /* Once at boot: exercise uploads, descriptors, vertex layout, sampling,
       offscreen rendering and readback. No synchronous readback in normal frames. */
    auto saved = nv_get();
    plat_texture *probe = plat_texture_create(2, 2, nullptr);
    const uint8_t colors[16]{255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    nd_image_upload(probe->image, colors);
    uint8_t uploaded[16]{};
    bool upload_ok =
        nd_image_read(probe->image, uploaded) && nd_rgba_equal(colors, uploaded, 16, 0);
    std::fprintf(stderr, "Switch deko3d selftest: upload=%s\n", upload_ok ? "PASS" : "FAIL");
    if (!upload_ok)
        for (unsigned i = 0; i < 4; i++)
            std::fprintf(stderr, "Switch deko3d upload texel %u: %u,%u,%u,%u\n", i, uploaded[i * 4],
                         uploaded[i * 4 + 1], uploaded[i * 4 + 2], uploaded[i * 4 + 3]);
    /* A square target was letterboxed to 16:9; the former bottom samples lay
       on the excluded bottom edge. Keep the actual colour check, fix its view. */
    auto *target = nd_image_create(ND_UI_PROBE_WIDTH, ND_UI_PROBE_HEIGHT);
    nv_target(target, nullptr);
    plat_video_set_view(ND_UI_PROBE_WIDTH, ND_UI_PROBE_HEIGHT);
    nd_clear(0, 0, 0, 1, true, false);
    const uint8_t tint[4]{255, 255, 255, 255};
    plat_draw_quad(probe, 0, 0, ND_UI_PROBE_WIDTH, ND_UI_PROBE_HEIGHT, 0, 0, 1, 1, tint);
    video_flush2d();
    uint8_t pixels[ND_UI_PROBE_WIDTH * ND_UI_PROBE_HEIGHT * 4]{};
    bool quad_ok = nd_image_read(target, pixels);
    for (unsigned i = 0; i < 4; i++) {
        unsigned offset = (nd_ui_probe_xy[i][1] * ND_UI_PROBE_WIDTH + nd_ui_probe_xy[i][0]) * 4;
        bool corner_ok = nd_rgba_equal(colors + i * 4, pixels + offset, 4, 1);
        std::fprintf(stderr,
                     "Switch deko3d corner %u: actual=%u,%u,%u,%u expected=%u,%u,%u,%u %s\n", i,
                     pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3],
                     colors[i * 4], colors[i * 4 + 1], colors[i * 4 + 2], colors[i * 4 + 3],
                     corner_ok ? "PASS" : "FAIL");
        quad_ok = corner_ok && quad_ok;
    }
    /* Exercise the 2D blit engine on a non-power-of-two image too: normal
       presentation and showroom captures use it, unlike the draw above. */
    auto *copied = nd_image_create(ND_UI_PROBE_WIDTH, ND_UI_PROBE_HEIGHT);
    nd_copy(target, copied, 0, 0, ND_UI_PROBE_WIDTH, ND_UI_PROBE_HEIGHT);
    uint8_t copied_pixels[sizeof pixels]{};
    bool copy_ok = nd_image_read(copied, copied_pixels) &&
                   nd_rgba_equal(pixels, copied_pixels, sizeof pixels, 1);
    std::fprintf(stderr, "Switch deko3d selftest: image-blit=%s\n", copy_ok ? "PASS" : "FAIL");
    nd_image_destroy(copied);
    /* Composition targets intentionally differ from the display aspect.
       Verify all their rows, not only the center of a 16:9 presentation. */
    const uint8_t layer_color[4][4]{
        {192, 24, 80, 255}, {16, 176, 48, 255}, {64, 32, 224, 255}, {128, 96, 32, 255}};
    const float weights[4]{.25f, .5f, .125f, .125f};
    const uint8_t expected_mix[4]{80, 110, 76, 255};
    nd_image *layers[4]{};
    for (unsigned i = 0; i < 4; i++) {
        layers[i] = nd_image_create(1, 1);
        nd_image_upload(layers[i], layer_color[i]);
    }
    bool mix_ok = true;
    const unsigned mix_sizes[2][2]{{640, 448}, {256, 256}};
    for (auto &size : mix_sizes) {
        nd_image *mixed = nd_image_create(size[0], size[1]);
        nv_target(mixed, nullptr);
        nd_clear(1, 0, 1, 0, true, false);
        nv_draw_mix(mixed, layers, weights);
        std::vector<uint8_t> mixed_pixels((size_t)size[0] * size[1] * 4);
        bool covered = nd_image_read(mixed, mixed_pixels.data());
        for (size_t at = 0; covered && at < mixed_pixels.size(); at += 4)
            covered = nd_rgba_equal(expected_mix, mixed_pixels.data() + at, 4, 1);
        std::fprintf(stderr, "Switch deko3d selftest: composition-full=%s size=%ux%u\n",
                     covered ? "PASS" : "FAIL", size[0], size[1]);
        mix_ok = covered && mix_ok;
        nv_set(saved);
        nd_image_destroy(mixed);
    }
    for (auto layer : layers)
        nd_image_destroy(layer);
    /* Boot-only validation: exercise all alpha byte values and dependent
       3D ping-pong snapshots without an intermediate CPU/GPU wait. */
    bool snapshot_ok = true;
    {
        constexpr unsigned w = 32, h = 24, bytes = w * h * 4;
        uint8_t source[bytes], old[bytes], expected[bytes], actual[bytes];
        for (unsigned i = 0; i < bytes; ++i) {
            source[i] = (uint8_t)(i * 37 + i / 4);
            old[i] = (uint8_t)(255 - source[i]);
        }
        nd_image *src = nd_image_create(w, h), *dst = nd_image_create(w, h);
        nd_image_upload(src, source);
        nd_image_upload(dst, old);
        for (unsigned i = 0; i < 20; ++i) {
            nv_snapshot(src, dst, 0, 0, w, h);
            nv_snapshot(dst, src, 0, 0, w, h);
        }
        snapshot_ok = nd_image_read(dst, actual) && !std::memcmp(actual, source, bytes);
        nd_image_upload(dst, old);
        nv_snapshot(src, dst, 3, 5, 17, 11);
        std::memcpy(expected, old, bytes);
        for (unsigned y = 5; y < 16; ++y)
            std::memcpy(expected + (y * w + 3) * 4, source + (y * w + 3) * 4, 17 * 4);
        snapshot_ok =
            nd_image_read(dst, actual) && !std::memcmp(actual, expected, bytes) && snapshot_ok;
        nd_image *mask = nd_image_create(w, h, false, false, true);
        nv_snapshot(src, mask, 0, 0, w, h);
        uint8_t scalar[w * h];
        bool mask_ok = nd_image_read(mask, scalar);
        for (unsigned i = 0; i < w * h; ++i)
            mask_ok = mask_ok && scalar[i] == source[i * 4 + 3];
        nd_image_upload(src, old);
        nv_snapshot(src, mask, 3, 5, 17, 11);
        mask_ok = nd_image_read(mask, scalar) && mask_ok;
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x) {
                unsigned i = y * w + x;
                uint8_t wanted =
                    x >= 3 && x < 20 && y >= 5 && y < 16 ? old[i * 4 + 3] : source[i * 4 + 3];
                mask_ok = mask_ok && scalar[i] == wanted;
            }
        std::fprintf(stderr, "Switch deko3d selftest: scalar-DATE=%s full/crop/alpha256\n",
                     mask_ok ? "PASS" : "FAIL");
        snapshot_ok = snapshot_ok && mask_ok;
        nv_set(saved);
        nd_image_destroy(mask);
        nd_image_destroy(src);
        nd_image_destroy(dst);
    }
    std::fprintf(stderr, "Switch deko3d selftest: snapshot-3D=%s full/crop/dependent=41\n",
                 snapshot_ok ? "PASS" : "FAIL");
    bool ok = upload_ok && quad_ok && copy_ok && mix_ok && snapshot_ok;
    nv_set(saved);
    plat_video_set_view(640, 480);
    plat_texture_destroy(probe);
    nd_image_destroy(target);
    std::fprintf(stderr, "Switch deko3d selftest: upload/2D/corners=%s\n", ok ? "PASS" : "FAIL");
    std::fflush(stderr);
    if (!ok) {
        plat_video_shutdown();
        SDL_SetError("deko3d GPU startup check failed; see moonengine.log");
        return -1;
    }
    return 0;
}
void video_flush2d(void)
{
    draw_batch();
}
void plat_video_begin_frame(void)
{
    nd_begin();
    nv_target(page, nullptr);
    nd_clear(0, 0, 0, 1, true, false);
}
void plat_video_end_frame(void)
{
    video_flush2d();
    SWP_PUSH(SWP_SWAP);
    nd_copy(page, nd_screen(), 0, 0, page->width, page->height);
    nd_present();
    SWP_POP();
    unsigned rate = menu_pacing ? 60u : (unsigned)frame_limit;
    if (rate && previous) {
        uint64_t interval = 1000000000ull / rate;
        uint64_t elapsed = plat_time_ns() - previous;
        if (elapsed < interval)
            plat_sleep_ms((interval - elapsed) / 1000000);
    }
    previous = plat_time_ns();
    SWP_FRAME(display_width, display_height);
}
void plat_video_shutdown(void)
{
    if (!initialized)
        return;
    video_flush2d();
    nd_sync();
    nd_render_shutdown();
    nd_image_destroy(white);
    nd_image_destroy(black);
    nd_image_destroy(page);
    nd_image_destroy(scene);
    nd_image_destroy(scene_depth);
    nd_image_destroy(license_lut);
    nd_shutdown();
    initialized = false;
}
void video_set_menu_pacing(int enabled)
{
    menu_pacing = enabled;
}
int video_export_fixed_step(void)
{
    return 0;
}
void video_set_menu_projection(int enabled)
{
    video_flush2d();
    menu_projection = enabled;
    layout_view();
}
void video_set_aspect(int)
{
    layout_view();
}
void video_set_fill_window(int enabled)
{
    video_flush2d();
    fill_window = enabled != 0;
    layout_view();
}
int video_fill_window(void)
{
    return fill_window;
}
void video_performance_options(int low_latency, int fps_limit)
{
    /* The native queue owns presentation; the desktop Vulkan latency option is inapplicable. */
    (void)low_latency;
    frame_limit = fps_limit >= 0 && fps_limit <= 500 ? fps_limit : 0;
    previous = 0;
}
float video_projection_x(void)
{
    return fill_window ? 640.f / view_w : view_w / (view_h * (16.f / 9));
}
int video_set_upscale(int)
{
    return 0;
}
int video_fsr_available(void)
{
    return 0;
}
int video_set_vsync(int)
{
    return 0;
}
void video_ui_quality(int, float)
{
}
void video_begin_scene(void)
{
    if (scene_active)
        return;
    scene_saved = nv_get();
    unsigned w = display_width >= 1920 ? 1280 : 960, h = display_height >= 1080 ? 720 : 540;
    if (!scene) {
        scene = nd_image_create(w, h);
        scene_depth = nd_image_create(w, h, true);
    }
    nv_target(scene, scene_depth);
    nd_clear(0, 0, 0, 1, true, true);
    scene_active = true;
}
void video_end_scene(void)
{
    if (!scene_active)
        return;
    video_flush2d();
    nd_copy(scene, scene_saved.color, 0, 0, scene->width, scene->height);
    nv_set(scene_saved);
    scene_active = false;
}
void plat_video_set_view(float w, float h)
{
    video_flush2d();
    if (w > 0 && h > 0) {
        view_w = w;
        view_h = h;
    }
    layout_view();
}
void plat_set_clip(float x, float y, float w, float h)
{
    video_flush2d();
    if (w <= 0 || h <= 0) {
        current.clip[0] = current.clip[1] = 0;
        current.clip[2] = current.color->width;
        current.clip[3] = current.color->height;
    } else {
        nd_clip_horizontal(x, w, view_w, off_x, scale_x, content_x, content_width, current.clip[0],
                           current.clip[2]);
        current.clip[1] = std::max(0, (int)std::floor(off_y + y * scale_y + .5f));
        current.clip[3] = std::max(0, (int)std::floor(h * scale_y + .5f));
    }
    nd_scissor(current.clip[0], current.clip[1], current.clip[2], current.clip[3]);
}
plat_texture *plat_texture_create(int w, int h, const uint8_t *rgba)
{
    if (w <= 0 || h <= 0)
        return nullptr;
    auto *t = new plat_texture{nd_image_create(w, h), false, false};
    if (rgba)
        nd_image_upload(t->image, rgba);
    return t;
}
void plat_texture_update(plat_texture *t, const uint8_t *rgba)
{
    video_flush2d();
    if (t)
        nd_image_upload(t->image, rgba);
}
void plat_texture_destroy(plat_texture *t)
{
    if (!t)
        return;
    video_flush2d();
    nd_image_destroy(t->image);
    delete t;
}
unsigned video_texture_id(const plat_texture *t)
{
    return t ? t->image->slot : 0;
}
unsigned video_draw_framebuffer(void)
{
    return current.color ? current.color->slot : 0;
}
float video_view_height(void)
{
    return view_h;
}
void video_viewport(float x, float y, float w, float h, int *vx, int *vy, int *vw, int *vh)
{
    *vx = (int)std::floor(off_x + x * scale_x + .5f);
    *vy = (int)current.color->height - (int)std::floor(off_y + (y + h) * scale_y + .5f);
    *vw = (int)std::floor(w * scale_x + .5f);
    *vh = (int)std::floor(h * scale_y + .5f);
    if (x == 0 && w == view_w) {
        nd_content_viewport(content_x, content_width, *vx, *vw);
    }
}
void video_ui_panel(float y, float h)
{
    video_flush2d();
    panel_on = false;
    layout_view();
    if (h > 0) {
        int vx, vy, vw, vh;
        video_viewport(0, y, 640, h, &vx, &vy, &vw, &vh);
        panel[0] = vx;
        panel[1] = current.color->height - vy - vh;
        panel[2] = vw;
        panel[3] = vh;
        panel_on = true;
        std::memcpy(current.clip, panel, sizeof panel);
    } else {
        current.clip[0] = current.clip[1] = 0;
        current.clip[2] = current.color->width;
        current.clip[3] = current.color->height;
    }
    layout_view();
    nd_scissor(current.clip[0], current.clip[1], current.clip[2], current.clip[3]);
}
plat_texture *video_grab(plat_texture *t, float x, float y, float w, float h)
{
    video_flush2d();
    int vx, vy, vw, vh;
    video_viewport(x, y, w, h, &vx, &vy, &vw, &vh);
    vx = std::max(0, vx);
    vy = std::max(0, (int)current.color->height - vy - vh);
    vw = std::min(vw, (int)current.color->width - vx);
    vh = std::min(vh, (int)current.color->height - vy);
    if (vw <= 0 || vh <= 0)
        return t;
    if (!t || t->image->width != (unsigned)vw || t->image->height != (unsigned)vh) {
        plat_texture_destroy(t);
        t = plat_texture_create(vw, vh, nullptr);
    }
    nd_copy(current.color, t->image, vx, vy, vw, vh);
    t->screen_capture = x <= 0 && x + w >= view_w;
    t->gpu_capture = true;
    return t;
}
plat_texture *video_capture_frame(plat_texture *t)
{
    return video_grab(t, 0, 0, view_w, view_h);
}
unsigned char *plat_sdl_read_pixels(int *w, int *h)
{
    video_flush2d();
    *w = current.color->width;
    *h = current.color->height;
    auto *rgba = (unsigned char *)std::malloc((size_t)*w * *h * 4);
    if (rgba && !nd_image_read(current.color, rgba)) {
        std::free(rgba);
        return nullptr;
    }
    return rgba;
}
unsigned char *plat_sdl_read_view(int *w, int *h)
{
    return plat_sdl_read_pixels(w, h);
}
int video_read_texture(const plat_texture *t, uint8_t *rgba)
{
    video_flush2d();
    if (!t || !nd_image_read(t->image, rgba))
        return 0;
    /* Existing photo export flips GL rows; retain that caller contract. */
    unsigned w = t->image->width, h = t->image->height;
    for (unsigned y = 0; y < h / 2; y++)
        for (unsigned x = 0; x < w * 4; x++)
            std::swap(rgba[y * w * 4 + x], rgba[(h - 1 - y) * w * 4 + x]);
    return 1;
}
void video_texture_dimensions(const plat_texture *t, int *w, int *h)
{
    *w = t ? t->image->width : 0;
    *h = t ? t->image->height : 0;
}
float video_read_depth(int x, int y)
{
    auto *depth = current.depth;
    if (!depth)
        return 1;
    auto *values = (float *)std::malloc((size_t)depth->width * depth->height * 4);
    float result = 1;
    if (values && nd_image_read(depth, values) && x >= 0 && y >= 0 && (unsigned)x < depth->width &&
        (unsigned)y < depth->height)
        result = values[((int)depth->height - 1 - y) * depth->width + x];
    std::free(values);
    return result;
}
}

extern "C" {
void plat_draw_quad4(const plat_texture *t, float x, float y, float w, float h, float u0, float v0,
                     float u1, float v1, const uint8_t c[4][4])
{
    /* 002182E0 +0xA4..0xF0 draws the retained framebuffer in its own
       dimensions. A capture includes the wide viewport; map every scaled
       piece in that space, not only quads exactly spanning 0..view_w.
       GL adapter A: this does not certify the original GS rasterization. */
    if (t && t->screen_capture && side_margin > 0) {
        float ratio = (view_w + 2 * side_margin) / view_w;
        x = view_w * .5f + (x - view_w * .5f) * ratio;
        w *= ratio;
    }
    if (t && t->gpu_capture) {
        v0 = 1 - v0;
        v1 = 1 - v1;
    }
    const float px[4] = {x, x + w, x, x + w}, py[4] = {y, y, y + h, y + h};
    const float tu[4] = {u0, u1, u0, u1}, tv[4] = {v0, v0, v1, v1};
    quad(t ? t->image->slot : 0, 0, px, py, tu, tv, c);
}

void plat_draw_quad(const plat_texture *t, float x, float y, float w, float h, float u0, float v0,
                    float u1, float v1, const uint8_t c[4])
{
    uint8_t cc[4][4];
    for (int i = 0; i < 4; i++)
        memcpy(cc[i], c, 4);
    plat_draw_quad4(t, x, y, w, h, u0, v0, u1, v1, cc);
}

void plat_draw_solid_quad(const float xy[4][2], const uint8_t c[4])
{
    float x[4], y[4];
    const float uv[4] = {0, 0, 0, 0};
    uint8_t colors[4][4];
    for (int i = 0; i < 4; i++) {
        x[i] = xy[i][0];
        y[i] = xy[i][1];
        memcpy(colors[i], c, 4);
    }
    quad(0, 0, x, y, uv, uv, colors);
}

void plat_draw_quad_blend(const plat_texture *t, float x, float y, float w, float h, float u0,
                          float v0, float u1, float v1, const uint8_t color[4], float factor)
{
    video_flush2d();
    batch_fixed_alpha = fminf(1, fmaxf(0, factor));
    plat_draw_quad(t, x, y, w, h, u0, v0, u1, v1, color);
    video_flush2d();
    batch_fixed_alpha = -1;
}

void plat_draw_glyph(const plat_texture *t, float x, float y, float w, float h, float baseline,
                     float shear, float u0, float v0, float u1, float v1, const uint8_t c[4])
{
    float px[4], py[4];
    const float tu[4] = {u0, u1, u0, u1}, tv[4] = {v0, v0, v1, v1};
    uint8_t cc[4][4];
    for (int i = 0; i < 4; i++) {
        py[i] = y + (i / 2) * h;
        px[i] = x + (i % 2) * w + shear * (py[i] - baseline);
        memcpy(cc[i], c, 4);
    }
    quad(t ? t->image->slot : 0, 1, px, py, tu, tv, cc);
}

void plat_draw_rotated(const plat_texture *t, float x, float y, float w, float h, float cx,
                       float cy, float degrees, const uint8_t c[4])
{
    float a = degrees * 0.01745329252f, sn = sinf(a), cs = cosf(a), px[4], py[4];
    const float tu[4] = {0, 1, 0, 1}, tv[4] = {0, 0, 1, 1};
    uint8_t cc[4][4];
    for (int i = 0; i < 4; i++) {
        float xx = x + (i % 2) * w - cx, yy = y + (i / 2) * h - cy;
        px[i] = cx + cs * xx - sn * yy;
        py[i] = cy + sn * xx + cs * yy;
        memcpy(cc[i], c, 4);
    }
    quad(t ? t->image->slot : 0, 0, px, py, tu, tv, cc);
}

/* 004A81A8's existing channel/matrix boundary, implemented by the native UI shader. */
void video_license_image(const plat_texture *t, float x, float y, float w, float h,
                         const uint8_t lut[256][3], const uint8_t mix[3][3])
{
    if (!t)
        return;
    video_flush2d();
    if (!license_lut)
        license_lut = nd_image_create(256, 1);
    if (!lut_valid || std::memcmp(cached_lut, lut, sizeof cached_lut)) {
        uint8_t rgba[256][4];
        for (unsigned i = 0; i < 256; i++) {
            std::memcpy(rgba[i], lut[i], 3);
            rgba[i][3] = 255;
        }
        nd_image_upload(license_lut, &rgba[0][0]);
        std::memcpy(cached_lut, lut, sizeof cached_lut);
        lut_valid = true;
    }
    nd_uniform params{};
    params.v[0][0] = 2;
    params.v[0][1] = 1;
    for (unsigned i = 0; i < 3; i++)
        for (unsigned c = 0; c < 3; c++)
            params.v[40 + i][c] = mix[i][c];
    unsigned sources[4]{t->image->slot, license_lut->slot, white->slot, white->slot};
    const uint8_t tint[4]{255, 255, 255, 255};
    plat_draw_quad(t, x, y, w, h, 0, 1, 1, 0, tint);
    draw_batch(ND_UI, nullptr, sources, &params);
}
}
