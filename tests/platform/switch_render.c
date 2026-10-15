/* Compare Switch GPU work with the port's existing CPU formulas, not PS2 pixels. */
#include "../../src/platform/sdl/render3d.c"
#include "switch_render.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Switch GPU line %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum { FACE = 8, MAP = 32 };
static unsigned char faces[6][FACE * FACE * 4];
static unsigned char reference[MAP * MAP * 4], pixels[MAP * MAP * 4];
static int read_image(switch_gpu_image *s)
{
    GLuint fb;
    GLuint restore = video_draw_framebuffer();
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           video_texture_id(switch_gpu_image_texture(s)), 0);
    glReadPixels(0, 0, MAP, MAP, GL_RGBA, GL_UNSIGNED_BYTE, pixels); /* Oracle only. */
    glBindFramebuffer(GL_FRAMEBUFFER, restore);
    glDeleteFramebuffers(1, &fb);
    return glGetError() == GL_NO_ERROR;
}
static int compare(int negative)
{
    if (negative)
        pixels[0] ^= 128;
    int worst = 0;
    for (unsigned i = 0; i < sizeof pixels; i++) {
        int error = abs((int)pixels[i] - reference[i]);
        if (error > worst)
            worst = error;
    }
    if (worst > 1)
        fprintf(stderr, "maximum channel difference: %d\n", worst);
    return worst <= 1;
}
static void cpu_environment(const float r[3], const float u[3], const float f[3])
{
    static const float axes[6][2][3] = {{{1, 0, 0}, {0, 1, 0}},  {{-1, 0, 0}, {0, 1, 0}},
                                        {{0, 0, 1}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}},
                                        {{0, 1, 0}, {0, 0, -1}}, {{0, -1, 0}, {0, 0, 1}}};
    for (int y = 0; y < MAP; y++)
        for (int x = 0; x < MAP; x++) {
            float mx = ((x + .5f) / MAP - .5f) * 2, my = -((y + .5f) / MAP - .5f) * 2,
                  m2 = mx * mx + my * my;
            if (m2 > 1) {
                float k = 1 / sqrtf(m2);
                mx *= k;
                my *= k;
                m2 = 1;
            }
            float nz = sqrtf(1 - m2), R[3] = {2 * nz * mx, 2 * nz * my, 2 * nz * nz - 1}, D[3];
            for (int k = 0; k < 3; k++)
                D[k] = R[0] * r[k] + R[1] * u[k] - R[2] * f[k];
            int best = 0;
            float big = -1;
            for (int j = 0; j < 6; j++) {
                const float *ff = axes[j][0];
                float d = D[0] * ff[0] + D[1] * ff[1] + D[2] * ff[2];
                if (d > big) {
                    big = d;
                    best = j;
                }
            }
            const float *ff = axes[best][0], *uu = axes[best][1];
            float rr[3] = {ff[1] * uu[2] - ff[2] * uu[1], ff[2] * uu[0] - ff[0] * uu[2],
                           ff[0] * uu[1] - ff[1] * uu[0]};
            float qx = (D[0] * rr[0] + D[1] * rr[1] + D[2] * rr[2]) / big,
                  qy = (D[0] * uu[0] + D[1] * uu[1] + D[2] * uu[2]) / big;
            float fx = fminf(FACE - 1, fmaxf(0, FACE / 2 + qx * FACE / 2 - .5f)),
                  fy = fminf(FACE - 1, fmaxf(0, FACE / 2 - qy * FACE / 2 - .5f));
            int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < FACE ? x0 + 1 : x0,
                y1 = y0 + 1 < FACE ? y0 + 1 : y0;
            float wx = fx - x0, wy = fy - y0;
            for (int c = 0; c < 3; c++) {
                const unsigned char *p = faces[best];
                float a = p[(y0 * FACE + x0) * 4 + c] * (1 - wx) + p[(y0 * FACE + x1) * 4 + c] * wx;
                float b = p[(y1 * FACE + x0) * 4 + c] * (1 - wx) + p[(y1 * FACE + x1) * 4 + c] * wx;
                reference[(y * MAP + x) * 4 + c] = (unsigned char)(a * (1 - wy) + b * wy + .5f);
            }
            reference[(y * MAP + x) * 4 + 3] = 255;
        }
}
static void texture_cache(int uncached)
{
#ifdef _WIN32
    _putenv_s("OPENGT4_SWITCH_UNCACHED", uncached ? "1" : "");
#else
    if (uncached)
        setenv("OPENGT4_SWITCH_UNCACHED", "1", 1);
    else
        unsetenv("OPENGT4_SWITCH_UNCACHED");
#endif
}

