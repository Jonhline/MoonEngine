/* Developer-only UI root tour: arbitrary authored projects, no launcher or save UI. */
#include "menu.h"
#include "gtmode_game.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int screenshot(const char *path)
{
    int w = 0, h = 0;
    unsigned char *pixels = plat_sdl_read_pixels(&w, &h);
    if (!pixels)
        return 0;
    size_t length = 0;
    void *png = tdefl_write_image_to_png_file_in_memory(pixels, w, h, 4, &length);
    free(pixels);
    FILE *file = png ? fopen(path, "wb") : NULL;
    int ok = file && fwrite(png, 1, length, file) == length;
    if (file && fclose(file))
        ok = 0;
    mz_free(png);
    return ok;
}
#undef main
int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: ui_tour disc project root --shot path\n");
        return 2;
    }
    const char *shot = NULL;
    for (int i = 4; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--shot"))
            shot = argv[i + 1];
    if (!shot)
        return 2;
    plat_video_desc video = {640, 480, 0, 0, 0, 0};
    if (plat_video_init(&video))
        return 1;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    if (!vol)
        return 1;
    if (!gt4_game_load_disc(argv[1]))
        return 1;
    menu *m = menu_open(vol, argv[2], error, sizeof error);
    if (!m) {
        fprintf(stderr, "error: %s\n", error);
        return 1;
    }
    ui_project *p = menu_ui(m);
    int root = -1;
    for (int i = 0; i < ui_root_count(p); i++)
        if (!strcmp(ui_root_name(p, i), argv[3]))
            root = i;
    if (root < 0) {
        fprintf(stderr, "error: requested root unavailable\n");
        return 2;
    }
    menu_enter(m, root);
    for (int i = 0; i < 3; i++)
        menu_tick(m);
    int active = menu_active_root(m), drawn = 0, skipped = 0;
    printf("UI_TOUR requested=%s/%s active=%s/%s\n", argv[2], argv[3], argv[2],
           ui_root_name(p, active));
    /* Capture the requested tree even if its script redirects. The active name
       above distinguishes a contextual layout capture from successful entry. */
    plat_video_begin_frame();
    plat_video_set_view(640, 480);
    ui_draw_root(p, root, &drawn, &skipped);
    printf("UI_TOUR drawn=%d skipped=%d\n", drawn, skipped);
    int ok = screenshot(shot);
    plat_video_end_frame();
    menu_close(m);
    gt4_vol_close(vol);
    plat_video_shutdown();
    return ok ? 0 : 1;
}
