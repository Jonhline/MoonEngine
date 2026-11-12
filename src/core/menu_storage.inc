static int nat_save_status(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    *ret = ah_bool(data ? gt4_save_dirty() : gt4_save_active() != 0);
    return 0;
}
static int nat_save_auto(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    int ok = gt4_save_capture(vm, game_object(m), gt4_save_dirty());
    if (ok && gt4_save_dirty() && gt4_save_auto_enabled() && gt4_save_active())
        ok = menu_save_store(m);
    *ret = ah_str(vm, ok ? "NO_ERROR" : "IO_ERROR");
    return 0;
}
/* PC storage adapters for the authored acquisition sequence. These replace
   PS2 device I/O, never the verified garage/money/calendar game rules. */
/* P: local profile files replace PS2 card I/O. Trade alone chooses another
   player; ordinary garage/Arcade operations use the active profile. */
void menu_set_photo_saver(menu *m, int (*fn)(void *), void *user)
{
    if (m) {
        m->photo_saver = fn;
        m->photo_saver_user = user;
    }
}
void menu_set_profile_picker(menu *m, int (*fn)(menu *))
{
    if (m)
        m->profile_picker = fn;
}
static int nat_pc_file(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                       ah_val *ret)
{
    const char *op = data;
    ah_val slot = ah_nil();
    ah_get_member(vm, self, "_slot", &slot);
    int index = (int)ah_num(slot);
    gt4_save_info info;
    if (op && !strcmp(op, "load")) {
        int ok = menu_save_load(ah_user(vm), index);
        if (ok)
            menu_save_refresh(ah_user(vm));
        *ret = ah_str(vm, ok ? "OK" : "FILE_NOT_FOUND");
        return 0;
    }
    if (op && !strcmp(op, "saveNew")) {
        menu *m = ah_user(vm);
        int ok = gt4_save_capture(vm, game_object(m), 1) && menu_save_store(m);
        *ret = ah_str(vm, ok ? "OK" : "DEVICE_ERROR");
        return 0;
    }

    if (!op || !strcmp(op, "isExist")) {
        *ret = ah_int(gt4_save_info_get(index, &info) > 0 ? 0 : -1);
        return 0;
    }
    ah_obj *game = ah_module_new(vm, "profile_game", NULL, NULL, NULL);
    int ok = gt4_save_read_profile(vm, game, index);
    if (ok) {
        gt4_game_bind(vm, game);
        menu *m = ah_user(vm);
        ah_val garage = ah_nil(), cars = ah_nil();
        ah_get_member(vm, game, "garage", &garage);
        ah_module_attach_host(garage.u.o, &garage_ui_host);
        const char *queries[] = {"pickupCountry", "pickupTuner", "pickup"};
        for (int i = 0; i < 3; i++)
            ah_set_member(
                vm, garage.u.o, queries[i],
                ah_bind(vm, garage.u.o,
                        ah_native(vm, queries[i], nat_garage_pickup, (void *)(intptr_t)i)));
        ah_set_member(vm, garage.u.o, "_getCarName", ah_native(vm, "_getCarName", nat_car_name, m));
        ah_set_member(vm, garage.u.o, "_bindCar", ah_native(vm, "_bindCar", nat_bind_game_car, m));
        if (ah_get_member(vm, garage.u.o, "cars", &cars))
            for (int i = 0; i < ah_array_size(cars.u.o); i++) {
                ah_val car = ah_array_get(cars.u.o, i), bound;
                nat_bind_game_car(vm, NULL, m, 1, &car, &bound);
            }
    }
    ah_obj *result = ah_array_new(vm);
    ah_array_push(vm, result, ah_str(vm, ok ? "OK" : "FILE_NOT_FOUND"));
    ah_array_push(vm, result, ok ? ah_objv(game) : ah_nil());
    *ret = ah_objv(result);
    (void)argc;
    (void)argv;
    return 0;
}
static int nat_pc_manager(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_pc_check_profile(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    menu *m = data;
    ah_obj *manager = argc > 2 ? ah_as_obj(argv[2], AO_MODULE) : NULL;
    /* memorycard.ad L167..172: omitted/zero slot is the active player's
       card; GarageRoot L6749 uses it when selecting an already owned car.
       GTmode Home Trade L4297 explicitly requests the other card (slot 1).
       The PC user changes the active profile in Player Profiles separately. */
    int external = !strcmp(m->project_label, "gtmode") &&
                   argc > 3 && to_int(argv[3]) == 1;
    int slot = external && m->profile_picker ? m->profile_picker(m) : gt4_save_active();
    gt4_save_info info;
    if (external && (slot <= 0 || gt4_save_info_get(slot, &info) <= 0)) {
        *ret = ah_nil();
        return 0;
    }
    if (!manager) {
        manager = ah_module_new(vm, "native_profile_manager", NULL, NULL, NULL);
        ah_val mode[] = {ah_str(vm, "MODE_LOAD_GAME"), ah_int(external)};
        nat_pc_manager(vm, manager, m, 2, mode, ret);
    }
    ah_val file = ah_nil();
    ah_get_member(vm, manager, "_file", &file);
    ah_set_member(vm, file.u.o, "_slot", ah_int(slot));
    *ret = ah_objv(manager);
    (void)self;
    return 0;
}

/* MMemoryCardManager's statics labomode.ad calls: setAction (1 after a
   replay is loaded: the caller plays it), setExistValidGameData, and the
   photo print list (none here) */
/* Native PNG exports have no printer queue. labomode.ad L1968 still
   queries the original list.search(file) contract, rather than an array. */
static int nat_print_search(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_int(-1);
    return 0;
}
static int nat_mc_static(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    const char *op = data;
    (void)self;
    if (!strcmp(op, "setAction")) {
        replay_action = argc ? (int)ah_num(argv[0]) : 0;
        /* the Replay Theater's commandPlay: start_page(context, nil) then
           setAction(1); the replay plays over the theater and the theater
           comes back after it, so the page stays */
        if (replay_action == 1 && replay_theater) {
            menu *m = ah_user(vm);
            m->next_project[0] = 0;
        }
        if (replay_action == 1) { /* commandPlayList: the playlist, in order or shuffled */
            menu *m = ah_user(vm);
            ah_val result;
            if (ah_get_member(vm, game_object(m), "menu_result", &result) && ah_cstr(result) &&
                !strcmp(ah_cstr(result), "list")) {
                playlist_load();
                replay_queue_n = replay_queue_at = 0;
                for (int k = 0; k < playlist_count; k++)
                    memcpy(replay_queue[replay_queue_n++], playlist_names[k], 64);
                const char *mode = menu_option_text(m, "replay_play_mode", "STRAIGHT");
                if (mode && !strcmp(mode, "SHUFFLE"))
                    for (int k = replay_queue_n - 1; k > 0; k--) {
                        int j = rand() % (k + 1);
                        char t[64];
                        memcpy(t, replay_queue[k], 64);
                        memcpy(replay_queue[k], replay_queue[j], 64);
                        memcpy(replay_queue[j], t, 64);
                    }
            }
        }
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(op, "getPrintList")) {
        ah_obj *list = ah_module_new(vm, "native_print_list", NULL, NULL, NULL);
        ah_set_member(vm, list, "size", ah_int(0));
        ah_set_member(vm, list, "search", ah_native(vm, "search", nat_print_search, NULL));
        *ret = ah_objv(list);
        return 0;
    }
    *ret = ah_nil();
    return 0;
}
static ah_obj *native_profile_file(menu *m, int slot)
{
    ah_vm *vm = m->vm;
    ah_obj *file = ah_module_new(vm, "native_profile_file", NULL, NULL, NULL);
    gt4_save_info info;
    ah_set_member(vm, file, "_slot", ah_int(slot));
    ah_set_member(vm, file, "objectName", ah_str(vm, "FileGame"));
    ah_set_member(
        vm, file, "title",
        ah_str(vm, gt4_save_info_get(slot, &info) > 0 ? info.name : "New Player Profile"));
    uint8_t *bytes = NULL;
    size_t size = 0;
    if (gt4_state_pack(vm, game_object(m), &bytes, &size))
        free(bytes);
    ah_set_member(vm, file, "size", ah_int((int64_t)size));
    const char *ops[] = {"isExist", "loadInstance", "load", "saveNew"};
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++)
        ah_set_member(vm, file, ops[i],
                      ah_bind(vm, file, ah_native(vm, ops[i], nat_pc_file, (void *)ops[i])));
    return file;
}
static void native_profile_list(menu *m, ah_obj *manager)
{
    ah_obj *slots = ah_array_new(m->vm);
    gt4_save_info info;
    for (int i = 1; i <= GT4_SAVE_SLOTS; i++)
        if (gt4_save_info_get(i, &info) > 0)
            ah_array_push(m->vm, slots, ah_int(i));
    ah_set_member(m->vm, manager, "_profile_slots", ah_objv(slots));
    ah_set_member(m->vm, manager, "count", ah_int(ah_array_size(slots)));
}
#include "menu_legacy_import.inc"
static int nat_pc_manager_method(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    const char *name = data;
    menu *m = ah_user(vm);
    if (!strcmp(name, "getNewFile")) {
        ah_get_member(vm, self, "_file", ret);
        return 0;
    }
    if (!strcmp(name, "getFile")) {
        ah_val slots;
        ah_get_member(vm, self, "_profile_slots", &slots);
        ah_val slot = ah_array_get(slots.u.o, argc ? to_int(argv[0]) : -1);
        *ret = slot.t == AV_NIL ? ah_nil() : ah_objv(native_profile_file(m, to_int(slot)));
        return 0;
    }
    if (!strcmp(name, "start") || !strcmp(name, "restart") || !strcmp(name, "clear")) {
        native_profile_list(m, self);
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(name, "isNoFile")) {
        ah_val count;
        ah_get_member(vm, self, "count", &count);
        *ret = ah_bool(!ah_num(count));
        return 0;
    }
    if (!strcmp(name, "isUseNewFile")) {
        ah_val mode;
        ah_get_member(vm, self, "_mode", &mode);
        *ret = ah_bool(ah_cstr(mode) && strstr(ah_cstr(mode), "SAVE"));
        return 0;
    }
    if (!strcmp(name, "format")) {
        const char *dir = gt4_save_directory();
        *ret = ah_bool(dir && *dir && gt4_save_mkdir(dir));
        return 0;
    }
    *ret = ah_bool(strcmp(name, "isChanged") && strcmp(name, "isAutoSaveDisabled"));
    return 0;
}

