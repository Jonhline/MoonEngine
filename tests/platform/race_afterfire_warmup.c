/* Compare preloading against the six branches used by the retained lazy path. */
#define r3d_mesh_create warmup_mesh_create
#include "../../src/platform/sdl/race_view.c"
#undef r3d_mesh_create
static unsigned uploads;
static uint64_t expected[12];
static uint64_t mesh_hash(const gt4_model *model)
{
    const unsigned char *bytes = (const unsigned char *)model->triangles;
    size_t length = (size_t)model->count * sizeof *model->triangles;
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < length; i++)
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    return hash;
}
r3d_mesh *warmup_mesh_create(const gt4_model *model)
{
    if (uploads >= 12 || mesh_hash(model) != expected[uploads])
        return NULL;
    return (r3d_mesh *)(uintptr_t)(++uploads);
}
int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;
    const char *error = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    size_t size = 0;
    uint8_t *bytes = vol ? gt4_vol_read(vol, "/race/after_fire.mdl", &size) : NULL;
    if (!bytes)
        return 1;
    for (int i = 0; i < 12; i++) {
        gt4_model *m = gt4_model_decode_afterfire(bytes, size, i % 2, i / 2);
        if (!m)
            return 1;
        expected[i] = mesh_hash(m);
        gt4_model_free(m);
    }
    free(bytes);
    gt4_vol_close(vol);
    if (argc > 2)
        expected[3] ^= 1;
    race_view view = {0};
    view.disc = argv[1];
    prepare_afterfire(&view);
    if (!view.afterfire_tried || uploads != 12)
        return 1;
    for (unsigned i = 0; i < 6; i++)
        if (view.afterfire[i] != (r3d_mesh *)(uintptr_t)(i * 2 + 1) ||
            view.afterfire_ground[i] != (r3d_mesh *)(uintptr_t)(i * 2 + 2))
            return 1;
    prepare_afterfire(&view);
    if (uploads != 12)
        return 1;
    puts(
        "after-fire: twelve original branch meshes match lazy data; repeated preparation uploads nothing");
    return 0;
}