static void shader_reference(int enabled)
{
#ifdef _WIN32
    _putenv_s("OPENGT4_SWITCH_SHADER_REFERENCE", enabled ? "1" : "");
#else
    if (enabled)
        setenv("OPENGT4_SWITCH_SHADER_REFERENCE", "1", 1);
    else
        unsetenv("OPENGT4_SWITCH_SHADER_REFERENCE");
#endif
}

/* Compare optimized shader work against its original branches, including
   authored lighting, reflection, native mip/region addressing and final colour.
   Hardware bilinear rounding is allowed one byte per output channel. */
static int shader_case(int negative)
{
    uint8_t rgba[16 * 16 * 4], mip[8 * 8 * 4], environment[8 * 8 * 4];
    for (unsigned i = 0; i < sizeof rgba; i++)
        rgba[i] = i % 4 == 3 ? 128 : (uint8_t)(i * 17 + i / 64);
    for (unsigned i = 0; i < sizeof mip; i++) {
        mip[i] = i % 4 == 3 ? 128 : (uint8_t)(i * 13 + i / 32);
        environment[i] = i % 4 == 3 ? 255 : (uint8_t)(i * 7 + i / 32);
    }
    tex1_image env = {0};
    env.width = env.height = env.tex_w = env.tex_h = 8;
    env.rgba = environment;
    unsigned cases = 0, colored = 0;
    int worst = 0;
    for (int material = 0; material < 6; material++)
        for (int sample = 0; sample < 10; sample++)
            for (int lighting = 0; lighting < 4; lighting++) {
                tex1_image texture = {0};
                texture.width = texture.height = texture.tex_w = texture.tex_h = 16;
                texture.rgba = rgba;
                texture.magnify = sample != 6;
                texture.wrap_u = texture.wrap_v = sample == 2   ? 1
                                                  : sample == 3 ? 2
                                                  : sample == 4 ? 3
                                                                : 0;
                texture.min_u = texture.min_v = sample == 4 ? 8 : 2;
                texture.max_u = texture.max_v = sample == 4 ? 7 : 12;
                if (sample == 5) {
                    texture.mip_count = 1;
                    texture.minify = 5;
                    texture.mip_rgba[0] = mip;
                }
                gt4_triangle triangles[2] = {0};
                const float positions[4][3] = {{-3, 1, 8}, {3, 1, 8}, {3, 5, 8}, {-3, 5, 8}};
                const int indices[2][3] = {{0, 1, 2}, {0, 2, 3}};
                const unsigned kinds[6] = {0, 1, 4, 5, 1, 0};
                for (int t = 0; t < 2; t++) {
                    triangles[t].kind = kinds[material];
                    triangles[t].texture = material >= 4 ? -2 : sample == 0 ? -1 : 0;
                    for (int j = 0; j < 3; j++) {
                        int at = indices[t][j];
                        gt4_vertex *v = &triangles[t].v[j];
                        memcpy(v->p, positions[at], sizeof v->p);
                        v->normal[0] = .3f + at * .1f;
                        v->normal[1] = .6f;
                        v->normal[2] = -.7f;
                        v->alpha = 1;
                        v->shine = lighting == 3 ? 16 : lighting == 2 ? 8 : 4;
                        v->uv[0] = (positions[at][0] + 3) * (sample == 5 ? 4 : .3f) - .2f;
                        v->uv[1] = (positions[at][1] - 1) * .4f - .3f;
                        for (int c = 0; c < 3; c++) {
                            v->rgb[c] = .25f + c * .1f;
                            v->ambient[c] = .15f + c * .05f;
                            v->specular[c] = .2f;
                        }
                    }
                }
                gt4_model model = {0};
                model.triangles = triangles;
                model.count = 2;
                model.textures = &texture;
                model.ntextures = 1;
                r3d_mesh *mesh = r3d_mesh_create(&model);
                CHECK(mesh);
                r3d_view view = {0};
                view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
                view.focal = 320;
                view.focal_y = 224;
                view.cam[1] = sample == 9 ? -3 : 3;
                view.mirror = sample == 9;
                view.unlit = sample == 7;
                view.shadow = sample == 8;
                view.shadow_alpha = .6f;
                view.env_color[0] = .6f;
                view.env_color[1] = lighting & 1 ? -.2f : 0;
                view.fog = sample & 1;
                view.fog_distance = 4;
                view.fog_max = .5f;
                view.fog_rgb[0] = .1f;
                view.fog_rgb[1] = .2f;
                view.authored_color = sample % 3 == 0;
                view.color_mul[0] = view.color_mul[1] = view.color_mul[2] = .8f;
                view.color_add[0] = .05f;
                view.grade[0] = view.grade[1] = view.grade[2] = 1;
                view.grade[3] = .7f;
                view.grade[4] = .02f;
                view.grade[5] = .3f;
                r3d_lights lights = {0};
                lights.count = lighting == 0 ? 0 : lighting == 1 ? 1 : 3;
                lights.vertex_limit = lighting == 3 ? 1 : 0;
                for (int l = 0; l < lights.count; l++) {
                    lights.direction[l][0] = .3f + l * .1f;
                    lights.direction[l][1] = .6f;
                    lights.direction[l][2] = -.7f;
                    for (int c = 0; c < 3; c++) {
                        lights.ambient[l][c] = .1f;
                        lights.diffuse[l][c] = .2f + c * .05f;
                        lights.specular[l][c] = .1f;
                    }
                }
                uint8_t images[2][64 * 64 * 4];
                for (int ref = 0; ref < 2; ref++) {
                    shader_reference(ref);
                    r3d_begin(0, 0, 640, 448, &env);
                    CHECK(r3d_begin_target(64));
                    r3d_draw_lit(mesh, &view, &lights);
                    r3d_end_target(images[ref]);
                }
                if (negative)
                    images[0][0] ^= 128;
                for (unsigned i = 0; i < sizeof images[0]; i++) {
                    int error = abs((int)images[0][i] - images[1][i]);
                    if (error > worst)
                        worst = error;
                    if (error > 1) {
                        fprintf(stderr, "shader material=%d sample=%d lights=%d byte=%u error=%d\n",
                                material, sample, lighting, i, error);
                        return 1;
                    }
                    if (i % 4 != 3 && images[0][i])
                        colored++;
                }
                r3d_mesh_destroy(mesh);
                cases++;
            }
    shader_reference(0);
    /* r3d_begin retains the environment pointer: invalidate before locals die. */
    r3d_environment_changed();
    r3d_begin(0, 0, 640, 448, NULL);
    CHECK(cases==240 && colored>10000 && glGetError()==GL_NO_ERROR);
    printf("Switch shader comparison: %u cases, maximum channel difference %d\n", cases, worst);
    return 0;
}