/* The memory card's replay files (memcard.ad FileSelector in the modes
   MODE_LOAD_REPLAY, MODE_SAVE_FULL_REPLAY, ...), here the save folder's
   (replay_file.h, the PC layer): a card always there and formatted, its
   files the saved replays newest first, each with what the list shows
   (setItem: objectName, size in KB, date, time, title, carName). Loading
   one leaves its name for the caller; saving writes the race the caller
   lent (menu_set_replay_to_save). */
static char replay_names[256][64];
static int replay_count;
static char replay_loaded[64];
static const gt4_replay_file *replay_to_save;
void menu_set_replay_to_save(const gt4_replay_file *r)
{
    replay_to_save = r;
}
int menu_in_replay_theater(const menu *m)
{
    return m && replay_theater;
}
int menu_take_loaded_replay(char *name, size_t size)
{
    if (replay_action != 1)
        return 0;
    if (replay_queue_at < replay_queue_n) { /* the playlist's next */
        snprintf(name, size, "%s", replay_queue[replay_queue_at++]);
        if (replay_queue_at == replay_queue_n) {
            replay_action = 0;
            replay_queue_n = replay_queue_at = 0;
        }
        return 1;
    }
    if (!replay_loaded[0])
        return 0;
    replay_action = 0;
    snprintf(name, size, "%s", replay_loaded);
    replay_loaded[0] = 0;
    return 1;
}
int menu_take_loaded_ghost(char *name, size_t size)
{
    if (replay_action != 1 || strncmp(replay_loaded, "bestlap_", 8))
        return 0;
    replay_action = 0;
    snprintf(name, size, "%s", replay_loaded);
    replay_loaded[0] = 0;
    return 1;
}
static int replay_kind_matches(const char *name, const char *mode)
{
    int best = mode && strstr(mode, "BESTLAP") != NULL;
    return best ? !strncmp(name, "bestlap_", 8) : !strncmp(name, "replay_", 7);
}
static int menu_read_replay(menu *m, const char *name, gt4_replay_file *r)
{
    if (strncmp(name, "/replay/", 8) && gt4_replay_load(name, r))
        return 1;
    if (!m->disc_replays)
        m->disc_replays = gt4_disc_replay_open(m->vol);
    char error[128];
    return !strncmp(name, "/replay/", 8)
               ? gt4_disc_replay_read(m->disc_replays, name, r, error, sizeof error)
               : gt4_disc_replay_saved(m->disc_replays, name, r, error, sizeof error);
}
static void replay_refresh(menu *m, const char *mode)
{
    if (mode && !strcmp(mode, "MODE_DEMONSTRATION")) {
        replay_count = gt4_disc_replay_list(m->vol, replay_names, 256, 0);
        return;
    }
    char all[256][64];
    int n = gt4_replay_list(all, 256);
    replay_count = 0;
    if (mode &&
        strstr(
            mode,
            "PLAYLIST")) { /* the Playlist folder: the list's replays in its order, those still there */
        playlist_load();
        int kept = 0;
        for (int k = 0; k < playlist_count; k++) {
            int found = 0;
            for (int i = 0; i < n && !found; i++)
                found = !strcmp(all[i], playlist_names[k]);
            if (found) {
                memcpy(playlist_names[kept], playlist_names[k], 64);
                memcpy(replay_names[replay_count++], playlist_names[k], 64);
                kept++;
            }
        }
        playlist_count = kept;
        return;
    }
    for (int i = 0; i < n; i++)
        if (replay_kind_matches(all[i], mode))
            memcpy(replay_names[replay_count++], all[i], 64);
}
static void replay_manager_files(ah_vm *vm, ah_obj *self, const char *mode)
{
    replay_refresh(ah_user(vm), mode);
    ah_obj *files = ah_array_new(vm);
    for (int i = 0; i < replay_count; i++)
        ah_array_push(vm, files, ah_str(vm, replay_names[i]));
    ah_set_member(vm, self, "_files", ah_objv(files));
    ah_set_member(vm, self, "count", ah_int(replay_count));
}
void menu_stop_replay_queue(void)
{
    replay_queue_n = replay_queue_at = 0;
}
/* labomode.ad's update_widget: title, time, courseName (a course label
   it translates by CourseNameCaption), carName, size in KB, date (the
   PS2's dd/mm/yy), spectatorMode (B-spec), and the thumbnail
   (isIconValid / getIconTexture; disc icon and native saved picture). */
