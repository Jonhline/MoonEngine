/* P: compare direct/batched atlas depth with the old per-tile copy transport.
   Include the renderer to keep this reference outside the public game API. */
#include "../../src/platform/sdl/render3d.c"
#include "plat_sdl.h"
#include <stdio.h>
static r3d_mesh *casters(void)
{
    gt4_triangle triangles[180] = {0};
    for (int i = 0; i < 180; i++) {
        gt4_triangle *t = &triangles[i];
        t->texture = -1;
        t->kind = i % 2;
        t->state.depth_func = 1;
        t->state.depth_write = 1;
        int cell = (i / 2) % 36;
        float x = -2.6f + (cell % 6) * .2f, y = -.6f + (cell / 6) * .2f, z = 3.f + (i / 72) * .3f;
        const float corner[6][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}};
        for (int j = 0; j < 3; j++) {
            gt4_vertex *v = &t->v[j];
            v->p[0] = x + corner[(i % 2) * 3 + j][0] * .2f;
            v->p[1] = y + corner[(i % 2) * 3 + j][1] * .2f;
            v->p[2] = z;
            v->alpha = 1;
            v->rgb[0] = v->rgb[1] = v->rgb[2] = 1;
            v->normal[2] = -1;
        }
        if (i >= 144 && i < 156) {
            t->state.alpha_test = 1;
            t->state.alpha_func = 6;
            t->state.alpha_ref = i % 2 ? 64 : 100;
            for (int j = 0; j < 3; j++)
                t->v[j].alpha = i % 3 ? 1 : 0;
        }
        if (i >= 156 && i < 162)
            t->blend_enabled = 1;
        if (i >= 162 && i < 168)
            t->texture = -2;
        if (i >= 168)
            t->lamp = i % 2 ? 1 : 2;
    }
    gt4_model m = {0};
    m.triangles = triangles;
    m.count = 180;
    return r3d_mesh_create(&m);
}
static GLuint reference_image(void)
{
    GLuint image;
    glGenTextures(1, &image);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1536, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return image;
}
int main(int argc, char **argv)
{
    int status = 1, started = 0;
    GLuint reference = 0;
    r3d_mesh *mesh = NULL;
    r3d_lamp_shadow *atlas = NULL;
    r3d_shadow_map *tile = NULL;
    unsigned char *actual = NULL, *expected = NULL;
    if (plat_video_init(&(plat_video_desc){128, 128, 0, 0, 0}))
        goto cleanup;
    started = 1;
    SDL_HideWindow(plat_sdl_window());
    if (!r3d_available())
        goto cleanup;
    mesh = casters();
    if (!mesh)
        goto cleanup;
    r3d_car_lamp lamps[R3D_CAR_LAMPS] = {0};
    for (int i = 0; i < 24; i++)
        lamps[i] = (r3d_car_lamp){
            {-4, (i - 12) * .015f, 0, 60}, {.5547002f, 0, .8320503f, .9f}, {6, 5.4f, 4.5f, 0}};
    r3d_car_lamps(1);
    r3d_car_lamp_frame(lamps, 24);
    r3d_view view = {0};
    float a = .08f;
    view.rot[0] = view.rot[8] = cosf(a);
    view.rot[2] = sinf(a);
    view.rot[6] = -sinf(a);
    view.rot[4] = 1;
    plat_video_begin_frame();
    r3d_totals before = r3d_work_totals();
    if (!r3d_lamp_shadow_begin(&atlas))
        goto cleanup;
    for (unsigned i = 0; i < 24; i++) {
        if (!r3d_lamp_shadow_light(atlas, i))
            goto cleanup;
        view.brake = i % 2;
        if (!(argc > 1 && i == 23))
            r3d_lamp_shadow_mesh(atlas, mesh, &view);
        r3d_lamp_shadow_end_light(atlas);
    }
    r3d_lamp_shadow_bind(atlas);
    r3d_totals direct = r3d_work_totals();
    reference = reference_image();
    for (unsigned i = 0; i < 24; i++) {
        if (!shadow_map_begin_size(&tile, lamps[i].position, lamps[i].direction, 256))
            goto cleanup;
        memcpy(tile->projection, atlas->projection[i], sizeof tile->projection);
        tile->perspective = 1;
        tile->batch = 0;
        glUniformMatrix4fv(tile->projection_uniform, 1, GL_FALSE, tile->projection);
        glUniform1i(tile->perspective_uniform, 1);
        view.brake = i % 2;
        r3d_shadow_map_mesh(tile, mesh, &view, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, reference);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, (i % 6) * 256, (i / 6) * 256, 0, 0, 256, 256);
        r3d_shadow_map_end(tile);
    }
    r3d_totals copied = r3d_work_totals();
    size_t bytes = 1536u * 1024 * 4;
    actual = malloc(bytes);
    expected = malloc(bytes);
    if (!actual || !expected)
        goto cleanup;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas->scratch->image);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, actual);
    glBindTexture(GL_TEXTURE_2D, reference);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, expected);
    unsigned covered = 0;
    double maximum = 0;
    int alpha_mismatch = 0;
    for (size_t i = 0; i < bytes; i += 4) {
        alpha_mismatch += actual[i + 3] != expected[i + 3];
        if (actual[i + 3] || expected[i + 3]) {
            double x = (actual[i] * 65536. + actual[i + 1] * 256. + actual[i + 2]) / 16777215.;
            double y =
                (expected[i] * 65536. + expected[i + 1] * 256. + expected[i + 2]) / 16777215.;
            maximum = fmax(maximum, fabs(x - y));
            covered++;
        }
    }
    printf(
        "atlas direct draws=%llu reference=%llu merged=%llu covered=%u max_depth_error=%g alpha_mismatch=%d\n",
        (unsigned long long)(direct.draws - before.draws),
        (unsigned long long)(copied.draws - direct.draws),
        (unsigned long long)(direct.merged_groups - before.merged_groups), covered, maximum,
        alpha_mismatch);
    if (alpha_mismatch || maximum > 2e-6 || covered < 10000 ||
        direct.draws - before.draws >= copied.draws - direct.draws)
        goto cleanup;
    plat_video_end_frame();
    puts(
        "PASS all 24 tiles, copy reference, batching, alpha rejection, reflection/glass exclusion and brake branches");
    status = 0;
cleanup:
    free(actual);
    free(expected);
    if (started) {
        r3d_lamp_shadow_destroy(atlas);
        r3d_shadow_map_destroy(tile);
        r3d_mesh_destroy(mesh);
        if (reference)
            glDeleteTextures(1, &reference);
        plat_video_shutdown();
    }
    return status;
}
