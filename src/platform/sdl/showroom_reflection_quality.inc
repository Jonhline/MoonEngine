/* P: generated studio maps may grow independently of authored env textures.
   Keep the captured camera basis when rebuilding so Original restoration
   does not bake a later animated camera into the reference environment. */
static void showroom_reflection_resize(showroom *s)
{
    int size = r3d_reflection_size();
    if (s->reflection_size == size)
        return;
    r3d_gpu_environment(0, 0, 0);
    switch_gpu_image_destroy(s->pc_env);
    s->pc_env = NULL;
    s->pc_env_ready = 0;
    for (int i = 0; i < 4; i++) {
        free(s->panel_maps[i]);
        s->panel_maps[i] = NULL;
    }
    free(s->studio_env.rgba);
    memset(&s->studio_env, 0, sizeof s->studio_env);
    s->env_now = NULL;
    s->panel_ready = 0;
    s->reflection_size = size;
    fprintf(stderr, "studio reflections: map=%d\n", size);
}

static const tex1_image *showroom_reflection_compose(showroom *s, float time)
{
    s->pc_env_ready = 0;
    if (s->reflection_size == ENV_SIZE || !s->gpu || !r3d_resident_reflections() ||
        s->panel_ready != 1 || !s->light)
        return compose_env(s, time);
    if (!s->pc_env)
        s->pc_env = switch_gpu_mix_create(s->reflection_size, s->reflection_size,
                                          (const uint8_t *const *)s->panel_maps);
    if (!s->pc_env)
        return compose_env(s, time);
    static const char *const names[4] = {"Alpha0", "Alpha1", "Alpha2", "Alpha3"};
    float weights[4];
    for (int i = 0; i < 4; i++)
        if (!gt4_mot1_value(s->light, names[i], time, &weights[i]) || weights[i] < 0)
            weights[i] = 0;
    switch_gpu_mix(s->pc_env, weights);
    s->pc_env_ready = 1;
    return NULL;
}