static int nat_rp_info(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                       ah_val *ret)
{
    (void)argc;
    (void)argv;
    ah_val icon;
    int has_icon = ah_get_member(vm, self, "_icon", &icon) && ah_cstr(icon) && *ah_cstr(icon);
    if (!strcmp((const char *)data, "getIconTexture"))
        *ret = has_icon ? icon : ah_str(vm, "");
    else
        *ret = ah_bool(!strcmp((const char *)data, "isInfoValid") || has_icon);
    return 0;
}
static void replay_file_fields(ah_vm *vm, ah_obj *o, const gt4_replay_file *f, size_t bytes,
                               uint64_t when)
{
    static const char *const info[] = {"isInfoValid", "isIconValid", "getIconTexture"};
    char date[32] = "", clock[16] = "", best[24] = "";
    time_t t = (time_t)when;
    struct tm *local = when ? localtime(&t) : NULL;
    if (local) {
        strftime(date, sizeof date, "%d/%m/%y", local);
        strftime(clock, sizeof clock, "%H:%M", local);
    } else
        snprintf(date, sizeof date, "--/--/--");
    if (f && f->best_lap && f->best_lap < 0x157529ffu)
        snprintf(best, sizeof best, "%u'%02u.%03u", f->best_lap / 60000, f->best_lap / 1000 % 60,
                 f->best_lap % 1000);
    ah_set_member(
        vm, o, "objectName",
        ah_str(vm, f && f->kind == GT4_REPLAY_BEST_LAP ? "FileReplayLicense" : "FileReplayFull"));
    ah_set_member(vm, o, "size", ah_int((int64_t)((bytes + 1023) / 1024)));
    ah_set_member(vm, o, "date", ah_str(vm, date));
    ah_set_member(vm, o, "time", ah_str(vm, best));
    /* the race's mode (labomode.ad's SaveReplayRoot: racemode, racesubmode,
       its title the rtext RaceModeName of the mode, the submode's number
       after it when 0 or more) */
    {
        const char *mode = !f              ? ""
                           : f->time_trial ? "ARCADE_TIME_TRIAL"
                           : f->disc_info  ? "ARCADE_SINGLE_RACE"
                           : f->prepared
                               ? (f->championship ? "GTMODE_CHAMPIONSHIP" : "GTMODE_EVENT_RACE")
                               : "ARCADE_SINGLE_RACE";
        menu *m = ah_user(vm);
        const char *name = m && *mode ? ui_translate(m->ui, 0, "RaceModeName", mode) : NULL;
        ah_set_member(vm, o, "racemode", ah_str(vm, mode));
        ah_set_member(vm, o, "racesubmode", ah_int(-1));
        ah_set_member(vm, o, "title",
                      ah_str(vm, f && f->title[0] ? f->title
                                 : name           ? name
                                        : "")); /* the player's name for it, else the mode's */
    }
    ah_set_member(vm, o, "courseName", ah_str(vm, f ? f->course : ""));
    { /* the car's name; a file without it (older) by its label's CAR_NAME */
        const char *name = !f ? "" : f->car_name[0] ? f->car_name : car_name(ah_user(vm), f->car);
        ah_set_member(vm, o, "carName", ah_str(vm, name ? name : f->car));
    }
    ah_set_member(vm, o, "spectatorMode", ah_int(0));
    ah_set_member(vm, o, "portrait", ah_int(0));
    ah_set_member(vm, o, "dispstep", ah_int(0));
    (void)clock;
    for (unsigned k = 0; k < 3; k++)
        ah_set_member(vm, o, info[k],
                      ah_bind(vm, o, ah_native(vm, info[k], nat_rp_info, (void *)info[k])));
}
static int nat_rp_file(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                       ah_val *ret)
{
    const char *op = data;
    ah_val filename;
    const char *path = ah_get_member(vm, self, "_name", &filename) ? ah_cstr(filename) : NULL;
    (void)argc;
    (void)argv;
    if (!strcmp(op, "load")) {
        gt4_replay_file f;
        if (!path || !*path) {
            *ret = ah_str(vm, "DEVICE_ERROR");
            return 0;
        }
        if (!menu_read_replay(ah_user(vm), path, &f)) {
            *ret = ah_str(vm, "INVALID_FILE");
            return 0;
        }
        gt4_replay_free(&f);
        snprintf(replay_loaded, sizeof replay_loaded, "%s", path);
        *ret = ah_str(vm, "OK");
        return 0; /* labomode.ad / memcard.ad handleLoadSaveResult: "OK" */
    }
    if (!strcmp(op, "remove")) {
        *ret = ah_bool(path && strncmp(path, "/replay/", 8) && gt4_replay_remove(path));
        return 0;
    }
    if (!strcmp(
            op,
            "saveNew")) { /* a new file, or over the one chosen (Overwrite), with the name the player gave (its title) */
        char name[64], buf[64];
        ah_val title;
        int ok = 0;
        if (replay_to_save) {
            gt4_replay_file f = *replay_to_save; /* the tape and picture lent */
            if (ah_get_member(vm, self, "title", &title) && title.t != AV_NIL)
                snprintf(f.title, sizeof f.title, "%s", ah_to_text(title, buf, sizeof buf));
            ok = path && *path ? (strncmp(path, "/replay/", 8) && gt4_replay_store_over(&f, path))
                               : gt4_replay_store(&f, name, sizeof name);
        }
        *ret = ah_str(vm, ok ? "OK" : "DEVICE_ERROR");
        return 0;
    }
    *ret = ah_bool(1);
    return 0; /* isExist, setNewFile */
}
static ah_obj *replay_file_object(ah_vm *vm, int index, const char *filename,
                                  const gt4_replay_file *f, size_t bytes, uint64_t when)
{
    static const char *const ops[] = {"load", "remove", "saveNew", "setNewFile", "isExist"};
    ah_obj *o = ah_module_new(vm, "native_replay_file", NULL, NULL, NULL);
    replay_file_fields(vm, o, f, bytes, when);
    if (f && f->disc_icon) {
        menu *m = ah_user(vm);
        char path[96];
        snprintf(path, sizeof path, "disc/icon/%s", filename ? filename : "new");
        ui_image_put(m->ui, path, 64, 64, f->disc_icon);
        ah_set_member(vm, o, "_icon", ah_str(vm, path));
    } else if (f && f->thumb) { /* its picture, an image the list's Icon names (image_chunk) */
        menu *m = ah_user(vm);
        char path[64];
        uint8_t *rgba = malloc(GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 4);
        snprintf(path, sizeof path, "pc/replay/%s", filename ? filename : "new");
        if (rgba) {
            for (int k = 0; k < GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H; k++) {
                memcpy(rgba + k * 4, f->thumb + k * 3, 3);
                rgba[k * 4 + 3] = 255;
            }
            ui_image_put(m->ui, path, GT4_REPLAY_THUMB_W, GT4_REPLAY_THUMB_H, rgba);
            free(rgba);
            ah_set_member(vm, o, "_icon", ah_str(vm, path));
        }
    }
    ah_set_member(vm, o, "_index", ah_int(index));
    if (filename)
        ah_set_member(vm, o, "_name", ah_str(vm, filename));
    for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++)
        ah_set_member(vm, o, ops[k],
                      ah_bind(vm, o, ah_native(vm, ops[k], nat_rp_file, (void *)ops[k])));
    return o;
}
static int nat_rp_manager_method(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    const char *name = data;
    ah_val mode;
    ah_get_member(vm, self, "_mode", &mode);
    if (!strcmp(name, "getFile")) {
        int i = argc ? (int)ah_num(argv[0]) : -1;
        gt4_replay_file f;
        ah_val files;
        ah_get_member(vm, self, "_files", &files);
        const char *path = ah_cstr(ah_array_get(ah_as_obj(files, AO_ARRAY), i));
        if (!path) {
            *ret = ah_nil();
            return 0;
        }
        if (menu_read_replay(ah_user(vm), path, &f)) {
            uint8_t *packed;
            size_t bytes = 0;
            if (gt4_replay_pack(&f, &packed, &bytes))
                free(packed);
            if (f.disc_info)
                bytes = *(uint32_t *)f.disc_info;
            ah_obj *file = replay_file_object(vm, i, path, &f, bytes + 32, f.when);
            *ret = ah_objv(file);
            gt4_replay_free(&f);
        } else
            *ret = ah_objv(replay_file_object(vm, i, path, NULL, 0, 0));
        return 0;
    }
    if (!strcmp(name, "getNewFile")) {
        uint8_t *packed;
        size_t bytes = 0;
        if (replay_to_save && gt4_replay_pack(replay_to_save, &packed, &bytes))
            free(packed);
        *ret = ah_objv(
            replay_file_object(vm, -1, NULL, replay_to_save, bytes + 32, (uint64_t)time(NULL)));
        return 0;
    }
    if (!strcmp(name, "start") || !strcmp(name, "restart") || !strcmp(name, "clear")) {
        replay_manager_files(vm, self, ah_cstr(mode));
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(name, "isNoFile")) {
        ah_val count;
        ah_get_member(vm, self, "count", &count);
        *ret = ah_bool(ah_num(count) == 0);
        return 0;
    }
    if (!strcmp(name, "isUseNewFile")) {
        *ret = ah_bool(ah_cstr(mode) && strstr(ah_cstr(mode), "SAVE") != NULL);
        return 0;
    }
    if (!strcmp(name, "isChanged") || !strcmp(name, "isAutoSaveDisabled")) {
        *ret = ah_bool(0);
        return 0;
    }
    *ret = ah_bool(1);
    return 0; /* isConnect, isFormat, waitPause, requestPause, clearAutoSaveDisabled */
}
static int nat_playlist_save(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    playlist_load();
    *ret = ah_str(vm, gt4_replay_playlist_write((const char(*)[64])playlist_names, playlist_count)
                          ? "OK"
                          : "DEVICE_ERROR");
    return 0;
}
static void replay_manager(ah_vm *vm, ah_obj *self, const char *mode)
{
    static const char *const methods[] = {"getFile",
                                          "getNewFile",
                                          "start",
                                          "restart",
                                          "clear",
                                          "isNoFile",
                                          "isUseNewFile",
                                          "isChanged",
                                          "isAutoSaveDisabled",
                                          "isConnect",
                                          "isFormat",
                                          "waitPause",
                                          "requestPause",
                                          "clearAutoSaveDisabled"};
    ah_obj *progress = ah_module_new(vm, "progress", NULL, NULL, NULL);
    ah_set_member(vm, progress, "value", ah_float(0));
    ah_set_member(vm, self, "progress", ah_objv(progress));
    ah_set_member(vm, self, "_mode", ah_str(vm, mode));
    replay_manager_files(vm, self, mode);
    { /* the free space in KB: the PC layer's (docs/EDITIONS.md), the disk's
           that holds the saves, shown as "Hard Disk" (ui_translate); 8000 is
           labomode's "unformatted", never given */
        uint64_t bytes = 0;
        int64_t kb;
        const char *dir = gt4_save_directory();
        kb = plat_disk_free(dir && *dir ? dir : ".", &bytes) ? (int64_t)(bytes / 1024) : 0;
        if (kb == 8000)
            kb = 7999;
        ah_set_member(vm, self, "free", ah_int(kb));
        ah_set_member(vm, self, "realFree", ah_int(kb));
    }
    ah_set_member(vm, self, "list", ah_nil()); /* the playlist labomode assigns */
    { /* the playlist file, read with the card (autoloadPlayListResult) and written by save() */
        ah_obj *autoload = ah_module_new(vm, "autoloadPlayList", NULL, NULL, NULL);
        ah_set_member(vm, autoload, "list", ah_nil());
        ah_set_member(vm, autoload, "save",
                      ah_bind(vm, autoload, ah_native(vm, "save", nat_playlist_save, NULL)));
        ah_set_member(vm, self, "autoloadPlayList", ah_objv(autoload));
        ah_set_member(vm, self, "autoloadPlayListResult", ah_str(vm, "OK"));
    }
    for (unsigned i = 0; i < sizeof methods / sizeof methods[0]; i++)
        ah_set_member(
            vm, self, methods[i],
            ah_bind(vm, self,
                    ah_native(vm, methods[i], nat_rp_manager_method, (void *)methods[i])));
}
#include "menu_album.inc"
static int nat_pc_manager(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    menu *m = data;
    {
        const char *mode = argc ? ah_cstr(argv[0]) : NULL;
        ah_set_member(vm, self, "_mode", ah_str(vm, mode ? mode : "MODE_OPERATION"));
        ah_set_member(vm, self, "_source_slot", argc > 1 ? argv[1] : ah_int(0));
        ah_set_member(vm, self, "_directory", ah_str(vm, gt4_save_directory()));
        ah_set_member(vm, self, "_storage_kind",
                      ah_str(vm, mode && strstr(mode, "PHOTO") ? "photos"
                                 : mode && (strstr(mode, "REPLAY") || strstr(mode, "DEMONSTRATION"))
                                     ? "replays"
                                     : "profiles"));
        if (mode && strstr(mode, "PHOTO")) {
            album_manager(vm, self, mode);
            *ret = ah_nil();
            return 0;
        }
        if (mode && (strstr(mode, "REPLAY") || strstr(mode, "DEMONSTRATION"))) {
            replay_manager(vm, self, mode);
            *ret = ah_nil();
            return 0;
        }
    }
    ah_set_member(vm, self, "_native_save_manager", ah_int(1));
    ah_val mode;
    ah_get_member(vm, self, "_mode", &mode);
    replay_manager(vm, self, ah_cstr(mode)); /* Native directory/free-space/status contract. */
    ah_set_member(vm, self, "_file", ah_objv(native_profile_file(m, gt4_save_active())));
    native_profile_list(m, self);
    const char *methods[] = {"getNewFile",   "getFile",      "start",
                             "restart",      "clear",        "isNoFile",
                             "isUseNewFile", "isConnect",    "isFormat",
                             "format",       "isChanged",    "isAutoSaveDisabled",
                             "waitPause",    "requestPause", "clearAutoSaveDisabled"};
    for (unsigned i = 0; i < sizeof methods / sizeof *methods; i++)
        ah_set_member(
            vm, self, methods[i],
            ah_bind(vm, self,
                    ah_native(vm, methods[i], nat_pc_manager_method, (void *)methods[i])));
    const char *imports[] = {"isExistGT3Data", "inheritGT3Data", "isExistGT4pData",
                             "inheritGT4pData"};
    for (unsigned i = 0; i < 4; i++)
        ah_set_member(
            vm, self, imports[i],
            ah_bind(vm, self, ah_native(vm, imports[i], nat_legacy_import, (void *)imports[i])));
    *ret = ah_nil();
    return 0;
}
static int nat_pc_change_car(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)data;
    ah_val method;
    if (!ah_get_member(vm, self, "_changeRidingCarNative", &method))
        return ah_error(vm, "native selection unavailable");
    ah_val marker = ah_nil();
    if (argc == 2)
        ah_get_member(vm, ah_as_obj(argv[1], AO_MODULE), "_native_save_manager", &marker);
    if (!ah_truthy(marker))
        return ah_call(vm, method, argc, argv, ret);
    /* The PC entry owns its body; no PS2 garage file needs to be loaded. */
    if (ah_call(vm, method, 1, argv, ret) < 0)
        return -1;
    *ret = ah_str(vm, "OK");
    return 0;
}

/* Complete player imports may add lists at the same directory path. */
void menu_storage_library_changed(void)
{
    playlist_read=0;
    photo_playlist_read=0;
}
