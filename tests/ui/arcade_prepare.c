/* Real disc UI: warm resources without entering pages or changing pixels. */
#define SDL_MAIN_HANDLED
#include "../../src/core/ui.c"
#include "menu.h"
#include "gtmode_game.h"
#include "save.h"
#include "plat_sdl.h"
#include "video_gl.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Arcade preparation line %d: %s\n", __LINE__, #x);                     \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)

static int run(gt4_vol *vol, int warm, int negative, unsigned char **pixels, int *w, int *h)
{
    char error[256];
    menu *m = menu_open(vol, "arcade", error, sizeof error);
    CHECK(m);
    ui_project *p = menu_ui(m);
    int target = -1;
    int count = p->nall;
    ui_widget *before = malloc((size_t)count * sizeof *before);
    CHECK(before);
    for (int i = 0; i < count; i++)
        before[i] = *p->all[i];
    uint64_t start = plat_time_ns();
    if (warm && !negative)
        for (int root = 0; root < ui_root_count(p); root++) {
            const char *name = ui_root_name(p, root);
            if (!strcmp(name, "GameRoot") || !strcmp(name, "CategoryRoot") ||
                !strcmp(name, "CourseRoot"))
                ui_prepare_root_states(p, root, NULL, NULL);
        }
    CHECK(p->nall==count);
    for (int i = 0; i < count; i++)
        CHECK(!memcmp(before+i,p->all[i],sizeof *before));
    free(before);
    CHECK(menu_active_root(m)<0);
    for (int i = 0; i < ui_root_count(p); i++)
        if (!strcmp(ui_root_name(p, i), "GameRoot"))
            target = i;
    CHECK(target>=0);
    fprintf(stderr, "Arcade host: warm=%d preparation_ms=%.3f fonts=%d images=%d\n", warm,
            (plat_time_ns() - start) / 1e6, p->nfonts, p->nimgs);
    menu_enter(m, target);
    for (int frame = 0; frame < 40; frame++) {
        menu_tick(m);
        menu_advance(m, 1.0 / 60);
        int fonts = p->nfonts, images = p->nimgs, flashes = p->nflashes;
        start = plat_time_ns();
        plat_video_begin_frame();
        ui_draw_root(p, target, NULL, NULL);
        video_flush2d();
        if (!frame)
            fprintf(stderr,
                    "Arcade host: warm=%d first_submit_ms=%.3f new_fonts=%d new_images=%d\n", warm,
                    (plat_time_ns() - start) / 1e6, p->nfonts - fonts, p->nimgs - images);
        if (warm)
            CHECK(fonts==p->nfonts && images==p->nimgs && flashes==p->nflashes);
        if (frame == 39)
            *pixels = plat_sdl_read_pixels(w, h);
        plat_video_end_frame();
    }
    CHECK(*pixels);
    menu_close(m);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    SDL_SetMainReady();
    const char *error = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    if (!vol || !gt4_game_load_disc(argv[1]))
        return 1;
    gt4_save_init("");
    if (plat_video_init(&(plat_video_desc){640, 448, 0, 0, 0}))
        return 1;
    SDL_HideWindow(SDL_GL_GetCurrentWindow());
    video_set_menu_pacing(0);
    plat_video_set_view(640, 480);
    unsigned char *cold = NULL, *warm = NULL;
    int cw, ch, ww, wh;
    int ok = run(vol, 0, 0, &cold, &cw, &ch) && run(vol, 1, argc > 2, &warm, &ww, &wh);
    if (ok)
        ok = cw == ww && ch == wh && !memcmp(cold, warm, (size_t)cw * ch * 4);
    if (!ok)
        fprintf(stderr, "Arcade preparation: resource/state/pixel comparison failed\n");
    free(cold);
    free(warm);
    plat_video_shutdown();
    gt4_vol_close(vol);
    gt4_save_shutdown();
    return ok ? 0 : 1;
}
