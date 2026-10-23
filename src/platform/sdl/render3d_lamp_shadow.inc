/* P: one 256-pixel perspective tile per active lamp (six columns, four rows).
   Visual meshes only: no collision queries, simulation state or GPU readback.
   Render directly to the atlas, keeping one depth/render target across tiles. */
struct r3d_lamp_shadow {
    r3d_shadow_map *scratch;
    int drawing;
    unsigned mask, current;
    float projection[R3D_CAR_LAMPS][16];
};
static r3d_lamp_shadow *lamp_shadow_bound;
void r3d_lamp_shadow_bind(r3d_lamp_shadow *atlas)
{
    if (atlas && atlas->drawing) {
        r3d_shadow_map_end(atlas->scratch);
        atlas->drawing = 0;
    }
    lamp_shadow_bound = atlas && atlas->mask ? atlas : NULL;
}
void r3d_lamp_shadow_destroy(r3d_lamp_shadow *atlas)
{
    if (!atlas)
        return;
    if (lamp_shadow_bound == atlas)
        lamp_shadow_bound = NULL;
    if (atlas->drawing)
        r3d_shadow_map_end(atlas->scratch);
    r3d_shadow_map_destroy(atlas->scratch);
    free(atlas);
}
int r3d_lamp_shadow_begin(r3d_lamp_shadow **owner)
{
    lamp_shadow_bound = NULL;
    if (!owner || !car_lamps_enabled || !car_lamp_count)
        return 0;
    if (!*owner) {
        r3d_lamp_shadow *s = calloc(1, sizeof *s);
        if (!s)
            return 0;
        *owner = s;
    }
    if ((*owner)->drawing) {
        r3d_shadow_map_end((*owner)->scratch);
        (*owner)->drawing = 0;
    }
    (*owner)->mask = 0;
    return 1;
}
int r3d_lamp_shadow_light(r3d_lamp_shadow *atlas, unsigned index)
{
    if (!atlas || index >= (unsigned)car_lamp_count)
        return 0;
    const r3d_car_lamp *light = &car_lamp_frame[index];
    if (light->position[3] <= .05f || light->direction[3] <= 0 || light->direction[3] >= 1)
        return 0;
    if (!atlas->drawing) {
        if (!shadow_map_begin_target(&atlas->scratch, light->position, light->direction, 1536,
                                     1024))
            return 0;
        atlas->drawing = 1;
    }
    r3d_shadow_map *s = atlas->scratch;
    glViewport((index % 6) * 256, (index / 6) * 256, 256, 256);
    glScissor((index % 6) * 256, (index / 6) * 256, 256, 256);
    glEnable(GL_SCISSOR_TEST);
    float forward[3], right[3], up[3] = {0, 1, 0};
    float length = sqrtf(light->direction[0] * light->direction[0] +
                         light->direction[1] * light->direction[1] +
                         light->direction[2] * light->direction[2]);
    for (int k = 0; k < 3; k++)
        forward[k] = light->direction[k] / length;
    if (fabsf(forward[1]) > .95f) {
        up[1] = 0;
        up[2] = 1;
    }
    right[0] = forward[1] * up[2] - forward[2] * up[1];
    right[1] = forward[2] * up[0] - forward[0] * up[2];
    right[2] = forward[0] * up[1] - forward[1] * up[0];
    length = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    for (int k = 0; k < 3; k++)
        right[k] /= length;
    up[0] = right[1] * forward[2] - right[2] * forward[1];
    up[1] = right[2] * forward[0] - right[0] * forward[2];
    up[2] = right[0] * forward[1] - right[1] * forward[0];
    float scale = light->direction[3] / sqrtf(1 - light->direction[3] * light->direction[3]);
    memset(s->projection, 0, sizeof s->projection);
    for (int k = 0; k < 3; k++) {
        s->projection[k * 4] = .5f * (right[k] * scale + forward[k]);
        s->projection[k * 4 + 1] = .5f * (up[k] * scale + forward[k]);
        s->projection[k * 4 + 2] = forward[k] / light->position[3];
        s->projection[k * 4 + 3] = forward[k];
        for (int row = 0; row < 4; row++)
            s->projection[12 + row] -= s->projection[k * 4 + row] * light->position[k];
    }
    s->perspective = 1;
    s->batch = 1;
    atlas->current = index;
    memcpy(atlas->projection[index], s->projection, sizeof s->projection);
    glUniformMatrix4fv(s->projection_uniform, 1, GL_FALSE, s->projection);
    glUniform1i(s->perspective_uniform, 1);
    return 1;
}
void r3d_lamp_shadow_mesh(r3d_lamp_shadow *atlas, const r3d_mesh *mesh, const r3d_view *view)
{
    if (atlas)
        r3d_shadow_map_mesh(atlas->scratch, mesh, view, 1);
}
void r3d_lamp_shadow_end_light(r3d_lamp_shadow *atlas)
{
    if (!atlas || !atlas->scratch)
        return;
    atlas->mask |= 1u << atlas->current;
}
static void lamp_shadow_receiver(int count)
{
    r3d_lamp_shadow *s = count ? lamp_shadow_bound : NULL;
    glUniform1i(U.lamp_shadow_mask, s ? (int)s->mask : 0);
    glUniform1i(U.lamp_shadow_map, 4);
    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, s ? s->scratch->image : 0);
    glActiveTexture(GL_TEXTURE0);
    if (s)
        glUniformMatrix4fv(U.lamp_shadow_projection, count, GL_FALSE, s->projection[0]);
}
