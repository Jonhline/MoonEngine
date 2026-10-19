/* Authored access faces, real profile writes, responsive UI and failure propagation. */
#include <SDL.h>
#undef main
#define main access_fixture_main
#include "../test_process.h"
#include "pregrid_menu.c"
#undef main
static unsigned frames, on_frames, off_frames;
static int fail_write, fail_index;
static void present_wait(menu *m, float seconds, void *user)
{
    (void)user;
    menu_animate(m, seconds > 0 ? seconds : 1.0f / 60);
    ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
    ui_widget *indicator = save_access_widget(ui_root_widget(m->ui, menu_active_root(m)));
    CHECK(indicator);
    ui_widget *save = ui_widget_child(indicator, "save");
    ui_widget *load = ui_widget_child(indicator, "load");
    CHECK(save && load && (save->visible != load->visible));
    CHECK(!strcmp(save->type, "ImageFace") && !strcmp(load->type, "ImageFace"));
    CHECK(save->image_path && !strcmp(save->image_path, "image/labo/memory_header.png"));
    CHECK(load->image_path && !strcmp(load->image_path, "image/labo/memory_header.png"));
    ui_widget *active = save->visible ? save : load;
    frames++;
    on_frames += active->opacity > .5f;
    off_frames += active->opacity < .5f;
    SDL_Delay(17);
}
static void screenshot(const char *path)
{
    (void)path;
}
static int delayed_write(const char *path, const uint8_t *data, size_t size)
{
    SDL_Delay(250); /* Only this test simulates a slow storage device. */
    return fail_write || (fail_index && strstr(path, "active.ogt4"))
               ? 0
               : gt4_save_write(path, data, size);
}
static int delayed_read(const char *path, uint8_t **data, size_t *size, uint64_t *stamp)
{
    SDL_Delay(250);
    return gt4_save_read(path, data, size, stamp);
}
#define gt4_save_read delayed_read
#define gt4_save_write delayed_write
#include "../../src/app/storage_present.inc"
#undef gt4_save_write
#undef gt4_save_read
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    char folder[200], error[256];
    snprintf(folder, sizeof folder, "build/storage-access-%llu-%lu", (unsigned long long)time(NULL),
             test_process_id());
    gt4_save_init(folder);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    menu *m = menu_open(vol, "gtmode", error, sizeof error);
    CHECK(m);
    menu_enter(m, 0);
    menu_save_capture(m);
    CHECK(gt4_save_create(1, "Access test"));
    menu_save_refresh(m);
    ah_set_log(m->vm, log_script, NULL);
    errors = 0;
    menu_set_save_presenter(m, present_save);
    menu_set_load_presenter(m, present_load);
    gt4_save_mark_dirty();
    CHECK(menu_save_store(m));
    CHECK(!gt4_save_dirty() && frames >= 6 && on_frames && off_frames && !errors);
    ui_widget *indicator = save_access_widget(ui_root_widget(m->ui, menu_active_root(m)));
    CHECK(!ui_widget_child(indicator, "save")->visible);
    CHECK(!ui_widget_child(indicator, "load")->visible);
    menu_save_access(m, 2);
    CHECK(ui_widget_child(indicator, "load")->visible);
    CHECK(!ui_widget_child(indicator, "save")->visible);
    menu_save_access(m, 0);
    char path[240];
    snprintf(path, sizeof path, "%s/slot01.ogt4", folder);
    uint8_t *before, *after;
    size_t n, n2;
    CHECK(gt4_save_read(path, &before, &n, NULL) > 0);
    gt4_save_mark_dirty();
    fail_write = 1;
    CHECK(!menu_save_store(m) && gt4_save_dirty());
    CHECK(!ui_widget_child(indicator, "save")->visible);
    CHECK(gt4_save_read(path, &after, &n2, NULL) > 0 && n == n2 && !memcmp(before, after, n));
    free(before);
    free(after);
    fail_write = 0;
    CHECK(menu_save_load(m, 1) && !gt4_save_dirty());
    CHECK(!menu_save_load(m, 8) && gt4_save_active() == 1);
    CHECK(!ui_widget_child(indicator, "load")->visible);

    fail_write = 0;
    menu_set_profile_change_presenter(m, present_profile_change);
    unsigned old_frames = frames;
    CHECK(menu_save_profile_change(m, MENU_PROFILE_CREATE, 2, "New player"));
    CHECK(frames > old_frames && gt4_save_active() == 2);
    old_frames = frames;
    CHECK(menu_save_profile_change(m, MENU_PROFILE_RENAME, 2, "Renamed player"));
    CHECK(frames > old_frames && !strcmp(gt4_save_player(), "Renamed player"));
    fail_write = 1;
    CHECK(!menu_save_profile_change(m, MENU_PROFILE_RENAME, 2, "Failed rename"));
    CHECK(!strcmp(gt4_save_player(), "Renamed player"));
    CHECK(!menu_save_profile_change(m, MENU_PROFILE_CREATE, 3, "Failed creation"));
    CHECK(gt4_save_active() == 2 && !strcmp(gt4_save_player(), "Renamed player"));
    fail_write = 0;
    fail_index = 1;
    gt4_save_mark_dirty();
    CHECK(!menu_save_profile_change(m, MENU_PROFILE_CREATE, 3, "Activation failed"));
    CHECK(gt4_save_active() == 2 && gt4_save_dirty());
    CHECK(!menu_save_load(m, 1) && gt4_save_active() == 2 && gt4_save_dirty());
    fail_index = 0;
    menu_save_refresh(m);
    ah_obj *game = game_object(m);
    ah_val option, result;
    CHECK(ah_get_member(m->vm, game, "option", &option));
    ah_set_member(m->vm, game, "money", ah_int(54321));
    /* The one-time GT3 transfer flags must survive a new session, not just
       remain attached to an already-open menu VM (001D49C8 / 001D4698). */
    ah_set_member(m->vm, game, "withdrawn", ah_int(1));
    ah_set_member(m->vm, game, "withdrawnGT4p", ah_int(1));
    ah_set_member(m->vm, game, "withdrawn", ah_int(2));
    CHECK(!gt4_save_capture(m->vm, game, 1)); /* Strict boolean progress field. */
    ah_set_member(m->vm, game, "withdrawn", ah_int(1));
    ah_set_member(m->vm, option.u.o, "auto_save", ah_int(0));
    CHECK(gt4_save_capture(m->vm, game, 1));
    old_frames = frames;
    CHECK(nat_save_auto(m->vm, NULL, m, 0, NULL, &result) == 0);
    CHECK(frames == old_frames && gt4_save_dirty());
    ah_set_member(m->vm, option.u.o, "auto_save", ah_int(1));
    fail_write = 1;
    CHECK(nat_save_auto(m->vm, NULL, m, 0, NULL, &result) == 0);
    CHECK(!strcmp(ah_cstr(result), "IO_ERROR") && gt4_save_dirty());
    fail_write = 0;
    CHECK(nat_save_auto(m->vm, NULL, m, 0, NULL, &result) == 0);
    CHECK(!strcmp(ah_cstr(result), "NO_ERROR") && !gt4_save_dirty());
    CHECK(frames > old_frames);
    /* A fresh process must load the same profile through the authored load screen. */
    gt4_save_init_pending(folder);
    CHECK(!gt4_save_active() && gt4_save_pending_slot() == 2);
    old_frames = frames;
    CHECK(present_startup_load(vol));
    CHECK(frames > old_frames && gt4_save_active() == 2 && !gt4_save_pending_slot());
    CHECK(!strcmp(gt4_save_player(), "Renamed player"));
    ah_set_member(m->vm, game_object(m), "withdrawn", ah_int(0));
    ah_set_member(m->vm, game_object(m), "withdrawnGT4p", ah_int(0));
    menu_save_refresh(m);
    ah_val money, transferred;
    CHECK(ah_get_member(m->vm, game_object(m), "withdrawn", &transferred) && ah_truthy(transferred));
    CHECK(ah_get_member(m->vm, game_object(m), "withdrawnGT4p", &transferred) && ah_truthy(transferred));
    CHECK(ah_get_member(m->vm, game_object(m), "money", &money) && ah_num(money) == 54321);

    if (argc > 2)
        off_frames = 0; /* Negative: a permanently lit, non-animated icon. */
    CHECK(off_frames && !errors);
    menu_close(m);
    gt4_save_shutdown();
    gt4_vol_close(vol);
    puts(
        "PASS authored save/load icons blink during I/O; success/failure and previous save preserved");
    return 0;
}
