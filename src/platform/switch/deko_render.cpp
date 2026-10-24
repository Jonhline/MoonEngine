/* P: Switch-specific forward materials on native deko3d. */
#include "deko_video.h"
#include "deko_transfer.h"
#include "deko_blend.h"
#include "deko_clear.h"
#include "deko_snapshot.h"
#include "deko_index.h"
extern "C" {
#include "render3d.h"
#include "render3d_frustum.h"
#include "platform/switch_profile.h"
#include "switch_render.h"
}
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
namespace {
struct Texture {
    nd_image *image{};
    int cc{}, native{}, function{}, sampler{};
    int tex_w{}, tex_h{}, wrap_u{}, wrap_v{}, nearest{};
    unsigned region[4]{};
};
struct Textures {
    std::vector<Texture> entries;
    ~Textures()
    {
        for (auto &t : entries)
            nd_image_destroy(t.image);
    }
};
struct Vertex {
    float values[27];
};
struct Group {
    unsigned first{}, count{}, model{};
    int texture{}, blur{};
    unsigned kind{};
    uint8_t lamp{}, blend{};
    gt4_draw_state state{};
    unsigned bone_mask{};
    float weight_lo{1e30f}, weight_hi{};
    float lo[3]{1e30f, 1e30f, 1e30f}, hi[3]{-1e30f, -1e30f, -1e30f};
};
struct Context {
    r3d_stats stats{};
    nd_image *environment{}, *borrowed_environment{}, *ui_depth{}, *destination{};
    const tex1_image *environment_source{};
    bool environment_dirty{};
    float projection{1}, target_projection{1};
    nv_state target_saved{}, layer_saved{};
    nd_image *target{}, *target_depth{}, *layer{}, *layer_depth{};
    bool in_target{}, in_layer{};
} state;
void copy3(float *dst, const float *src)
{
    std::memcpy(dst, src, 3 * sizeof(float));
}
void upload_texture(Textures &textures, unsigned index, const gt4_model *model)
{
    if (index >= (unsigned)model->ntextures || textures.entries[index].image)
        return;
    const tex1_image &im = model->textures[index];
    if (!im.rgba || im.width <= 0 || im.height <= 0)
        return;
    Texture &t = textures.entries[index];
    /* Tex1 REGION_CLAMP storage is cropped (docs/TEX1_FORMAT.md). Keep the
       decoded extent and apply TEX0's nominal size when addressing each tap,
       as render3d.c does; padding to TW/TH exposed black texels on bodywork. */
    unsigned w = im.width, h = im.height;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++) {
            size_t src = (size_t)y * im.width + x, dst = (size_t)y * w + x;
            std::memcpy(rgba.data() + dst * 4, im.rgba + src * 4, 4);
            if (im.native_alpha)
                rgba[dst * 4 + 3] = im.native_alpha[src];
        }
    t.image = nd_image_create(w, h);
    nd_image_upload(t.image, rgba.data());
    t.cc = im.color_component;
    t.native = im.native_alpha != nullptr;
    t.function = im.texture_function;
    t.sampler = (im.wrap_u == 0 ? 1 : 0) | (im.wrap_v == 0 ? 2 : 0);
    t.tex_w = im.tex_w;
    t.tex_h = im.tex_h;
    t.wrap_u = im.wrap_u;
    t.wrap_v = im.wrap_v;
    t.nearest = !im.magnify && !im.minify;
    t.region[0] = im.min_u;
    t.region[1] = im.max_u;
    t.region[2] = im.min_v;
    t.region[3] = im.max_v;
}
void blend_state(const Group &g, DkColorState &color, DkBlendState &blend)
{
    nd_material_blend(g.blend != 0, g.state.blend, &color, &blend);
}
} // namespace
struct r3d_mesh {
    nd_buffer vertices{};
    DkGpuAddr index_address{};
    unsigned index_count{};
    std::vector<Vertex> cpu;
    std::vector<Group> groups;
    std::shared_ptr<Textures> textures;
    bool skinned{}, transient{};
};
struct r3d_sprite_texture {
    nd_image *image;
    bool nearest;
};
struct switch_gpu_image {
    plat_texture texture{};
    nd_image *layers[4]{};
    nd_image *atlas{};
    int face_size{};
};
extern "C" {
int r3d_available(void)
{
    return nd_program_count() != 0;
}
int r3d_warmup(void)
{
    static bool checked = false;
    if (checked)
        return r3d_available();
    gt4_triangle triangle{};
    triangle.texture = -1;
    triangle.kind = 0;
    triangle.state.depth_test = triangle.state.depth_write = 1;
    triangle.state.depth_func = 3;
    const float positions[3][3]{{-1, -1, 2}, {1, -1, 2}, {0, 1, 2}};
    for (unsigned i = 0; i < 3; i++) {
        copy3(triangle.v[i].p, positions[i]);
        triangle.v[i].normal[2] = -1;
        triangle.v[i].alpha = 1;
        for (float &c : triangle.v[i].rgb)
            c = 1;
    }
    gt4_model model{};
    model.triangles = &triangle;
    model.count = 1;
    auto *mesh = r3d_mesh_create(&model);
    r3d_view view{};
    view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    auto saved = nv_get();
    r3d_begin(0, 0, 640, 448, nullptr);
    r3d_begin_target(16);
    r3d_draw(mesh, &view);
    uint8_t pixels[16 * 16 * 4];
    float depths[16 * 16];
    bool ok = nd_image_read(state.target, pixels) && nd_image_read(state.target_depth, depths);
    const uint8_t expected[4]{128, 128, 128, 255};
    ok = nd_rgba_equal(expected, pixels + (8 * 16 + 8) * 4, 4, 1) && ok;
    ok = depths[8 * 16 + 8] > .89f && depths[8 * 16 + 8] < .91f && ok;
    /* The same opaque triangle twice shares all three vertex records. Compare
       the actual indexed draw, including depth, against the first draw above. */
    gt4_triangle repeated[2]{triangle, triangle};
    gt4_model indexed_model{};
    indexed_model.triangles = repeated;
    indexed_model.count = 2;
    auto *indexed_mesh = r3d_mesh_create(&indexed_model);
    uint8_t indexed_pixels[sizeof pixels];
    float indexed_depths[16 * 16];
    nd_clear(0, 0, 0, 1, true, true);
    if (indexed_mesh)
        r3d_draw(indexed_mesh, &view);
    bool indexed_ok = indexed_mesh && indexed_mesh->index_count == 6 &&
                      indexed_mesh->vertices.size == 3 * sizeof(Vertex) &&
                      nd_image_read(state.target, indexed_pixels) &&
                      nd_image_read(state.target_depth, indexed_depths) &&
                      !std::memcmp(pixels, indexed_pixels, sizeof pixels) &&
                      !std::memcmp(depths, indexed_depths, sizeof depths);
    std::fprintf(stderr, "Switch deko3d selftest: indexed-mesh=%s duplicate=6 unique=3\n",
                 indexed_ok ? "PASS" : "FAIL");
    r3d_mesh_destroy(indexed_mesh);
    ok = indexed_ok && ok;
    /* Exercise real deko transfer/mask reuse, not just the GLSL formula.
       Red writes alpha .5, DATM=0 green then DATM=1 blue each snapshot the
       preceding pass. No readback separates these dependent draws. */
    const float rgb[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (unsigned pass = 0; pass < 3; pass++) {
        triangle.state.destination_test = pass != 0;
        triangle.state.destination_bit = pass == 2;
        for (unsigned vertex = 0; vertex < 3; vertex++) {
            copy3(triangle.v[vertex].rgb, rgb[pass]);
            triangle.v[vertex].alpha = pass == 0 ? .5f : 1;
        }
        r3d_mesh *mask_mesh = r3d_mesh_create(&model);
        if (!mask_mesh) {
            ok = false;
            break;
        }
        r3d_draw(mask_mesh, &view);
        r3d_mesh_destroy(mask_mesh);
    }
    const uint8_t blue[4]{0, 0, 128, 255};
    bool mask_ok =
        nd_image_read(state.target, pixels) && nd_rgba_equal(blue, pixels + (8 * 16 + 8) * 4, 4, 1);
    std::fprintf(stderr, "Switch deko3d selftest: reused-DATE=%s\n", mask_ok ? "PASS" : "FAIL");
    ok = mask_ok && ok;
    /* Borrowed particle textures must retain a valid normalized UV extent.
       The RGB-only particle pass also preserves the mask alpha above. */
    const uint8_t green[16]{0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
    const uint8_t particle_alpha[4]{128, 128, 128, 128};
    tex1_image particle{};
    particle.width = particle.height = 2;
    particle.rgba = const_cast<uint8_t *>(green);
    particle.native_alpha = const_cast<uint8_t *>(particle_alpha);
    particle.magnify = 1;
    r3d_sprite_texture *sprite = r3d_sprite_texture_create(&particle);
    const float corners[36]{-1, -1, -1.5f, 0, 0, 1, 1, 1, 1, 1, -1, -1.5f, 1, 0, 1, 1, 1, 1,
                            -1, 1,  -1.5f, 0, 1, 1, 1, 1, 1, 1, 1,  -1.5f, 1, 1, 1, 1, 1, 1};
    if (sprite)
        r3d_draw_sprites(sprite, &view, corners, 1, 0);
    const uint8_t green_pixel[4]{0, 255, 0, 255};
    bool sprite_ok = sprite && nd_image_read(state.target, pixels) &&
                     nd_rgba_equal(green_pixel, pixels + (8 * 16 + 8) * 4, 4, 1);
    r3d_sprite_texture_destroy(sprite);
    std::fprintf(stderr, "Switch deko3d selftest: particle=%s\n", sprite_ok ? "PASS" : "FAIL");
    ok = sprite_ok && ok;
    /* The prior native 1+C fallback used C, incorrectly darkening this
       half-alpha surface. Exercise the actual fixed-function state too. */
    nd_clear(0, 0, 0, 1, true, true);
    const uint8_t cleared[4]{0, 0, 0, 255};
    bool clear_ok =
        nd_image_read(state.target, pixels) && nd_image_read(state.target_depth, depths) &&
        nd_rgba_equal(cleared, pixels + (8 * 16 + 8) * 4, 4, 1) && depths[8 * 16 + 8] == 1;
    std::fprintf(stderr,
                 "Switch deko3d selftest: clear-after-particle=%s rgba=%u,%u,%u,%u depth=%.9g\n",
                 clear_ok ? "PASS" : "FAIL", pixels[(8 * 16 + 8) * 4], pixels[(8 * 16 + 8) * 4 + 1],
                 pixels[(8 * 16 + 8) * 4 + 2], pixels[(8 * 16 + 8) * 4 + 3], depths[8 * 16 + 8]);
    ok = clear_ok && ok;
    triangle.state.destination_test = 0;
    triangle.blend_enabled = 0;
    /* Hardware gpu-compare.log isolated 36fe696's vertex program: even an
       unblended .25/.5/.75 input became alpha 1. Check that boundary before
       the blend equations, so shader and state failures stay distinct. */
    for (unsigned sample = 1; sample <= 3; sample++) {
        nd_clear(0, 0, 0, 1, true, true);
        for (unsigned vertex = 0; vertex < 3; vertex++) {
            copy3(triangle.v[vertex].rgb, rgb[0]);
            triangle.v[vertex].alpha = sample / 4.f;
        }
        r3d_mesh *alpha_mesh = r3d_mesh_create(&model);
        if (alpha_mesh)
            r3d_draw(alpha_mesh, &view);
        const uint8_t expected_alpha[4]{128, 0, 0, (uint8_t)((255 * sample + 2) / 4)};
        bool alpha_ok = alpha_mesh && nd_image_read(state.target, pixels) &&
                        nd_rgba_equal(expected_alpha, pixels + (8 * 16 + 8) * 4, 4, 1);
        r3d_mesh_destroy(alpha_mesh);
        std::fprintf(
            stderr,
            "Switch deko3d selftest: vertex-alpha=%s input=%u/4 rgba=%u,%u,%u,%u expected=128,0,0,%u\n",
            alpha_ok ? "PASS" : "FAIL", sample, pixels[(8 * 16 + 8) * 4],
            pixels[(8 * 16 + 8) * 4 + 1], pixels[(8 * 16 + 8) * 4 + 2],
            pixels[(8 * 16 + 8) * 4 + 3], expected_alpha[3]);
        ok = alpha_ok && ok;
    }
    triangle.blend_enabled = 1;
    for (unsigned vertex = 0; vertex < 3; vertex++) {
        copy3(triangle.v[vertex].rgb, rgb[0]);
        triangle.v[vertex].alpha = .5f;
    }
    const unsigned equations[3]{0x08, 0x44, 0x48};
    for (unsigned equation : equations) {
        nd_clear(0, 0, 0, 1, true, true);
        triangle.state.blend = equation;
        r3d_mesh *blend_mesh = r3d_mesh_create(&model);
        if (blend_mesh)
            r3d_draw(blend_mesh, &view);
        const uint8_t red[4]{(uint8_t)(equation == 0x08 ? 128 : 64), 0, 0, 128};
        bool blend_ok = blend_mesh && nd_image_read(state.target, pixels) &&
                        nd_rgba_equal(red, pixels + (8 * 16 + 8) * 4, 4, 1);
        r3d_mesh_destroy(blend_mesh);
        std::fprintf(
            stderr,
            "Switch deko3d selftest: material-blend=%s mode=%02x rgba=%u,%u,%u,%u expected=%u,0,0,128\n",
            blend_ok ? "PASS" : "FAIL", equation, pixels[(8 * 16 + 8) * 4],
            pixels[(8 * 16 + 8) * 4 + 1], pixels[(8 * 16 + 8) * 4 + 2],
            pixels[(8 * 16 + 8) * 4 + 3], red[0]);
        ok = blend_ok && ok;
    }
    /* Queue twelve different pictures before reading them back. This covers
       all three production frame slots and reuse, unlike the one-frame probe. */
    nd_image *snapshots[12]{};
    r3d_mesh *frames[3]{};
    triangle.blend_enabled = 0;
    for (unsigned i = 0; i < 3; i++) {
        for (unsigned vertex = 0; vertex < 3; vertex++) {
            copy3(triangle.v[vertex].rgb, rgb[i]);
            triangle.v[vertex].alpha = 1;
        }
        frames[i] = r3d_mesh_create(&model);
    }
    for (unsigned i = 0; i < 12; i++) {
        nd_begin();
        nv_target(state.target, state.target_depth);
        nd_clear(0, 0, 0, 1, true, true);
        if (frames[i % 3])
            r3d_draw(frames[i % 3], &view);
        snapshots[i] = nd_image_create(16, 16);
        nd_copy(state.target, snapshots[i], 0, 0, 16, 16);
        /* Keep the loading page visible while advancing the real frame ring. */
        nd_copy(saved.color, nd_screen(), 0, 0, saved.color->width, saved.color->height);
        nd_present();
    }
    bool frames_ok = true;
    for (unsigned i = 0; i < 12; i++) {
        uint8_t wanted[4]{0, 0, 0, 255};
        wanted[i % 3] = 128;
        bool frame_ok = frames[i % 3] && nd_image_read(snapshots[i], pixels) &&
                        nd_rgba_equal(wanted, pixels + (8 * 16 + 8) * 4, 4, 1);
        if (!frame_ok)
            std::fprintf(stderr, "Switch deko3d frame %u: rgba=%u,%u,%u,%u expected=%u,%u,%u,%u\n",
                         i, pixels[(8 * 16 + 8) * 4], pixels[(8 * 16 + 8) * 4 + 1],
                         pixels[(8 * 16 + 8) * 4 + 2], pixels[(8 * 16 + 8) * 4 + 3], wanted[0],
                         wanted[1], wanted[2], wanted[3]);
        frames_ok = frame_ok && frames_ok;
        nd_image_destroy(snapshots[i]);
    }
    for (auto *frame_mesh : frames)
        r3d_mesh_destroy(frame_mesh);
    std::fprintf(stderr, "Switch deko3d selftest: frame-ring=%s frames=12 slots=3\n",
                 frames_ok ? "PASS" : "FAIL");
    ok = frames_ok && ok;
    r3d_end_target(nullptr);
    nv_set(saved);
    r3d_mesh_destroy(mesh);
    std::fprintf(stderr, "Switch deko3d selftest: mesh/depth=%s\n", ok ? "PASS" : "FAIL");
    if (!ok)
        return 0;
    checked = true;
    return 1;
}
r3d_stats r3d_frame_stats(void)
{
    return state.stats;
}
static r3d_mesh *create_mesh(const gt4_model *model, const r3d_mesh *shared, bool transient)
{
    if (!model || model->count <= 0)
        return nullptr;
    auto *mesh = new r3d_mesh;
    mesh->skinned = model->skin != nullptr;
    mesh->transient = transient;
    mesh->textures = shared && shared->textures->entries.size() == (size_t)model->ntextures
                         ? shared->textures
                         : std::make_shared<Textures>();
    mesh->textures->entries.resize(model->ntextures);
    mesh->cpu.resize((size_t)model->count * 3);
    for (int i = 0; i < model->count; i++) {
        const auto &t = model->triangles[i];
        Group *group = mesh->groups.empty() ? nullptr : &mesh->groups.back();
        if (!group || group->count >= 384 || group->texture != t.texture ||
            group->blur != t.wheel_blur_texture || group->kind != t.kind || group->lamp != t.lamp ||
            group->blend != t.blend_enabled || group->model != t.source_model ||
            std::memcmp(&group->state, &t.state, sizeof t.state)) {
            mesh->groups.emplace_back();
            group = &mesh->groups.back();
            group->first = i * 3;
            group->texture = t.texture;
            group->blur = t.wheel_blur_texture;
            group->kind = t.kind;
            group->lamp = t.lamp;
            group->blend = t.blend_enabled;
            group->state = t.state;
            group->model = t.source_model;
        }
        group->count += 3;
        for (unsigned j = 0; j < 3; j++) {
            float *v = mesh->cpu[i * 3 + j].values;
            const auto &src = t.v[j];
            copy3(v, src.p);
            copy3(v + 3, src.normal);
            std::memcpy(v + 6, src.uv, 8);
            copy3(v + 8, src.rgb);
            copy3(v + 11, src.ambient);
            copy3(v + 14, src.specular);
            v[17] = src.shine;
            v[18] = src.alpha;
            float len = v[3] * v[3] + v[4] * v[4] + v[5] * v[5];
            if (len < 1e-12f) {
                float a[3], b[3];
                for (int k = 0; k < 3; k++) {
                    a[k] = t.v[1].p[k] - t.v[0].p[k];
                    b[k] = t.v[2].p[k] - t.v[0].p[k];
                }
                v[3] = a[1] * b[2] - a[2] * b[1];
                v[4] = a[2] * b[0] - a[0] * b[2];
                v[5] = a[0] * b[1] - a[1] * b[0];
                if (v[3] * v[3] + v[4] * v[4] + v[5] * v[5] < 1e-12f)
                    v[4] = 1;
            }
            if (model->skin)
                for (unsigned k = 0; k < 4; k++) {
                    v[19 + k] = model->skin[i].v[j].bone[k];
                    v[23 + k] = model->skin[i].v[j].weight[k];
                }
            float sum = 0;
            if (model->skin)
                for (unsigned k = 0; k < 4; k++) {
                    float weight = v[23 + k];
                    unsigned bone = (unsigned)v[19 + k];
                    if (!std::isfinite(weight) || weight < 0 || bone >= 20) {
                        delete mesh;
                        return nullptr;
                    }
                    if (weight > 0)
                        group->bone_mask |= 1u << bone;
                    sum += weight;
                }
            if (sum == 0)
                group->bone_mask |= 0x80000000u;
            group->weight_lo = std::min(group->weight_lo, sum);
            group->weight_hi = std::max(group->weight_hi, sum);
            for (int k = 0; k < 3; k++) {
                group->lo[k] = std::min(group->lo[k], src.p[k]);
                group->hi[k] = std::max(group->hi[k], src.p[k]);
            }
        }
        if (t.texture >= 0)
            upload_texture(*mesh->textures, t.texture, model);
        if (t.wheel_blur_texture > 0)
            upload_texture(*mesh->textures, t.wheel_blur_texture - 1, model);
    }
    mesh->vertices.size = mesh->cpu.size() * sizeof(Vertex);
    if (!transient) {
        std::vector<Vertex> unique;
        std::vector<uint32_t> indices;
        nd_index_vertices(mesh->cpu, unique, indices);
        size_t vertex_bytes = unique.size() * sizeof(Vertex);
        size_t index_bytes = indices.size() * sizeof(uint32_t);
        mesh->vertices = nd_alloc(vertex_bytes + index_bytes,
                                  DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
        std::memcpy(nd_cpu(mesh->vertices), unique.data(), vertex_bytes);
        std::memcpy(static_cast<uint8_t *>(nd_cpu(mesh->vertices)) + vertex_bytes, indices.data(),
                    index_bytes);
        mesh->vertices.size = vertex_bytes;
        mesh->index_address = nd_gpu(mesh->vertices) + vertex_bytes;
        mesh->index_count = indices.size();
    }
    return mesh;
}
r3d_mesh *r3d_mesh_create_shared(const gt4_model *model, const r3d_mesh *shared)
{
    return create_mesh(model, shared, false);
}
r3d_mesh *r3d_mesh_create(const gt4_model *model)
{
    return r3d_mesh_create_shared(model, nullptr);
}
void r3d_mesh_destroy(r3d_mesh *mesh)
{
    if (mesh) {
        nd_release(mesh->vertices);
        delete mesh;
    }
}
int r3d_mesh_update_positions(r3d_mesh *mesh, const float (*positions)[3], unsigned count,
                              float alpha)
{
    if (!mesh || count != mesh->cpu.size())
        return 0;
    for (unsigned i = 0; i < count; i++) {
        copy3(mesh->cpu[i].values, positions[i]);
        mesh->cpu[i].values[18] = alpha;
    }
    for (auto &g : mesh->groups) {
        for (int k = 0; k < 3; k++) {
            g.lo[k] = 1e30f;
            g.hi[k] = -1e30f;
        }
        for (unsigned i = g.first; i < g.first + g.count; i++)
            for (int k = 0; k < 3; k++) {
                g.lo[k] = std::min(g.lo[k], positions[i][k]);
                g.hi[k] = std::max(g.hi[k], positions[i][k]);
            }
    }
    nd_release(mesh->vertices);
    mesh->index_address = 0;
    mesh->index_count = 0;
    mesh->vertices =
        nd_alloc(count * sizeof(Vertex), DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    std::memcpy(nd_cpu(mesh->vertices), mesh->cpu.data(), count * sizeof(Vertex));
    return 1;
}
int r3d_mesh_update_colours(r3d_mesh *mesh, const gt4_model *model)
{
    if (!mesh || !model || mesh->transient || model->skin || model->morph ||
        mesh->cpu.size() != (size_t)model->count * 3)
        return 0;
    for (int i = 0; i < model->count; i++)
        for (int j = 0; j < 3; j++) {
            const gt4_vertex &src = model->triangles[i].v[j];
            Vertex &v = mesh->cpu[(size_t)i * 3 + j];
            copy3(v.values + 8, src.rgb);
            v.values[18] = src.alpha;
        }
    std::vector<Vertex> unique;
    std::vector<uint32_t> indices;
    nd_index_vertices(mesh->cpu, unique, indices);
    size_t vertex_bytes = unique.size() * sizeof(Vertex);
    size_t index_bytes = indices.size() * sizeof(uint32_t);
    nd_release(mesh->vertices);
    mesh->vertices = nd_alloc(vertex_bytes + index_bytes,
                              DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    std::memcpy(nd_cpu(mesh->vertices), unique.data(), vertex_bytes);
    std::memcpy(static_cast<uint8_t *>(nd_cpu(mesh->vertices)) + vertex_bytes, indices.data(),
                index_bytes);
    mesh->vertices.size = vertex_bytes;
    mesh->index_address = nd_gpu(mesh->vertices) + vertex_bytes;
    mesh->index_count = indices.size();
    return 1;
}

void r3d_environment_changed(void)
{
    state.environment_dirty = true;
}
void r3d_begin(float x, float y, float w, float h, const tex1_image *environment)
{
    video_flush2d();
    state.stats = {};
    state.borrowed_environment = nullptr;
    state.projection = video_projection_x();
    auto current = nv_get();
    if (!current.depth) {
        if (!state.ui_depth || state.ui_depth->width != current.color->width ||
            state.ui_depth->height != current.color->height) {
            nd_image_destroy(state.ui_depth);
            state.ui_depth = nd_image_create(current.color->width, current.color->height, true);
        }
        current.depth = state.ui_depth;
        nv_set(current);
    }
    int vx, vy, vw, vh;
    video_viewport(x, y, w, h, &vx, &vy, &vw, &vh);
    nv_view(vx, current.color->height - vy - vh, vw, vh);
    nd_scissor(std::max(0, vx), std::max(0, (int)current.color->height - vy - vh), vw, vh);
    nd_begin_3d_commands(nd_commands());
    if (state.environment_source != environment || state.environment_dirty) {
        nd_image_destroy(state.environment);
        state.environment = nullptr;
        if (environment && environment->rgba) {
            state.environment = nd_image_create(environment->width, environment->height);
            nd_image_upload(state.environment, environment->rgba);
        }
        state.environment_source = environment;
        state.environment_dirty = false;
    }
}
void r3d_begin_ui_environment(float x, float y, float w, float h, const tex1_image *environment)
{
    auto old = nv_get();
    r3d_begin(x, y, w, h, environment);
    state.projection = 1;
    nd_scissor(old.clip[0], old.clip[1], old.clip[2], old.clip[3]);
}
void r3d_begin_ui(float x, float y, float w, float h)
{
    r3d_begin_ui_environment(x, y, w, h, nullptr);
}
void r3d_clear_depth(void)
{
    nd_clear(0, 0, 0, 1, false, true);
}
void r3d_end(void)
{
    auto current = nv_get();
    nd_viewport(0, 0, current.color->width, current.color->height);
    nd_scissor(current.clip[0], current.clip[1], current.clip[2], current.clip[3]);
}
void r3d_draw_lit(r3d_mesh *mesh, const r3d_view *view, const r3d_lights *lights)
{
    if (!mesh || !view)
        return;
    SWP_PUSH(SWP_GL);
    nd_uniform uniform{};
    for (unsigned i = 0; i < 3; i++)
        copy3(uniform.v[i], view->rot + i * 3);
    copy3(uniform.v[3], view->pos);
    copy3(uniform.v[4], view->cam);
    copy3(uniform.v[5], view->r);
    copy3(uniform.v[6], view->u);
    copy3(uniform.v[7], view->f);
    uniform.v[7][3] = view->mirror;
    uniform.v[8][0] = view->focal / 320 * state.projection;
    uniform.v[8][1] = (view->focal_y > 0 ? view->focal_y : view->focal) / 224;
    /* Same near/far bias as render3d.c: the authored shadow pass offsets
       its projection instead of changing the car's geometry or physics. */
    uniform.v[8][2] = (view->near_clip > 0 ? view->near_clip : .2f) + view->depth_bias;
    uniform.v[8][3] = 20000 + view->depth_bias;
    uniform.v[9][0] = view->shadow ? 2 : view->unlit ? 1 : 0;
    uniform.v[9][1] = view->color_scale > 0 ? view->color_scale : 1;
    uniform.v[10][1] = view->unlit ? 128.f / 255 : 1;
    unsigned n = lights ? std::min(4, std::max(0, lights->count)) : 0;
    uniform.v[10][3] = n;
    uniform.v[10][2] = lights && lights->specular_scale > 0 ? lights->specular_scale : 1;
    uniform.v[13][0] = view->ghost == 2 ? view->ghost_alpha : 1;
    uniform.v[14][0] = mesh->skinned && view->skin_palette;
    uniform.v[14][3] = n ? lights->vertex_limit : 0;
    if (view->skin_palette)
        std::memcpy(uniform.bones, view->skin_palette, sizeof uniform.bones);
    std::memcpy(uniform.v[15], view->jitter, 8);
    copy3(uniform.v[16], view->shadow_rgb);
    uniform.v[16][3] = view->shadow_alpha > 0 ? view->shadow_alpha : .55f;
    copy3(uniform.v[17], view->color_mul);
    uniform.v[17][3] = view->authored_color;
    copy3(uniform.v[18], view->color_add);
    uniform.v[19][0] = view->env_color[0];
    uniform.v[19][1] = view->env_color[1];
    if (!view->env_color[0] && !view->env_color[1])
        uniform.v[19][0] = 1;
    uniform.v[20][0] = view->fog_distance;
    uniform.v[20][1] = view->fog_max;
    uniform.v[20][3] = view->fog;
    copy3(uniform.v[21], view->fog_rgb);
    copy3(uniform.v[22], view->grade);
    uniform.v[22][3] = view->grade[3];
    uniform.v[23][0] = view->grade[4];
    uniform.v[23][3] = view->grade[5];
    for (unsigned i = 0; i < n; i++) {
        copy3(uniform.v[24 + i], lights->direction[i]);
        copy3(uniform.v[28 + i], lights->ambient[i]);
        copy3(uniform.v[32 + i], lights->diffuse[i]);
        copy3(uniform.v[36 + i], lights->specular[i]);
    }
    float planes[6][4];
    r3d_frustum(view, state.projection, planes);
    DkGpuAddr streamed =
        mesh->transient ? nd_stream(mesh->cpu.data(), mesh->cpu.size() * sizeof(Vertex), 16) : 0;
    for (const Group &g : mesh->groups) {
        if (g.lamp && (g.lamp == 2) != (view->brake != 0))
            continue;
        if (view->model_list) {
            unsigned count = view->model_list[0] | view->model_list[1] << 8;
            bool found = false;
            for (unsigned i = 0; i < count; i++)
                if ((unsigned)(view->model_list[2 + i * 2] | view->model_list[3 + i * 2] << 8) ==
                    g.model) {
                    found = true;
                    break;
                }
            if (!found)
                continue;
        }
        float posed_lo[3], posed_hi[3];
        const float *bound_lo = g.lo, *bound_hi = g.hi;
        if (mesh->skinned && view->skin_palette) {
            r3d_skin_bounds(g.lo, g.hi, g.bone_mask, g.weight_lo, g.weight_hi, view->skin_palette,
                            posed_lo, posed_hi);
            bound_lo = posed_lo;
            bound_hi = posed_hi;
        }
        if (!r3d_bounds_visible(planes, bound_lo, bound_hi)) {
            state.stats.culled_groups++;
            continue;
        }
        if (view->ghost == 1 && g.blend)
            continue;
        int index = view->wheel_blur == 1 && g.blur ? g.blur - 1 : g.texture;
        const Texture *texture = index >= 0 && (size_t)index < mesh->textures->entries.size()
                                     ? &mesh->textures->entries[index]
                                     : nullptr;
        unsigned images[4]{texture && texture->image ? texture->image->slot : nv_white(),
                           nv_black(), nv_white(), nv_white()};
        nd_image *env = state.borrowed_environment ? state.borrowed_environment : state.environment;
        if (env)
            images[1] = env->slot;
        uniform.v[9][2] = (g.kind == 0 || g.kind == 5 || view->unlit || view->shadow) ? 1 : 0;
        uniform.v[9][3] = g.texture == -2;
        uniform.v[10][0] = texture && texture->image;
        uniform.v[11][0] = texture ? texture->cc : 0;
        uniform.v[11][1] = texture ? texture->native : 0;
        uniform.v[11][2] = texture ? texture->function : 0;
        uniform.v[12][0] = g.state.alpha_test;
        uniform.v[12][1] = g.state.alpha_func;
        uniform.v[12][2] = g.state.alpha_ref;
        uniform.v[14][1] = g.kind == 4 && !view->shadow;
        if (texture && texture->image) {
            nd_texture_extent(&uniform, texture->tex_w, texture->tex_h, texture->image->width,
                              texture->image->height);
            uniform.v[44][0] = texture->wrap_u;
            uniform.v[44][1] = texture->wrap_v;
            uniform.v[44][2] = texture->nearest;
            for (unsigned k = 0; k < 4; k++)
                uniform.v[45][k] = texture->region[k];
        }
        uniform.v[46][0] = g.state.destination_test;
        uniform.v[46][1] = g.state.destination_bit;
        uniform.v[46][2] = 1;
        if (g.state.destination_test) {
            /* 00455708 commands 29/31: DATE tests the prior framebuffer's
               alpha, not this fragment. A GPU-only snapshot avoids feedback
               while retaining authored draw order; never CPU readback. */
            auto target = nv_get();
            if (!state.destination || state.destination->width != target.color->width ||
                state.destination->height != target.color->height) {
                nd_image_destroy(state.destination);
                state.destination =
                    nd_image_create(target.color->width, target.color->height, false, false, true);
            }
            SWP_PUSH(SWP_GPU_DATE);
            nd_snapshot_rect rect = {0, 0, target.color->width, target.color->height};
            if (!mesh->transient && !mesh->skinned)
                rect = nd_snapshot_bounds(uniform.v, bound_lo, bound_hi, target.viewport,
                                          target.color->width, target.color->height);
            nv_snapshot(target.color, state.destination, rect.x, rect.y, rect.width, rect.height);
            SWP_POP();
            images[2] = state.destination->slot;
            uniform.v[46][3] = 1; /* Scalar snapshot stores the original alpha in R. */
        }
        if (g.state.fog_override)
            for (unsigned k = 0; k < 3; k++)
                uniform.v[21][k] = g.state.fog_rgb[k] / 255.f *
                                   (view->unlit && view->color_scale > 0 ? view->color_scale : 1);
        else
            copy3(uniform.v[21], view->fog_rgb);
        DkRasterizerState raster;
        dkRasterizerStateDefaults(&raster);
        raster.cullMode = g.state.cull == 0   ? DkFace_None
                          : g.state.cull == 1 ? DkFace_Back
                                              : DkFace_Front;
        raster.frontFace =
            (view->mirror != 0) != (view->screen_reflect != 0) ? DkFrontFace_CW : DkFrontFace_CCW;
        DkDepthStencilState depth;
        dkDepthStencilStateDefaults(&depth);
        depth.depthTestEnable = true;
        depth.depthWriteEnable = g.state.depth_write && view->ghost != 2;
        depth.depthCompareOp = !g.state.depth_test       ? DkCompareOp_Always
                               : g.state.depth_func == 0 ? DkCompareOp_Never
                               : g.state.depth_func == 1 ? DkCompareOp_Always
                               : g.state.depth_func == 2 ? DkCompareOp_Less
                                                         : DkCompareOp_Lequal;
        DkColorState color;
        dkColorStateDefaults(&color);
        DkBlendState blend;
        dkBlendStateDefaults(&blend);
        blend_state(g, color, blend);
        if (view->ghost == 2) {
            color.blendEnableMask = 1;
            blend.srcColorBlendFactor = DkBlendFactor_ConstAlpha;
            blend.dstColorBlendFactor = DkBlendFactor_InvConstAlpha;
        }
        DkColorWriteState mask;
        dkColorWriteStateDefaults(&mask);
        unsigned channels = 0;
        for (unsigned i = 0; i < 4; i++)
            if (((g.state.color_mask >> (i * 8)) & 255) != 255)
                channels |= 1u << i;
        dkColorWriteStateSetMask(&mask, 0, view->ghost == 1 ? 0 : channels);
        nd_pipeline(
            ND_MESH, uniform, mesh->vertices, g.first, g.count, images, raster, depth, color, mask,
            blend,
            view->ghost == 2 ? view->ghost_alpha : std::min(1.f, g.state.fixed_alpha / 128.f),
            texture ? texture->sampler : 0, streamed, 0, mesh->index_address, mesh->index_count);
        state.stats.draws++;
        state.stats.triangles += g.count / 3;
        if (!view->ghost && g.state.alpha_test && g.state.alpha_fail) {
            /* AFAIL FB_ONLY/ZB_ONLY/RGB_ONLY: same actions as set_masks and
               the second pass in render3d.c, decoded from 00455708 TEST. */
            bool rgb = g.state.alpha_fail != 2, alpha = g.state.alpha_fail == 1;
            dkColorWriteStateSetMask(&mask, 0, channels & ((rgb ? 7u : 0u) | (alpha ? 8u : 0u)));
            depth.depthWriteEnable = g.state.depth_write && g.state.alpha_fail == 2;
            uniform.v[46][2] = 2;
            nd_pipeline(ND_MESH, uniform, mesh->vertices, g.first, g.count, images, raster, depth,
                        color, mask, blend, std::min(1.f, g.state.fixed_alpha / 128.f),
                        texture ? texture->sampler : 0, streamed, 0, mesh->index_address,
                        mesh->index_count);
            state.stats.draws++;
            state.stats.triangles += g.count / 3;
        }
        state.stats.state_updates++;
        state.stats.texture_updates++;
    }
    SWP_POP();
}
/* Renderer-owned targets outlive individual scenes, but must be retired before
   dkDeviceDestroy. borrowed_environment and saved targets belong to callers. */
void nd_render_shutdown()
{
    for (auto *image : {state.environment, state.ui_depth, state.destination, state.target,
                        state.target_depth, state.layer, state.layer_depth})
        nd_image_destroy(image);
    state = Context{};
}

void r3d_draw(r3d_mesh *mesh, const r3d_view *view)
{
    r3d_draw_lit(mesh, view, nullptr);
}
void r3d_draw_ui(r3d_mesh *mesh, const r3d_view *view, const r3d_ui_light *light)
{
    r3d_lights lights{};
    lights.count = 1;
    copy3(lights.direction[0], light->direction);
    copy3(lights.ambient[0], light->ambient);
    copy3(lights.diffuse[0], light->diffuse);
    copy3(lights.specular[0], light->specular);
    r3d_draw_lit(mesh, view, &lights);
}
int r3d_begin_target(int size)
{
    if (size <= 0 || state.in_target)
        return 0;
    state.target_saved = nv_get();
    state.target_projection = state.projection;
    state.projection = 1;
    if (!state.target || state.target->width != (unsigned)size) {
        nd_image_destroy(state.target);
        nd_image_destroy(state.target_depth);
        state.target = nd_image_create(size, size);
        state.target_depth = nd_image_create(size, size, true);
    }
    nv_target(state.target, state.target_depth);
    nd_clear(0, 0, 0, 1, true, true);
    state.in_target = true;
    return 1;
}
int r3d_begin_target_view(int size)
{
    return r3d_begin_target(size);
}
void r3d_end_target(uint8_t *rgba)
{
    if (!state.in_target)
        return;
    if (rgba)
        nd_image_read(state.target, rgba);
    nv_set(state.target_saved);
    state.projection = state.target_projection;
    state.in_target = false;
}
int r3d_begin_layer(int copy)
{
    if (state.in_layer)
        return 0;
    state.layer_saved = nv_get();
    unsigned w = state.layer_saved.color->width, h = state.layer_saved.color->height;
    if (!state.layer || state.layer->width != w || state.layer->height != h) {
        nd_image_destroy(state.layer);
        nd_image_destroy(state.layer_depth);
        state.layer = nd_image_create(w, h);
        state.layer_depth = nd_image_create(w, h, true);
    }
    nv_target(state.layer, state.layer_depth);
    nd_clear(0, 0, 0, 1, true, true);
    if (copy)
        nd_copy(state.layer_saved.color, state.layer, 0, 0, w, h);
    state.in_layer = true;
    return 1;
}
void r3d_end_layer(float opacity, int)
{
    if (!state.in_layer)
        return;
    video_flush2d();
    nv_set(state.layer_saved);
    plat_texture texture{state.layer, false, false};
    const uint8_t white[4]{255, 255, 255, 255};
    plat_draw_quad_blend(&texture, 0, 0, 640, video_view_height(), 0, 0, 1, 1, white, opacity);
    video_flush2d();
    state.in_layer = false;
}
void r3d_end_layer_sample(float opacity, int additive, int, int)
{
    r3d_end_layer(opacity, additive);
}
void r3d_clear_alpha(float alpha)
{
    const float value[4]{0, 0, 0, alpha};
    nd_clear_color_commands(nd_commands(), DkColorMask_A, value);
}
void r3d_end_target_add(const float rgb[3], int)
{
    if (!state.in_target)
        return;
    r3d_end_target(nullptr);
    plat_texture texture{state.target, false, false};
    uint8_t tint[4]{(uint8_t)(std::min(1.f, rgb[0]) * 255), (uint8_t)(std::min(1.f, rgb[1]) * 255),
                    (uint8_t)(std::min(1.f, rgb[2]) * 255), 128};
    plat_draw_quad(&texture, 0, 0, 640, video_view_height(), 0, 0, 1, 1, tint);
    video_flush2d();
}
/* P: use the authored contact-shadow mesh; no extra per-frame silhouette blur. */
int r3d_floor_shadow(r3d_mesh *, const r3d_view *, const float[3], float, float)
{
    return 0;
}
void r3d_gpu_environment(unsigned texture, int, int)
{
    state.borrowed_environment = nd_image_slot(texture);
}
void r3d_copy_target(unsigned destination, int x)
{
    nd_image *image = nd_image_slot(destination);
    if (!image || !state.in_target)
        return;
    DkImageRect a{0, 0, 0, state.target->width, state.target->height, 1},
        b{(unsigned)x, 0, 0, state.target->width, state.target->height, 1};
    nd_copy_commands(nd_commands(), &state.target->view, &a, &image->view, &b);
    r3d_end_target(nullptr);
}
r3d_sprite_texture *r3d_sprite_texture_create(const tex1_image *image)
{
    if (!image || !image->rgba)
        return nullptr;
    auto *result = new r3d_sprite_texture{nd_image_create(image->width, image->height),
                                          !image->magnify && !image->minify};
    if (image->native_alpha) {
        std::vector<uint8_t> rgba(image->rgba,
                                  image->rgba + (size_t)image->width * image->height * 4);
        for (int i = 0; i < image->width * image->height; i++)
            rgba[i * 4 + 3] = image->native_alpha[i];
        nd_image_upload(result->image, rgba.data());
    } else
        nd_image_upload(result->image, image->rgba);
    return result;
}
void r3d_sprite_texture_destroy(r3d_sprite_texture *texture)
{
    if (texture) {
        nd_image_destroy(texture->image);
        delete texture;
    }
}
void r3d_draw_sprites(const r3d_sprite_texture *texture, const r3d_view *view, const float *corners,
                      int squares, int mode)
{
    if (!texture || !view || !corners || squares <= 0)
        return;
    gt4_model model{};
    tex1_image image{};
    model.ntextures = 1;
    model.textures = &image;
    std::vector<gt4_triangle> tris(squares * 2);
    model.triangles = tris.data();
    model.count = squares * 2;
    const unsigned order[6]{0, 1, 2, 1, 3, 2};
    for (int i = 0; i < squares; i++)
        for (unsigned j = 0; j < 6; j++) {
            auto &t = tris[i * 2 + j / 3];
            t.texture = 0;
            t.kind = 0;
            t.blend_enabled = mode != 2;
            t.state.color_mask = 0xff000000u;
            t.state.alpha_test = mode == 2;
            t.state.alpha_func = 6; /* Desktop particle test: texel alpha > 0. */
            t.state.blend = mode == 1 ? 0x48 : 0x44;
            t.state.depth_test = 1;
            t.state.depth_func = 3;
            auto &v = t.v[j % 3];
            const float *c = corners + (i * 4 + order[j]) * 9;
            for (unsigned k = 0; k < 3; k++) {
                v.p[k] = view->cam[k] + view->r[k] * c[0] + view->u[k] * c[1] - view->f[k] * c[2];
                v.rgb[k] = c[k + 5];
            }
            std::memcpy(v.uv, c + 3, 8);
            v.alpha = c[8];
            v.normal[1] = 1;
        }
    /* Dynamic smoke/leaf geometry uses fence-owned frame streaming memory. */
    r3d_mesh *mesh = create_mesh(&model, nullptr, true);
    mesh->textures->entries[0].image = texture->image;
    auto &material = mesh->textures->entries[0];
    material.cc = material.native = 1;
    material.tex_w = texture->image->width;
    material.tex_h = texture->image->height;
    material.wrap_u = material.wrap_v = 1;
    material.nearest = texture->nearest;
    r3d_view flat = *view;
    flat.rot[0] = flat.rot[4] = flat.rot[8] = 1;
    flat.rot[1] = flat.rot[2] = flat.rot[3] = flat.rot[5] = flat.rot[6] = flat.rot[7] = 0;
    std::memset(flat.pos, 0, sizeof flat.pos);
    flat.unlit = 1;
    flat.color_scale = 1;
    flat.fog = 0;
    flat.authored_color = 0;
    flat.grade[5] = 0;
    r3d_draw(mesh, &flat);
    mesh->textures->entries[0].image = nullptr;
    r3d_mesh_destroy(mesh);
}
int switch_gpu_warmup(void)
{
    return r3d_available();
}
switch_gpu_image *switch_gpu_mix_create(int width, int height, const unsigned char *const layers[4])
{
    auto *image = new switch_gpu_image;
    image->texture = {nd_image_create(width, height), false, false};
    for (unsigned i = 0; i < 4; i++) {
        image->layers[i] = nd_image_create(width, height);
        nd_image_upload(image->layers[i], layers[i]);
    }
    return image;
}
plat_texture *switch_gpu_image_texture(const switch_gpu_image *image)
{
    return image ? const_cast<plat_texture *>(&image->texture) : nullptr;
}
void switch_gpu_image_destroy(switch_gpu_image *image)
{
    if (image) {
        nd_image_destroy(image->texture.image);
        nd_image_destroy(image->atlas);
        for (auto layer : image->layers)
            nd_image_destroy(layer);
        delete image;
    }
}
void switch_gpu_mix(switch_gpu_image *image, const float weights[4])
{
    if (image)
        nv_draw_mix(image->texture.image, image->layers, weights);
}
/* Same six-view/sphere-map boundary as the PC backend, entirely GPU-resident. */
switch_gpu_image *switch_gpu_env_create(int face_size, int map_size)
{
    if (face_size <= 0 || map_size <= 0)
        return nullptr;
    auto *image = new switch_gpu_image;
    image->texture = {nd_image_create(map_size, map_size), false, false};
    image->atlas = nd_image_create(face_size * 6, face_size);
    image->face_size = face_size;
    return image;
}
void switch_gpu_env_capture(switch_gpu_image *image, int face)
{
    if (image && face >= 0 && face < 6)
        r3d_copy_target(image->atlas->slot, face * image->face_size);
}
void switch_gpu_env_remap(switch_gpu_image *image, const float r[3], const float u[3],
                          const float f[3])
{
    if (image)
        nv_draw_environment(image->texture.image, image->atlas, image->face_size, r, u, f);
}
}
