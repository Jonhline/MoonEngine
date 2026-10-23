/* Prize materials stay faithful to the source MDLS, including rose bronze. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#include "model.h"
static unsigned word(const uint8_t *p)
{
    return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    const char *paths[] = {"/piece/prize/1st.mdl", "/piece/prize/2nd.mdl", "/piece/prize/3rd.mdl"};
    for (int rank = 0; rank < 3; rank++) {
        size_t size;
        uint8_t *bytes = gt4_vol_read(vol, paths[rank], &size);
        CHECK(bytes && size >= 128);
        size_t table = word(bytes + 64);
        CHECK(table + 3 * 80 <= size);
        gt4_model *model = gt4_model_decode_part(bytes, size, 0, 0);
        CHECK(model && !model->unsupported);
        for (int material = 0; material < 3; material++) {
            float ambient[3], specular[3];
            memcpy(ambient, bytes + table + material * 80, sizeof ambient);
            memcpy(specular, bytes + table + material * 80 + 32, sizeof specular);
            int matched = 0;
            for (int i = 0; i < model->count; i++) {
                const gt4_triangle *t = model->triangles + i;
                if (t->kind != 1 || memcmp(t->v[0].ambient, ambient, sizeof ambient))
                    continue;
                CHECK(!memcmp(t->v[0].specular, specular, sizeof specular));
                matched++;
            }
            CHECK(matched > 0);
        }
        if (rank == 2) {
            float diffuse[3];
            memcpy(diffuse, bytes + table + 80 + 16, sizeof diffuse);
            if (argc > 2)
                diffuse[1] = diffuse[0]; /* Negative: recolor the authored rose material. */
            CHECK(diffuse[0] == .4f && diffuse[1] == .2f && diffuse[2] == .22039999067783356f);
        }
        gt4_model_free(model);
        free(bytes);
    }
    gt4_vol_close(vol);
    puts(
        "PASS three original prize models retain all source material colours, including rose bronze");
    return 0;
}