static int cache_case(int negative)
{
    uint8_t rgba[2][16 * 16 * 4], mip[8 * 8 * 4];
    tex1_image textures[2] = {0};
    for (int j = 0; j < 2; j++) {
        for (unsigned i = 0; i < sizeof rgba[j]; i++)
            rgba[j][i] = i % 4 == 3 ? 255 : (uint8_t)(j * 73 + i % 4 * 47);
        textures[j].width = textures[j].height = textures[j].tex_w = textures[j].tex_h = 16;
        textures[j].rgba = rgba[j];
    }
    memset(mip, 151, sizeof mip);
    textures[0].mip_count = 1;
    textures[0].minify = 5;
    textures[0].mip_rgba[0] = mip;
    gt4_triangle triangles[12] = {0};
    for (int i = 0; i < 12; i++) {
        gt4_triangle *t = triangles + i;
        t->texture = i < 4 ? 0 : i < 8 ? 1 : -1;
        t->state.alpha_ref = (uint8_t)i; /* Keep separate state groups. */
        t->state.destination_test = i == 5;
        t->state.destination_bit = 1;
        float x = -3.7f + (i % 4) * 2, y = -2.7f + (i / 4) * 2;
        const float points[3][3] = {{x, y, 8}, {x + 1.4f, y, 8}, {x + .7f, y + 1.4f, 8}};
        for (int v = 0; v < 3; v++) {
            memcpy(t->v[v].p, points[v], sizeof points[v]);
            t->v[v].uv[0] = v == 1;
            t->v[v].uv[1] = v == 2;
            t->v[v].alpha = 1;
            t->v[v].rgb[0] = t->v[v].rgb[1] = t->v[v].rgb[2] = 1;
        }
    }
    triangles[0].wheel_blur_texture = 2;
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 12;
    model.ntextures = 2;
    model.textures = textures;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    CHECK(mesh && mesh->ngroups==12);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, mesh->textures[i].id);
        GLint filter, level;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
        CHECK(filter==GL_LINEAR);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &level);
        CHECK(level==(i ? 0 : 8)); /* Native level retained, generated level absent. */
        if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
            GLfloat anisotropy;
            glGetTexParameterfv(GL_TEXTURE_2D, 0x84FE, &anisotropy);
            CHECK(anisotropy==1);
        }
    }
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    view.wheel_interval = .1f;
    uint8_t images[2][64 * 64 * 4];
    for (int frame = 0; frame < 3; frame++) {
        view.wheel_blur = frame == 1 ? 2 : frame == 2 ? 1 : 0;
        for (int uncached = 0; uncached < 2; uncached++) {
            texture_cache(uncached);
            r3d_begin(0, 0, 640, 448, NULL);
            CHECK(r3d_begin_target(64));
            r3d_draw(mesh, &view);
            r3d_end_target(images[uncached]);
            r3d_stats stats = r3d_frame_stats();
            unsigned updates = uncached ? 12 : frame ? 4 : 3;
            CHECK(stats.draws==12 && stats.texture_updates==updates);
            CHECK(stats.texture_cache_hits==12-updates);
        }
        if (negative)
            images[0][0] ^= 1;
        CHECK(!memcmp(images[0],images[1],sizeof images[0]));
    }
    texture_cache(0);
    r3d_mesh_destroy(mesh);
    return 0;
}

/* State is reusable across different textures but not alpha-fail/ghost
   side effects. DATE must still snapshot freshly before every group. */
static int state_case(int negative)
{
    uint8_t rgba[2][4] = {{180, 20, 70, 255}, {20, 150, 90, 255}};
    tex1_image textures[2] = {0};
    for (int i = 0; i < 2; i++) {
        textures[i].width = textures[i].height = textures[i].tex_w = textures[i].tex_h = 1;
        textures[i].rgba = rgba[i];
    }
    unsigned total_hits = 0;
    for (int sample = 0; sample < 10; sample++) {
        gt4_triangle triangles[12] = {0};
        for (int i = 0; i < 12; i++) {
            gt4_triangle *t = triangles + i;
            t->texture = i & 1; /* Distinct groups, independent of material state. */
            t->kind = 0;
            t->state.depth_write = 1;
            t->state.depth_test = 1;
            t->state.depth_func = 3;
            if (sample == 1 && i == 5) {
                t->state.fog_override = 1;
                t->state.fog_rgb[1] = 170;
            }
            if (sample == 2 && i == 5) {
                t->blend_enabled = 1;
                t->state.blend = 0x44;
            }
            if (sample == 3 && i == 5) {
                t->state.alpha_test = 1;
                t->state.alpha_func = 6;
                t->state.alpha_ref = 80;
                t->state.color_mask = 0xff;
            }
            if (sample == 4 && i == 5) {
                t->state.alpha_test = 1;
                t->state.alpha_func = 0;
                t->state.alpha_fail = 1;
            }
            if (sample == 5) {
                t->state.destination_test = 1;
                t->state.destination_bit = 1;
            }
            if (sample == 8 && i == 5) {
                t->state.depth_func = 0;
                t->state.depth_write = 0;
                t->state.cull = 2;
            }
            if (sample == 9 && i == 5) {
                t->kind = 4;
                t->lamp = 2;
            }
            const float positions[3][3] = {{-3, -3, 8}, {3, -3, 8}, {0, 3, 8}};
            for (int j = 0; j < 3; j++) {
                memcpy(t->v[j].p, positions[j], sizeof t->v[j].p);
                t->v[j].p[2] -= i * .01f; /* Later state changes remain depth-visible. */
                if (sample != 5)
                    t->v[j].p[0] += i * .15f; /* Keep exposed pixels from earlier states. */
                t->v[j].normal[2] = -1;
                t->v[j].alpha = .5f;
                t->v[j].rgb[0] = t->v[j].rgb[1] = t->v[j].rgb[2] = .7f;
            }
        }
        gt4_model model = {0};
        model.triangles = triangles;
        model.count = 12;
        model.textures = textures;
        model.ntextures = 2;
        r3d_mesh *mesh = r3d_mesh_create(&model);
        CHECK(mesh && mesh->ngroups==12);
        r3d_view view = {0};
        view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
        view.focal = 320;
        view.focal_y = 224;
        view.fog = 1;
        view.fog_distance = 4;
        view.fog_max = .5f;
        view.ghost = sample == 6 ? 1 : sample == 7 ? 2 : 0;
        view.ghost_alpha = .5f;
        view.brake = 1;
        uint8_t images[2][64 * 64 * 4];
        float depths[2][64 * 64];
        for (int uncached = 0; uncached < 2; uncached++) {
            texture_cache(uncached);
            r3d_begin(0, 0, 640, 448, NULL);
            CHECK(r3d_begin_target(64));
            GLuint nested_read = 0, draw_fb = video_draw_framebuffer();
            if (sample == 5) {
                glGenFramebuffers(1, &nested_read);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, nested_read);
            }
            r3d_draw(mesh, &view);
            if (sample == 5) {
                GLint restored;
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &restored);
                CHECK((GLuint)restored==nested_read && video_draw_framebuffer()==draw_fb);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_fb);
                glDeleteFramebuffers(1, &nested_read);
            }
            glReadPixels(0, 0, 64, 64, GL_DEPTH_COMPONENT, GL_FLOAT, depths[uncached]);
            r3d_end_target(images[uncached]);
            r3d_stats stats = r3d_frame_stats();
            if (stats.draws != (sample == 4 ? 13u : 12u))
                fprintf(stderr, "state sample=%d uncached=%d draws=%u\n", sample, uncached,
                        stats.draws);
            CHECK(stats.draws==(sample==4 ? 13u : 12u));
            CHECK(stats.state_updates+stats.state_cache_hits==12);
            if (uncached || view.ghost)
                CHECK(stats.state_updates==12 && !stats.state_cache_hits);
            else {
                total_hits += stats.state_cache_hits;
                if (sample == 0 || sample == 5)
                    CHECK(stats.state_updates==1 && stats.state_cache_hits==11);
            }
        }
        if (negative == 1)
            images[0][0] ^= 1;
        if (negative == 2)
            depths[0][0] = .5f;
        CHECK(!memcmp(images[0],images[1],sizeof images[0]));
        CHECK(!memcmp(depths[0],depths[1],sizeof depths[0]));
        int colored = 0;
        for (unsigned i = 0; i < sizeof images[0]; i++)
            if (i % 4 != 3 && images[0][i])
                colored++;
        CHECK(colored>100 || sample==6);
        r3d_mesh_destroy(mesh);
    }
    texture_cache(0);
    CHECK(total_hits>60 && glGetError()==GL_NO_ERROR);
    printf("Switch state comparison: 10 cases, %u state blocks reused, exact RGBA/depth\n",
           total_hits);
    return 0;
}

static int batch_case(int negative)
{
    gt4_triangle triangles[300] = {0};
    for (int i = 0; i < 300; i++) {
        gt4_triangle *t = triangles + i;
        t->texture = -1;
        float x = -3.7f + (i % 10) * .7f, y = -3.f + (i / 10) * .2f;
        const float points[3][3] = {{x, y, 8}, {x + .6f, y, 8}, {x + .3f, y + .19f, 8}};
        for (int v = 0; v < 3; v++) {
            memcpy(t->v[v].p, points[v], sizeof points[v]);
            t->v[v].alpha = 1;
            t->v[v].rgb[0] = t->v[v].rgb[1] = t->v[v].rgb[2] = .6f;
        }
    }
    gt4_model model = {0};
    model.triangles = triangles;
    model.count = 300;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    CHECK(mesh && mesh->ngroups==3);
    r3d_group original[3];
    memcpy(original, mesh->groups, sizeof original);
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    uint8_t images[2][64 * 64 * 4];
    const uint8_t model_list[4] = {1, 0, 0, 0};
    for (int sample = 0; sample < 7; sample++) {
        memcpy(mesh->groups, original, sizeof original);
        mesh->model_ranges[1] = sample == 6 ? 2 : 3;
        view.model_list = sample == 6 ? model_list : NULL;
        view.ghost = sample == 5 ? 2 : 0;
        view.ghost_alpha = .5f;
        if (sample == 1)
            mesh->groups[1].lo[0] = mesh->groups[1].hi[0] = 1000;
        if (sample == 2)
            mesh->groups[1].blend_enabled = 1;
        if (sample == 3) {
            mesh->groups[1].state.alpha_test = 1;
            mesh->groups[1].state.alpha_func = 1;
            mesh->groups[1].state.alpha_fail = 1;
        }
        if (sample == 4) {
            mesh->groups[1].state.destination_test = 1;
            mesh->groups[1].state.destination_bit = 1;
        }
        for (int uncached = 0; uncached < 2; uncached++) {
            texture_cache(uncached);
            r3d_begin(0, 0, 640, 448, NULL);
            CHECK(r3d_begin_target(64));
            r3d_draw(mesh, &view);
            r3d_end_target(images[uncached]);
            r3d_stats stats = r3d_frame_stats();
            unsigned draws = sample == 1 ? 2 : sample == 3 ? 4 : 3;
            if (sample == 6)
                draws = uncached ? 2 : 1;
            if (!sample && !uncached)
                draws = 1;
            CHECK(stats.draws==draws);
            CHECK(stats.merged_groups==(!uncached ? !sample ? 2u : sample==6 ? 1u : 0u : 0u));
        }
        if (negative)
            images[0][0] ^= 1;
        CHECK(!memcmp(images[0],images[1],sizeof images[0]));
        if (!sample) {
            int colored = 0;
            for (unsigned i = 0; i < sizeof images[0]; i++)
                if (i % 4 != 3 && images[0][i])
                    colored++;
            CHECK(colored>64);
        }
    }
    texture_cache(0);
    r3d_mesh_destroy(mesh);
    return 0;
}

int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    int quality_negative = argc > 1 && !strcmp(argv[1], "--quality-negative");
    int cache_negative = argc > 1 && !strcmp(argv[1], "--cache-negative");
    int batch_negative = argc > 1 && !strcmp(argv[1], "--batch-negative");
    int shader_negative = argc > 1 && !strcmp(argv[1], "--shader-negative");
    int state_negative = argc > 1 && !strcmp(argv[1], "--state-negative");
    if (argc > 1 && !strcmp(argv[1], "--state-depth-negative"))
        state_negative = 2;
    CHECK(!plat_video_init(&(plat_video_desc){1280,720,0,0,0}));
    SDL_HideWindow(SDL_GL_GetCurrentWindow());
    video_set_aspect(1);
    plat_video_set_view(640, 448);
    plat_video_begin_frame();
    CHECK(r3d_warmup());
    unsigned warmed = gl_program_count();
    CHECK(warmed==9);
    CHECK(r3d_warmup() && gl_program_count()==warmed);
    CHECK(!cache_case(cache_negative));
    CHECK(!batch_case(batch_negative));
    CHECK(!shader_case(shader_negative));
    CHECK(!state_case(state_negative));
    int w, h;
    switch_scene_dimensions(1920, 1080, &w, &h);
    CHECK(w==1280 && h==720);
    switch_scene_dimensions(640, 448, &w, &h);
    CHECK(w==640 && h==448);
    switch_scene_dimensions(1280, 720, &w, &h);
    CHECK(w==960 && h==540);
    plat_video_begin_frame();
    video_begin_scene();
    int x, y, vw, vh;
    video_viewport(0, 0, 640, 448, &x, &y, &vw, &vh);
    CHECK(vw==960+quality_negative && vh==540);
    GLint samples;
    glGetIntegerv(GL_SAMPLES, &samples);
    CHECK(samples<=1);
    uint8_t solid[4] = {31, 79, 143, 255};
    plat_draw_quad(NULL, 0, 0, 640, 448, 0, 0, 1, 1, solid);
    video_end_scene();
    video_viewport(0, 0, 640, 448, &x, &y, &vw, &vh);
    CHECK(vw==1280 && vh==720);
    r3d_begin(0, 0, 640, 448, NULL);
    GLuint page = video_draw_framebuffer();
    CHECK(r3d_begin_layer(1));
    CHECK(layer.w==960 && layer.h==540 && vp[2]==960 && vp[3]==540);
    r3d_end_layer(1, 0);
    CHECK(video_draw_framebuffer()==page && vp[2]==1280 && vp[3]==720);
    int pw, ph;
    unsigned char *page_pixels = plat_sdl_read_pixels(&pw, &ph);
    CHECK(page_pixels && pw==1280 && ph==720);
    const unsigned char *middle = page_pixels + (ph / 2 * pw + pw / 2) * 4;
    CHECK(abs((int)middle[0]-solid[0])<=1 && abs((int)middle[1]-solid[1])<=1 &&
          abs((int)middle[2]-solid[2])<=1);
    free(page_pixels);
    plat_video_begin_frame();
    CHECK(switch_gpu_warmup());
    switch_gpu_image *env = switch_gpu_env_create(FACE, MAP);
    CHECK(env);
    for (int face = 0; face < 6; face++) {
        CHECK(r3d_begin_target(FACE));
        glEnable(GL_SCISSOR_TEST);
        for (int y = 0; y < FACE; y++)
            for (int x = 0; x < FACE; x++) {
                unsigned char *p = faces[face] + (y * FACE + x) * 4;
                for (int c = 0; c < 3; c++)
                    p[c] = (unsigned char)((face * 31 + x * 13 + y * 17 + c * 47) % 256);
                p[3] = 255;
                glScissor(x, FACE - 1 - y, 1, 1);
                glClearColor(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        switch_gpu_env_capture(env, face);
    }
    for (int frame = 0; frame < 12; frame++) {
        float angle = frame * .31f, pitch = frame * .13f;
        float r[3] = {cosf(angle), 0, sinf(angle)},
              u[3] = {sinf(angle) * sinf(pitch), cosf(pitch), -cosf(angle) * sinf(pitch)},
              f[3] = {-sinf(angle) * cosf(pitch), sinf(pitch), cosf(angle) * cosf(pitch)};
        cpu_environment(r, u, f);
        switch_gpu_env_remap(env, r, u, f);
        CHECK(read_image(env) && compare(negative));
    }
    gt4_triangle reflection[2] = {0};
    const float points[4][3] = {{-4, -4, 8}, {4, -4, 8}, {4, 4, 8}, {-4, 4, 8}};
    const int index[2][3] = {{0, 1, 2}, {0, 2, 3}};
    for (int t = 0; t < 2; t++) {
        reflection[t].texture = -2;
        for (int i = 0; i < 3; i++) {
            gt4_vertex *v = &reflection[t].v[i];
            memcpy(v->p, points[index[t][i]], sizeof v->p);
            v->normal[2] = -1;
            v->rgb[0] = v->rgb[1] = v->rgb[2] = v->alpha = 1;
        }
    }
    gt4_model reflective = {0};
    reflective.triangles = reflection;
    reflective.count = 2;
    r3d_mesh *reflective_mesh = r3d_mesh_create(&reflective);
    CHECK(reflective_mesh);
    tex1_image cpu_map = {0};
    cpu_map.width = cpu_map.height = cpu_map.tex_w = cpu_map.tex_h = MAP;
    cpu_map.rgba = reference;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    view.env_color[0] = 1;
    unsigned char *drawn[2];
    int width[2], height[2];
    for (int pass = 0; pass < 2; pass++) {
        plat_video_begin_frame();
        r3d_begin(0, 0, 640, 448, pass ? NULL : &cpu_map);
        if (pass)
            r3d_gpu_environment(video_texture_id(switch_gpu_image_texture(env)), MAP, MAP);
        r3d_draw(reflective_mesh, &view);
        r3d_end();
        drawn[pass] = plat_sdl_read_pixels(width + pass, height + pass);
        CHECK(drawn[pass]);
    }
    CHECK(width[0]==width[1] && height[0]==height[1]);
    int nonzero = 0;
    for (int i = 0; i < width[0] * height[0] * 4; i++) {
        CHECK(abs((int)drawn[0][i]-drawn[1][i])<=1);
        if (i % 4 < 3 && drawn[0][i])
            nonzero++;
    }
    CHECK(nonzero>100);
    free(drawn[0]);
    free(drawn[1]);
    r3d_mesh_destroy(reflective_mesh);
    switch_gpu_image_destroy(env);
    unsigned char layers[4][MAP * MAP * 4];
    const unsigned char *source[4];
    for (int j = 0; j < 4; j++) {
        source[j] = layers[j];
        for (unsigned k = 0; k < sizeof layers[j]; k++)
            layers[j][k] = (unsigned char)(j * 53 + k * 11);
    }
    switch_gpu_image *mix = switch_gpu_mix_create(MAP, MAP, source);
    CHECK(mix);
    GLuint nested;
    glGenFramebuffers(1, &nested);
    glBindFramebuffer(GL_FRAMEBUFFER, nested);
    for (int frame = 0; frame < 24; frame++) {
        float weights[4] = {frame * .013f, .1f, .2f, 1 - frame * .021f};
        for (int at = 0; at < MAP * MAP * 4; at += 4) {
            for (int c = 0; c < 3; c++) {
                float v = 0;
                for (int j = 0; j < 4; j++)
                    if (weights[j] > 0)
                        v += weights[j] * layers[j][at + c];
                reference[at + c] = (unsigned char)(v > 255 ? 255 : v);
            }
            reference[at + 3] = 255;
        }
        switch_gpu_mix(mix, weights);
        CHECK(video_draw_framebuffer()==nested);
        CHECK(read_image(mix) && compare(negative));
    }
    switch_gpu_image_destroy(mix);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &nested);
    unsigned char rgba[3][4] = {{11, 22, 33, 255}, {77, 88, 99, 255}, {123, 45, 67, 255}};
    tex1_image images[3] = {0};
    gt4_triangle triangle = {0};
    triangle.texture = 0;
    for (int i = 0; i < 3; i++) {
        images[i].width = images[i].height = images[i].tex_w = images[i].tex_h = 1;
        images[i].rgba = rgba[i];
    }
    gt4_model model = {0};
    model.triangles = &triangle;
    model.count = 1;
    model.ntextures = 3;
    model.textures = images;
    r3d_mesh *parent = r3d_mesh_create(&model);
    CHECK(parent);
    CHECK(parent->textures[0].id && !parent->textures[1].id && !parent->textures[2].id);
    triangle.texture = 1;
    triangle.wheel_blur_texture = 3;
    r3d_mesh *child = r3d_mesh_create_shared(&model, parent);
    CHECK(child);
    CHECK(child->textures[1].id && child->textures[2].id && !child->textures[0].id);
    r3d_mesh_destroy(parent);
    r3d_mesh_destroy(child);
    plat_video_shutdown();
    return 0;
}
