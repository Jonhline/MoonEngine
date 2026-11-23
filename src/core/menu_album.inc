/* Labomode's authored PhotoRoot / SlideRoot, backed by native PNG exports.
   PS2 card I/O (001C65B8/001C6918) is replaced at the platform boundary. */
static plat_directory_entry album_files[GT4_ALBUM_CAPACITY];
static int album_files_count;
static char album_storage[1024];
static int album_is_film(const char *name)
{
    return name && strlen(name) > 9 && !strcmp(name + strlen(name) - 9, ".ogt4film");
}
static int album_path(char *path, size_t size, const char *name)
{
    if (!name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, ".."))
        return 0;
    snprintf(path, size, "%s/%s/%s", gt4_save_directory(), album_is_film(name) ? "films" : "photos",
             name);
    return 1;
}
static int album_film(const char *name, gt4_film *film)
{
    char path[1200];
    uint8_t *bytes = NULL;
    size_t n = 0;
    int ok = album_path(path, sizeof path, name) && gt4_save_read(path, &bytes, &n, NULL) > 0 &&
             gt4_media_film_decode(bytes, n, film);
    free(bytes);
    return ok;
}
static void photo_playlist_load(void)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/photos/slideshow.list", gt4_save_directory());
    if (photo_playlist_read && !strcmp(path, album_storage))
        return;
    snprintf(album_storage, sizeof album_storage, "%s", path);
    photo_playlist_read = 1;
    photo_playlist_count = 0;
    plat_file *f = plat_file_open(path);
    if (!f)
        return;
    uint64_t size = plat_file_size(f);
    if (size <= sizeof photo_playlist_names && !(size % 64) &&
        plat_file_read_at(f, 0, photo_playlist_names, (size_t)size) == size) {
        for (int k = 0; k < (int)(size / 64); k++)
            if (memchr(photo_playlist_names[k], 0, 64) &&
                album_path(path, sizeof path, photo_playlist_names[k]))
                photo_playlist_count++;
            else
                break;
    }
    plat_file_close(f);
}
static void album_refresh(void)
{
    char dir[1024];
    snprintf(dir, sizeof dir, "%s/photos", gt4_save_directory());
    album_files_count = plat_directory_list(dir, ".png", album_files, GT4_ALBUM_CAPACITY);
    photo_playlist_load();
}
static ah_obj *album_playlist(menu *m)
{
    for (int k = 0; k < ui_root_count(m->ui); k++)
        if (!strcmp(ui_root_name(m->ui, k), "PhotoRoot")) {
            ah_val list;
            ah_obj *page = widget_module(m, ui_root_widget(m->ui, k));
            if (ah_get_member(m->vm, page, "playlist", &list) && ah_as_obj(list, AO_MODULE)) {
                ah_set_member(m->vm, list.u.o, "_photo_playlist", ah_bool(1));
                ah_set_member(m->vm, list.u.o, "size", ah_int(photo_playlist_count));
                return list.u.o;
            }
        }
    return NULL;
}
static int album_load_image(menu *m, const char *name, const char *key, int thumbnail, int *w,
                            int *h)
{
    char path[1024];
    if (!album_path(path, sizeof path, name))
        return 0;
    uint8_t *px = plat_png_read(path, w, h);
    if (!px)
        return 0;
    if (thumbnail) {
        /* P: exported PNG thumbnail; original PhotoItem handles layout/portrait. */
        uint8_t thumb_pixels[64 * 64 * 4];
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 64; x++)
                memcpy(thumb_pixels + (y * 64 + x) * 4,
                       px + ((size_t)(y * (*h) / 64) * (*w) + x * (*w) / 64) * 4, 4);
        ui_image_put(m->ui, key, 64, 64, thumb_pixels);
    } else
        ui_image_put(m->ui, key, *w, *h, px);
    free(px);
    return 1;
}
static int album_file_op(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)argc;
    (void)argv;
    const char *op = data, *name = file_name_of(vm, ah_objv(self));
    char path[1024];
    if (!strcmp(op, "saveNew")) {
        menu *m = ah_user(vm);
        int ok = m->photo_saver && m->photo_saver(m->photo_saver_user);
        *ret = ah_str(vm, ok ? "OK" : "DEVICE_ERROR");
        return 0;
    }
    if (!strcmp(op, "getInfo")) {
        *ret = ah_objv(self);
        return 0;
    }
    if (!strcmp(op, "remove")) {
        *ret = ah_bool(album_path(path, sizeof path, name) && plat_file_remove(path));
        return 0;
    }
    if (!strcmp(op, "load") || !strcmp(op, "loadData")) {
        if (album_is_film(name)) {
            gt4_film film;
            int ok = album_film(name, &film);
            if (ok && !strcmp(op, "load"))
                snprintf(film_loaded, sizeof film_loaded, "%s", name);
            *ret = ah_str(vm, ok ? "OK" : "INVALID_FILE");
            return 0;
        }
        int w, h;
        uint8_t *px = album_path(path, sizeof path, name) ? plat_png_read(path, &w, &h) : NULL;
        *ret = ah_str(vm, px ? "OK" : "INVALID_FILE");
        free(px);
        return 0;
    }
    if (!strcmp(op, "isExist")) {
        plat_file *f = album_path(path, sizeof path, name) ? plat_file_open(path) : NULL;
        *ret = ah_bool(f != NULL);
        plat_file_close(f);
        return 0;
    }
    *ret = ah_bool(1);
    return 0;
}
static ah_obj *album_file_object(menu *m, const char *name)
{
    ah_vm *vm = m->vm;
    ah_val cache_value, existing;
    ah_obj *cache = NULL;
    if (ah_get_member(vm, m->project, "_native_album_files", &cache_value))
        cache = ah_as_obj(cache_value, AO_MODULE);
    if (!cache) {
        cache = ah_module_new(vm, "native_album_files", NULL, NULL, NULL);
        ah_set_member(vm, m->project, "_native_album_files", ah_objv(cache));
    }
    if (ah_get_own_member(vm, cache, name, &existing))
        return ah_as_obj(existing, AO_MODULE);
    /* Retain decoded thumbnail/info across the authored visible/update callbacks.
    Full images still occupy only the preview and two slideshow texture slots. */
    ah_obj *o = ah_module_new(vm, "native_photo_file", NULL, NULL, NULL);
    int w = 0, h = 0;
    char key[160], date[32] = "";
    snprintf(key, sizeof key, "pc/photo/%s", name);
    gt4_film film;
    int isfilm = album_is_film(name);
    int valid = isfilm ? album_film(name, &film) : album_load_image(m, name, key, 1, &w, &h);
    if (isfilm && valid) {
        w = film.width;
        h = film.height;
    }
    ah_set_member(vm, o, "_name", ah_str(vm, name));
    ah_set_member(vm, o, "objectName", ah_str(vm, isfilm ? "FilePhotoFilm" : "FilePhotoPicture"));
    for (int k = 0; k < album_files_count; k++)
        if (!strcmp(album_files[k].name, name)) {
            time_t t = (time_t)album_files[k].modified;
            struct tm *tm = localtime(&t);
            if (tm) {
                strftime(date, sizeof date, "%d/%m/%y %H:%M", tm);
            }
            ah_set_member(vm, o, "size", ah_int((album_files[k].bytes + 1023) / 1024));
            break;
        }
    ah_set_member(vm, o, "title", ah_str(vm, name));
    ah_set_member(vm, o, "date", ah_str(vm, date));
    ah_set_member(vm, o, "time", ah_str(vm, ""));
    ah_set_member(vm, o, "carName", ah_str(vm, isfilm && valid ? film.car[0].code : ""));
    ah_set_member(vm, o, "courseName", ah_str(vm, isfilm && valid ? film.course : ""));
    ah_set_member(vm, o, "portrait", ah_int(h > w));
    ah_set_member(vm, o, "dispstep", ah_int(0));
    if (valid && !isfilm)
        ah_set_member(vm, o, "_icon", ah_str(vm, key));
    const char *info[] = {"isInfoValid", "isIconValid", "getIconTexture"};
    for (int k = 0; k < 3; k++)
        ah_set_member(vm, o, info[k],
                      ah_bind(vm, o, ah_native(vm, info[k], nat_rp_info, (void *)info[k])));
    const char *ops[] = {"getInfo", "loadData",   "load",   "remove",
                         "isExist", "setNewFile", "saveNew"};
    for (int k = 0; k < sizeof ops / sizeof *ops; k++)
        ah_set_member(vm, o, ops[k],
                      ah_bind(vm, o, ah_native(vm, ops[k], album_file_op, (void *)ops[k])));
    ah_set_member(vm, cache, name, ah_objv(o));
    return o;
}
static void album_manager_files(menu *m, ah_obj *self, const char *mode)
{
    album_refresh();
    album_playlist(m);
    ah_obj *files = ah_array_new(m->vm);
    if (mode && strstr(mode, "FILM")) {
        char dir[1100];
        snprintf(dir, sizeof dir, "%s/films", gt4_save_directory());
        album_files_count = plat_directory_list(dir, ".ogt4film", album_files, GT4_ALBUM_CAPACITY);
    }
    if (strstr(mode, "PLAYLIST")) {
        for (int k = 0; k < photo_playlist_count; k++)
            for (int i = 0; i < album_files_count; i++)
                if (!strcmp(photo_playlist_names[k], album_files[i].name)) {
                    ah_array_push(m->vm, files, ah_str(m->vm, photo_playlist_names[k]));
                    break;
                }
    } else if (!strstr(mode, "PRINT")) {
        for (int k = 0; k < album_files_count; k++)
            ah_array_push(m->vm, files, ah_str(m->vm, album_files[k].name));
    }
    ah_set_member(m->vm, self, "_files", ah_objv(files));
    ah_set_member(m->vm, self, "count", ah_int(ah_array_size(files)));
}
static int album_manager_op(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    menu *m = ah_user(vm);
    const char *op = data;
    ah_val mode, files;
    ah_get_member(vm, self, "_mode", &mode);
    if (!strcmp(op, "getNewFile")) {
        *ret = ah_objv(album_file_object(m, ""));
        return 0;
    }
    if (!strcmp(op, "isUseNewFile")) {
        *ret = ah_bool(ah_cstr(mode) && strstr(ah_cstr(mode), "SAVE"));
        return 0;
    }
    if (!strcmp(op, "getFile")) {
        ah_get_member(vm, self, "_files", &files);
        const char *name =
            ah_cstr(ah_array_get(ah_as_obj(files, AO_ARRAY), argc ? to_int(argv[0]) : -1));
        *ret = name ? ah_objv(album_file_object(m, name)) : ah_nil();
        return 0;
    }
    if (!strcmp(op, "start") || !strcmp(op, "restart") || !strcmp(op, "clear")) {
        album_manager_files(m, self, ah_cstr(mode));
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(op, "isNoFile")) {
        ah_val count;
        ah_get_member(vm, self, "count", &count);
        *ret = ah_bool(!ah_num(count));
        return 0;
    }
    *ret = ah_bool(strcmp(op, "isChanged") && strcmp(op, "isAutoSaveDisabled") &&
                   strcmp(op, "isUseNewFile"));
    return 0;
}
static int album_playlist_save(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    photo_playlist_load();
    *ret = ah_str(
        vm, plat_file_write(album_storage, photo_playlist_names, (size_t)photo_playlist_count * 64)
                ? "OK"
                : "DEVICE_ERROR");
    return 0;
}
static void album_manager(ah_vm *vm, ah_obj *self, const char *mode)
{
    menu *m = ah_user(vm);
    replay_manager(vm, self, mode); /* shared card status/progress only */
    const char *ops[] = {"getFile",     "getNewFile", "start",     "restart",
                         "clear",       "isNoFile",   "isChanged", "isAutoSaveDisabled",
                         "isUseNewFile"};
    for (int k = 0; k < sizeof ops / sizeof *ops; k++)
        ah_set_member(vm, self, ops[k],
                      ah_bind(vm, self, ah_native(vm, ops[k], album_manager_op, (void *)ops[k])));
    ah_val autoload;
    ah_get_member(vm, self, "autoloadPlayList", &autoload);
    ah_set_member(vm, autoload.u.o, "save",
                  ah_bind(vm, autoload.u.o, ah_native(vm, "save", album_playlist_save, NULL)));
    album_manager_files(m, self, mode);
    ah_set_member(vm, self, "list", ah_objv(album_playlist(m)));
}
static int album_next(menu *m, int slot)
{
    for (int tries = 0; tries < m->album_count; tries++) {
        int i = m->album_order[m->album_at++];
        if (m->album_at >= m->album_count)
            m->album_at = 0;
        char key[32];
        snprintf(key, sizeof key, "pc/slide/%d", slot);
        int w, h;
        if (album_load_image(m, photo_playlist_names[i], key, 0, &w, &h)) {
            /* 001AAFD0 +60..78 and 001C5E88 +A4..B8 draw direction, then zoom.
      P: timer seed belongs to the native host; same original RNG thereafter. */
            int reverse = gt4_pcm_music_random_unit(&m->album_random) < .5f;
            float random1 = gt4_pcm_music_random_unit(&m->album_random),
                  random2 = gt4_pcm_music_random_unit(&m->album_random);
            gt4_album_slide_init(
                &m->album_layer[slot], w, h, (int)menu_option_number(m, "album_slide_play_time", 8),
                (int)menu_option_number(m, "album_slide_transition_time", 4),
                menu_option_number(m, "album_slide_effect", 1) != 0, reverse, random1, random2);
            fprintf(stderr, "album slide: %s (%dx%d)\n", photo_playlist_names[i], w, h);
            return 1;
        }
    }
    return 0;
}
static int nat_album_widget(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    const char *op = data;
    *ret = ah_nil();
    if (!strcmp(op, "outUSBStorage")) {
        int ok = m->photo_saver && m->photo_saver(m->photo_saver_user);
        *ret = ah_str(vm, ok ? "OK" : "DEVICE_ERROR");
        return 0;
    }
    if (!strcmp(op, "doStop")) {
        m->album_playing = 0;
        return 0;
    }
    if (!strcmp(op, "doPlay")) {
        album_refresh();
        m->album_count = photo_playlist_count;
        m->album_at = 0;
        m->album_front = 0;
        /* P: host wall clock seeds the original RNG; PS2 clock seed is unavailable. */
        if (!m->album_random)
            m->album_random = (uint32_t)time(NULL);
        for (int k = 0; k < m->album_count; k++)
            m->album_order[k] = k;
        /* 001C6BE8 +B0..108 uses the selected order until the next play. */
        if (m->album_shuffle)
            gt4_album_slide_shuffle(m->album_order, m->album_count, &m->album_random);
        memset(m->album_layer, 0, sizeof m->album_layer);
        m->album_layer[1].state = 5;
        m->album_playing = m->album_count && album_next(m, 0);
        return 0;
    }
    if (!strcmp(op, "setPhotoFile")) {
        const char *name = argc ? file_name_of(vm, argv[0]) : NULL;
        if (!name || !album_load_image(m, name, "pc/album/preview", 0, &m->album_preview_w,
                                       &m->album_preview_h))
            return ah_error(vm, "album photo unavailable");
        m->photo.image_width = m->album_preview_w;
        m->photo.image_height = m->album_preview_h;
        m->photo.image_scale = 1;
        return 0;
    }
    /* PNG exports are already developed pictures; allocation/export are host I/O. */
    return 0;
}
static void album_tick(menu *m)
{
    if (!m->album_playing)
        return;
    if (strcmp(ui_root_name(m->ui, menu_active_root(m)), "SlideRoot")) {
        m->album_playing = 0;
        return;
    }
    int front = m->album_front, back = 1 - front;
    /* 001AAFD0/001C7358 double buffers: new slide starts when front is
    waiting and back is free. Fading the outgoing layer advances both. */
    if (m->album_layer[front].state == 2 && m->album_layer[back].state == 5) {
        if (album_next(m, back)) {
            gt4_album_slide_out(&m->album_layer[front]);
            m->album_front = back;
        } else
            m->album_playing = 0;
    }
    gt4_album_slide_tick(&m->album_layer[0]);
    gt4_album_slide_tick(&m->album_layer[1]);
}
static int album_draw(void *user, const ui_widget *widget, float x, float y, float w, float h,
                      float alpha)
{
    menu *m = user;
    float v[4] = {x, y, w, h};
    uint8_t black[4] = {0, 0, 0, (uint8_t)(255 * alpha)};
    plat_draw_quad(NULL, x, y, w, h, 0, 0, 1, 1, black);
    if (!strcmp(widget->type, "SlideShowFace")) {
        for (int k = 0; k < 2; k++) {
            int slot = k ? m->album_front : 1 - m->album_front;
            gt4_album_rect r;
            if (gt4_album_slide_rect(&m->album_layer[slot], v, &r)) {
                char key[32];
                snprintf(key, sizeof key, "pc/slide/%d", slot);
                ui_image_draw(m->ui, key, r.x, r.y, r.w, r.h, alpha * r.alpha / 255.f);
            }
        }
        return 1;
    }
    if (m->album_preview_w > 0 && m->album_preview_h > 0) {
        float scale = fminf(w / m->album_preview_w, h / m->album_preview_h) * m->photo.image_scale;
        float iw = m->album_preview_w * scale, ih = m->album_preview_h * scale;
        ui_image_draw(m->ui, "pc/album/preview", x + (w - iw) * .5f + m->photo.image_x,
                      y + (h - ih) * .5f + m->photo.image_y, iw, ih, alpha);
        return 1;
    }
    return 0;
}

static int album_music_list(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ah_obj *list = ah_array_new(vm);
    unsigned n = gt4_pcm_music_count(m->vol, "racing");
    for (unsigned i = 0; i < n; i++) {
        char info[3][128];
        if (!gt4_pcm_music_info(m->vol, i, info))
            continue;
        ah_obj *row = ah_array_new(vm);
        for (int k = 0; k < 3; k++)
            ah_array_push(vm, row, ah_str(vm, info[k]));
        ah_array_push(vm, row, ah_int(i));
        ah_array_push(vm, list, ah_objv(row));
    }
    *ret = ah_objv(list);
    return 0;
}
/* The Jukebox (gtmode MusicTheater -> slide.mproject MusicRoot): the race
   playlist the race plays (option.bgm_playlist, kept as the save's
   bgm_playlist_ps2, 004379F0), the library's order (config-main
   init_baseplaylist, main.default_library_playlist) and the replay's
   music volume. Rows [original, used, enabled, order] as 0019E900 maps
   them, as the slide playlist above. */
static gt4_music_playlist jukebox_playlist;
static int jukebox_loaded;
static menu *jukebox_menu; /* the slide project's menu: its option is bound again on refresh */
static void jukebox_bind(menu *m)
{
    jukebox_menu = m;
    ah_vm *vm = m->vm;
    ah_obj *game = game_object(m);
    ah_val option, main_;
    ah_get_member(vm, game, "option", &option);
    jukebox_loaded = gt4_save_option_blob_get("bgm_playlist_ps2", jukebox_playlist.bytes,
                                              sizeof jukebox_playlist.bytes) ||
                     gt4_music_playlist_default(&jukebox_playlist, m->vol, 0);
    if (jukebox_loaded) {
        ah_obj *playlist = ah_module_new(vm, "bgm_playlist", NULL, NULL, NULL),
               *rows = ah_array_new(vm);
        for (unsigned i = 0; i < jukebox_playlist.bytes[0] && i < 128; i++) {
            const uint8_t *e = jukebox_playlist.bytes + 16 + i * 16;
            ah_obj *row = ah_array_new(vm);
            ah_array_push(vm, row, ah_int(e[11]));
            ah_array_push(vm, row, ah_int(e[8]));
            ah_array_push(vm, row, ah_int(e[9]));
            ah_array_push(vm, row, ah_int(e[10]));
            ah_array_push(vm, rows, ah_objv(row));
        }
        ah_set_member(vm, playlist, "play_data", ah_objv(rows));
        ah_set_member(vm, playlist, "shuffle", ah_int(jukebox_playlist.bytes[6]));
        ah_set_member(vm, option.u.o, "bgm_playlist", ah_objv(playlist));
    }
    ah_set_member(vm, option.u.o, "replay_bgm_volume",
                  ah_float((float)menu_option_number(m, "replay_bgm_volume_raw", 120) / 128.f));
    unsigned library[256], n = gt4_music_library_default(m->vol, library, 256);
    ah_obj *list = ah_array_new(vm);
    for (unsigned i = 0; i < n; i++)
        ah_array_push(vm, list, ah_int(library[i]));
    if (ah_get_member(vm, ah_root(vm), "main", &main_))
        ah_set_member(vm, ah_as_obj(main_, AO_MODULE), "default_library_playlist", ah_objv(list));
}
/* the race playlist as the Jukebox left it, back into the save */
static void jukebox_store(menu *m)
{
    if (m->jukebox_external)
        return;
    ah_vm *vm = m->vm;
    ah_val option, playlist, rows, shuffle;
    if (!jukebox_loaded || !ah_get_member(vm, game_object(m), "option", &option) ||
        !ah_get_member(vm, ah_as_obj(option, AO_MODULE), "bgm_playlist", &playlist) ||
        !ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "play_data", &rows))
        return;
    ah_obj *a = ah_as_obj(rows, AO_ARRAY);
    if (!a)
        return;
    for (int i = 0; i < ah_array_size(a) && i < jukebox_playlist.bytes[0] && i < 128; i++) {
        ah_obj *row = ah_as_obj(ah_array_get(a, i), AO_ARRAY);
        if (!row || ah_array_size(row) != 4)
            continue;
        uint8_t *e = jukebox_playlist.bytes + 16 + i * 16;
        e[11] = (uint8_t)ah_num(ah_array_get(row, 0));
        e[8] = (uint8_t)ah_num(ah_array_get(row, 1));
        e[9] = (uint8_t)ah_num(ah_array_get(row, 2));
        e[10] = (uint8_t)ah_num(ah_array_get(row, 3));
    }
    if (ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "shuffle", &shuffle))
        jukebox_playlist.bytes[6] = (uint8_t)ah_truthy(shuffle);
    gt4_save_option_blob_set("bgm_playlist_ps2", jukebox_playlist.bytes,
                             sizeof jukebox_playlist.bytes);
}

static int jukebox_external_list(ah_vm *vm, ah_obj *self, void *data, int argc,
                                  const ah_val *argv, ah_val *ret)
{
    (void)self; (void)data; (void)argc; (void)argv;
    *ret = ah_nil();
    ah_get_member(vm, game_object(ah_user(vm)), "_local_music", ret);
    return 0;
}
int menu_jukebox_catalog(menu *m,const menu_music_entry *entries,int count,const int shuffle[2])
{
    /* Before MusicRoot is entered: a cached slide project is bound again on each visit. */
    int root=menu_active_root(m);
    if(!m || count<0 || count>256 || (count && !entries) || (root>=0 && m->entered[root]))return 0;
    ah_vm *vm=m->vm;ah_obj *game=game_object(m),*library=ah_array_new(vm),*indices=ah_array_new(vm);
    ah_val option,main_;ah_get_member(vm,game,"option",&option);
    for(int i=0;i<count;i++){
        ah_obj *row=ah_array_new(vm);
        ah_array_push(vm,row,ah_str(vm,entries[i].title?entries[i].title:""));
        ah_array_push(vm,row,ah_str(vm,entries[i].artist?entries[i].artist:""));
        ah_array_push(vm,row,ah_str(vm,entries[i].genre?entries[i].genre:""));
        ah_array_push(vm,row,ah_int(i));ah_array_push(vm,library,ah_objv(row));
        ah_array_push(vm,indices,ah_int(i));
    }
    ah_set_member(vm,game,"_local_music",ah_objv(library));
    ah_set_member(vm,game,"getAllMusicList",ah_native(vm,"getAllMusicList",jukebox_external_list,NULL));
    if(ah_get_member(vm,ah_root(vm),"main",&main_))
        ah_set_member(vm,ah_as_obj(main_,AO_MODULE),"default_library_playlist",ah_objv(indices));
    const char *keys[]={"bgm_playlist","slide_playlist"};
    for(int k=0;k<2;k++){
        ah_obj *playlist=ah_module_new(vm,keys[k],NULL,NULL,NULL),*rows=ah_array_new(vm);
        for(int i=0;i<count;i++){
            ah_obj *row=ah_array_new(vm);int at=entries[i].order[k];
            ah_array_push(vm,row,ah_int(at));ah_array_push(vm,row,ah_int(0));
            ah_array_push(vm,row,ah_int(at>=0));ah_array_push(vm,row,ah_int(at));
            ah_array_push(vm,rows,ah_objv(row));
        }
        ah_set_member(vm,playlist,"play_data",ah_objv(rows));
        ah_set_member(vm,playlist,"shuffle",ah_int(shuffle?!!shuffle[k]:0));
        ah_set_member(vm,ah_as_obj(option,AO_MODULE),keys[k],ah_objv(playlist));
    }
    m->jukebox_external=1;return 1;
}
int menu_jukebox_external(menu *m,const char *const *titles,const int *enabled,int count,int shuffle)
{
    if(count<0 || count>256 || (count && (!titles || !enabled)))return 0;
    menu_music_entry entries[256];int shuffles[2]={shuffle,shuffle};
    for(int i=0;i<count;i++)entries[i]=(menu_music_entry){titles[i],"Local music","",{enabled[i]?i:-1,enabled[i]?i:-1}};
    return menu_jukebox_catalog(m,entries,count,shuffles);
}
int menu_jukebox_playlist_result(menu *m,int selection,int *order,int *enabled,int capacity,int *shuffle)
{
    if (!m || !m->jukebox_external || selection<0 || selection>1 || capacity < 0 || !order || !enabled)
        return -1;
    /* Finalize through the authored script: applies live music_map/list edits. */
    if (menu_active_root(m) >= 0 && m->entered[menu_active_root(m)] &&
        !strcmp(ui_root_name(m->ui, menu_active_root(m)), "MusicRoot"))
        finalize_page(m, menu_active_root(m));
    ah_val option, playlist, rows, value;
    ah_vm *vm = m->vm;
    if (!ah_get_member(vm, game_object(m), "option", &option) ||
        !ah_get_member(vm, ah_as_obj(option, AO_MODULE), selection?"slide_playlist":"bgm_playlist", &playlist) ||
        !ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "play_data", &rows))
        return -1;
    ah_obj *array = ah_as_obj(rows, AO_ARRAY);
    if (!array) return -1;
    int n = ah_array_size(array);
    if (n > capacity || n>256) return -1;
    for (int i = 0; i < n; i++) {
        ah_obj *row = ah_as_obj(ah_array_get(array, i), AO_ARRAY);
        if (!row || ah_array_size(row) != 4) return -1;
        enabled[i] = ah_truthy(ah_array_get(row, 2));
        order[i] = -1;
    }
    int used[256] = {0}, out = 0;
    for (int at = 0; at < n; at++)
        for (int i = 0; i < n; i++) {
            ah_obj *row = ah_as_obj(ah_array_get(array, i), AO_ARRAY);
            if (enabled[i] && (int)ah_num(ah_array_get(row, 0)) == at && !used[i]) {
                order[out++] = i;
                used[i] = 1;
            }
        }
    for (int i = 0; i < n; i++)
        if (!used[i]) order[out++] = i;
    if (shuffle && ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "shuffle", &value))
        *shuffle = ah_truthy(value);
    return n;
}

int menu_jukebox_external_result(menu *m,int *order,int *enabled,int capacity,int *shuffle)
{return menu_jukebox_playlist_result(m,0,order,enabled,capacity,shuffle);}

/* P: Menu and Replay folders (docs/OPTIONS.md audio.jukebox_folders). Two
   more rows in MusicRoot's own folder column, run by the authored playlist
   code: music_list/music_map gain modes 3 and 4, and every add, remove and
   reorder goes through append_music_list(mode,..), list_box.append_playlist
   (mode) and remove_playlist(context,mode) as for the race and slideshow
   folders. Only set_mode_icons (mode_table_list has three rows) and
   mselect_callback (Append1/Append2/Trash) name the three folders; they run
   as authored for those and are extended for the two new ones. Pictures are
   the disc's: gtmode HomeRoot's option.png and labomode's folder_replay.png
   for the folders, labomode's to_list_mov for adding to replays, and
   to_list_slide_bgm with the note replaced by four squares, in the button's
   own two colours, for adding to the menu. */
static const char *const jukebox_folder_row[2] = {"PlayList3", "PlayList4"};
static const char *const jukebox_folder_add[2] = {"Append3", "Append4"};
static const char *const jukebox_folder_label[2] = {"Menu", "Replay"};
/* sidebar and button column, authored and with the two folders */
static const char *const jukebox_moved[] = {"PlayList1", "PlayList2", "VisualEffect_library"};
static const float jukebox_moved_y[2][3] = {{147, 227, 273}, {135, 203, 317}};

static void jukebox_menu_icon(menu *m, int root, const char *from, const char *to)
{
    size_t size;
    uint8_t *data = ui_load_asset(m->ui, root, from, &size);
    tex1_image *im = NULL;
    int n = data ? tex1_decode(data, size, &im) : -1;
    free(data);
    if (n > 0 && im[0].width >= 30 && im[0].height >= 31) {
        int w = im[0].width;
        uint8_t *px = im[0].rgba, dark[4], light[4];
        static const int tiles[4][4] = {
            {17, 22, 21, 25}, {24, 29, 21, 25}, {17, 22, 27, 30}, {24, 29, 27, 30}};
        memcpy(dark, px + (22 * w + 15) * 4, 4);
        memcpy(light, px + (28 * w + 12) * 4, 4);
        for (int y = 20; y < 31; y++)
            for (int x = 16; x < 30; x++)
                memcpy(px + (y * w + x) * 4, light, 4);
        for (int t = 0; t < 4; t++)
            for (int y = tiles[t][2]; y < tiles[t][3]; y++)
                for (int x = tiles[t][0]; x < tiles[t][1]; x++)
                    memcpy(px + (y * w + x) * 4, dark, 4);
        ui_image_put(m->ui, to, w, im[0].height, px);
    }
    tex1_free(im, n);
}

static void jukebox_folder_images(ui_widget *w, const char *normal, const char *over)
{
    if (w->image_path)
        ui_widget_set_image(w, strstr(w->image_path, "_over") ? over : normal);
    for (int i = 0; i < w->nkids; i++)
        jukebox_folder_images(w->kids[i], normal, over);
}

/* the two rows and buttons, cloned once into the cached page */
static void jukebox_folder_widgets(menu *m, ui_widget *page)
{
    ui_widget *row = ui_widget_child(page, "PlayList2"), *add = ui_widget_child(page, "Append2");
    if (!row || !add || ui_widget_child(page, jukebox_folder_row[0]))
        return;
    ui_load_package(m->ui, "/projects/labomode", "SaveReplayRoot");
    ui_load_package(m->ui, "/projects/labomode", "Template");
    ui_load_package(m->ui, "/projects/gtmode", "HomeRoot");
    jukebox_menu_icon(m, page->root, "image/labo/to_list_slide_bgm.png",
                      "moon/jukebox/to_list_menu.png");
    jukebox_menu_icon(m, page->root, "image/labo/to_list_slide__bgm_over.png",
                      "moon/jukebox/to_list_menu_over.png");
    static const char *const bg[2] = {"image/gtmode/home/option.png",
                                      "image/labo/folder_replay.png"};
    static const char *const icon[2][2] = {
        {"moon/jukebox/to_list_menu.png", "moon/jukebox/to_list_menu_over.png"},
        {"image/labo/to_list_mov.png", "image/labo/to_list_mov_over.png"}};
    for (int k = 0; k < 2; k++) {
        ui_widget *r = ui_widget_copy(m->ui, row), *a = ui_widget_copy(m->ui, add);
        if (!r || !a)
            return;
        r->name = jukebox_folder_row[k];
        a->name = jukebox_folder_add[k];
        /* no TOOLTIP| entry: the help line is only the one onFocusEnter writes */
        r->tooltip_key = jukebox_folder_row[k];
        a->tooltip_key = jukebox_folder_add[k];
        r->y = 271 + 68 * k;
        a->y = 233 + 42 * k;
        ui_widget *face = ui_widget_child(r, "bg"), *label = ui_widget_child(r, "label");
        if (face) {
            ui_widget_set_image(face, bg[k]);
            if (k == 0) { /* 94x88, fitted to the folder's height */
                float w = face->h * 94 / 88;
                face->x += (face->w - w) / 2;
                face->w = w;
            }
        }
        if (label) {
            free(label->text);
            free(label->text_key);
            label->text = strdup(jukebox_folder_label[k]);
            label->text_key = NULL;
        }
        jukebox_folder_images(a, icon[k][0], icon[k][1]);
        ui_widget_append(page, r);
        ui_widget_append(page, a);
        bind_tree(m, r);
        bind_tree(m, a);
    }
}

ui_widget *menu_jukebox_page_widget(menu *m)
{
    return m && m->jukebox_root ? ah_host_data(m->jukebox_root) : NULL;
}
static ui_widget *jukebox_page_child(menu *m, const char *name)
{
    return ui_widget_child(ah_host_data(m->jukebox_root), name);
}
static void jukebox_attr(menu *m, ui_widget *w, const char *attr, int v)
{
    if (w)
        ah_set_member(m->vm, widget_module(m, w), attr, ah_bool(v));
}
static int jukebox_mode(menu *m)
{
    ah_val v;
    return ah_call_member(m->vm, m->jukebox_root, "get_mode", 0, NULL, &v) == 1 ? (int)ah_num(v) : 0;
}

/* set_mode_icons (slide.ad L353..375): mode_table_list's rows, a fourth for
   the new folders as for a playlist folder with no cursor of its own */
static int jukebox_mode_icons(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    menu *m = data;
    int mode = jukebox_mode(m);
    *ret = ah_nil();
    if (mode < 3) {
        ah_val authored;
        if (ah_get_own_member(vm, m->jukebox_root, "_set_mode_icons_ps2", &authored) &&
            ah_call(vm, authored, argc, argv, ret) < 0)
            return -1;
    } else {
        static const char *const names[] = {"Append1", "Append2", "Trash", "Library", "PlayList1",
                                            "PlayList2", "VisualEffect", "VisualEffect_library",
                                            "Dot0"};
        static const int row[] = {0, 0, 1, 0, 0, 0, 1, 0, 1};
        for (int i = 0; i < 9; i++) {
            ui_widget *w = jukebox_page_child(m, names[i]);
            jukebox_attr(m, i >= 3 && i <= 5 && w ? ui_widget_child(w, "cursor") : w, "visible",
                         row[i]);
        }
        for (int i = 0; i < 2; i++) {
            ui_widget *w = jukebox_page_child(m, names[4 + i]);
            jukebox_attr(m, w ? ui_widget_child(w, "to_race_arrow") : NULL, "visible", 0);
        }
    }
    /* Dot1 fills the fourth button place, Append3's in the Library */
    jukebox_attr(m, jukebox_page_child(m, "Dot1"), "visible", mode != 0);
    for (int k = 0; k < 2; k++) {
        ui_widget *r = jukebox_page_child(m, jukebox_folder_row[k]);
        jukebox_attr(m, jukebox_page_child(m, jukebox_folder_add[k]), "visible", mode == 0);
        jukebox_attr(m, r ? ui_widget_child(r, "cursor") : NULL, "visible", mode == 3 + k);
        jukebox_attr(m, r ? ui_widget_child(r, "to_race_arrow") : NULL, "visible", mode == 0);
    }
    return 0;
}

/* mselect_callback (L400..402): the add buttons follow the selection */
static int jukebox_mselect(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    menu *m = data;
    ah_val authored;
    *ret = ah_nil();
    if (ah_get_own_member(vm, m->jukebox_root, "_mselect_callback_ps2", &authored) &&
        ah_call(vm, authored, argc, argv, ret) < 0)
        return -1;
    int on = argc > 0 && ah_truthy(argv[0]);
    for (int k = 0; k < 2; k++) {
        ui_widget *w = jukebox_page_child(m, jukebox_folder_add[k]);
        jukebox_attr(m, w, "active", on);
        jukebox_attr(m, w, "can_focus", on);
    }
    return 0;
}

static int jukebox_folder_index(menu *m, ah_obj *self, const char *const names[2])
{
    for (int k = 0; k < 2; k++)
        if (ah_host_data(self) == jukebox_page_child(m, names[k]))
            return k;
    return 0;
}

/* The Racing folder's own text with the folder's name in place of "Racing"
   ("Racing playlist/No. of songs:" -> "Menu playlist/No. of songs:") */
static void jukebox_folder_text(menu *m, const char *key, int k, char *out, size_t size)
{
    const char *text = ui_translate(m->ui, ((ui_widget *)ah_host_data(m->jukebox_root))->root,
                                    NULL, key),
               *word = text ? strstr(text, "Racing") : NULL;
    if (word)
        snprintf(out, size, "%.*s%s%s", (int)(word - text), text, jukebox_folder_label[k],
                 word + strlen("Racing"));
    else
        snprintf(out, size, "%s playlist", jukebox_folder_label[k]);
}

/* PlayList1.onFocusEnter (L1421): "%s%d" of the folder's text and its count */
static int jukebox_folder_focus(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    (void)argc; (void)argv;
    menu *m = data;
    int k = jukebox_folder_index(m, self, jukebox_folder_row);
    ah_val lists;
    char text[160], line[176];
    *ret = ah_nil();
    ah_get_member(vm, m->jukebox_root, "music_list", &lists);
    jukebox_folder_text(m, "TOOLTIP|FolderRace", k, text, sizeof text);
    snprintf(line, sizeof line, "%s%d", text,
             ah_array_size(ah_as_obj(ah_array_get(ah_as_obj(lists, AO_ARRAY), 3 + k), AO_ARRAY)));
    ah_val arg = ah_str(vm, line), r;
    return ah_call_member(vm, m->jukebox_root, "set_tooltip", 1, &arg, &r) < 0 ? -1 : 0;
}

/* PlayList1.onActivate (L1426) */
static int jukebox_folder_open(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    menu *m = data;
    ah_val args[2] = {argc ? argv[0] : ah_nil(),
                      ah_int(3 + jukebox_folder_index(m, self, jukebox_folder_row))};
    *ret = ah_int(1);
    return ah_call_member(vm, m->jukebox_root, "set_mode", 2, args, ret) < 0 ? -1 : 0;
}

/* Append1.onActivate (L1451..1459) */
static int jukebox_folder_add_activate(ah_vm *vm, ah_obj *self, void *data, int argc,
                                       const ah_val *argv, ah_val *ret)
{
    (void)argc; (void)argv;
    menu *m = data;
    ah_val active, sound, list, box, r;
    *ret = ah_int(2);
    int ok = ah_get_member(vm, self, "active", &active) && ah_truthy(active);
    sound = ah_str(vm, ok ? "ok" : "disable");
    if (ah_call_member(vm, m->jukebox_root, "seplay", 1, &sound, &r) < 0)
        return -1;
    if (ok && ah_get_member(vm, m->jukebox_root, "List", &list) &&
        ah_get_member(vm, ah_as_obj(list, AO_MODULE), "list_box", &box)) {
        ah_val mode = ah_int(3 + jukebox_folder_index(m, self, jukebox_folder_add));
        if (ah_call_member(vm, ah_as_obj(box, AO_MODULE), "append_playlist", 1, &mode, &r) < 0)
            return -1;
    }
    return 0;
}

/* Append1's TOOLTIP line ("Add to Racing playlist") for the folder */
static int jukebox_folder_add_focus(ah_vm *vm, ah_obj *self, void *data, int argc,
                                    const ah_val *argv, ah_val *ret)
{
    (void)argc; (void)argv;
    menu *m = data;
    char line[160];
    jukebox_folder_text(m, "TOOLTIP|Append1", jukebox_folder_index(m, self, jukebox_folder_add),
                        line, sizeof line);
    ah_val arg = ah_str(vm, line), r;
    *ret = ah_nil();
    return ah_call_member(vm, m->jukebox_root, "set_tooltip", 1, &arg, &r) < 0 ? -1 : 0;
}

static void jukebox_replace(menu *m, ah_obj *page, const char *name, const char *keep,
                            ah_native_fn fn)
{
    ah_val authored;
    if (!ah_get_own_member(m->vm, page, keep, &authored) &&
        ah_get_own_member(m->vm, page, name, &authored))
        ah_set_member(m->vm, page, keep, authored);
    if (fn)
        ah_set_member(m->vm, page, name, ah_native(m->vm, name, fn, m));
    else if (ah_get_own_member(m->vm, page, keep, &authored))
        ah_set_member(m->vm, page, name, authored);
}

/* Before MusicRoot.onInitialize: the folders shown or the authored page back */
static void jukebox_folders_layout(menu *m, ui_widget *page)
{
    int on = m->jukebox_folders;
    if (on)
        jukebox_folder_widgets(m, page);
    if (!ui_widget_child(page, jukebox_folder_row[0]))
        return;
    m->jukebox_root = widget_module(m, page);
    for (int i = 0; i < 3; i++) {
        ui_widget *w = ui_widget_child(page, jukebox_moved[i]);
        if (w)
            w->y = jukebox_moved_y[on][i];
    }
    for (int k = 0; k < 2; k++) {
        ui_widget *r = ui_widget_child(page, jukebox_folder_row[k]),
                  *a = ui_widget_child(page, jukebox_folder_add[k]);
        r->visible = a->visible = on;
        if (on) {
            menu_native_method(m, r, "onActivate", jukebox_folder_open, m);
            menu_native_method(m, r, "onFocusEnter", jukebox_folder_focus, m);
            menu_native_method(m, a, "onActivate", jukebox_folder_add_activate, m);
            menu_native_method(m, a, "onFocusEnter", jukebox_folder_add_focus, m);
        }
    }
    jukebox_attr(m, ui_widget_child(page, "Dot1"), "visible", 1);
    jukebox_replace(m, m->jukebox_root, "set_mode_icons", "_set_mode_icons_ps2",
                    on ? jukebox_mode_icons : NULL);
    jukebox_replace(m, m->jukebox_root, "mselect_callback", "_mselect_callback_ps2",
                    on ? jukebox_mselect : NULL);
}

/* After onInitialize's init_music_list (L311..329): the folders' lists */
static void jukebox_folders_fill(menu *m)
{
    ah_vm *vm = m->vm;
    ah_val lists, maps;
    if (!m->jukebox_folders || !m->jukebox_root ||
        !ah_get_member(vm, m->jukebox_root, "music_list", &lists) ||
        !ah_get_member(vm, m->jukebox_root, "music_map", &maps))
        return;
    ah_obj *list = ah_as_obj(lists, AO_ARRAY), *map = ah_as_obj(maps, AO_ARRAY),
           *base = map ? ah_as_obj(ah_array_get(map, 0), AO_ARRAY) : NULL;
    if (!list || !base || ah_array_size(list) != 3)
        return;
    for (int k = 0; k < 2; k++) {
        ah_obj *zero = ah_array_new(vm);
        for (int i = 0; i < ah_array_size(base); i++)
            ah_array_push(vm, zero, ah_int(0));
        ah_array_push(vm, list, ah_objv(ah_array_new(vm)));
        ah_array_push(vm, map, ah_objv(zero));
    }
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < m->jukebox_folder_count[k]; i++) {
            ah_val args[2] = {ah_int(3 + k), ah_int(m->jukebox_folder_ids[k][i])}, r;
            ah_call_member(vm, m->jukebox_root, "append_music_list", 2, args, &r);
        }
}

int menu_jukebox_retitle(menu *m, int row, const char *title)
{
    if (!m || row < 0 || !title) return 0;
    ah_val v;
    if (!ah_get_member(m->vm, game_object(m), "_local_music", &v)) return 0;
    ah_obj *lib = ah_as_obj(v, AO_ARRAY);
    if (!lib || row >= ah_array_size(lib)) return 0;
    ah_obj *item = ah_as_obj(ah_array_get(lib, row), AO_ARRAY);
    if (!item || ah_array_size(item) < 1) return 0;
    ah_array_set(item, 0, ah_str(m->vm, title));
    return 1;
}
int menu_jukebox_drop_row(menu *m, int row)
{
    if (!m || row < 0) return 0;
    ah_val v;
    if (!ah_get_member(m->vm, game_object(m), "_local_music", &v)) return 0;
    ah_obj *lib = ah_as_obj(v, AO_ARRAY);
    if (!lib || row >= ah_array_size(lib)) return 0;
    ah_obj *next = ah_array_new(m->vm);
    for (int i = 0; i < ah_array_size(lib); i++)
        if (i != row) ah_array_push(m->vm, next, ah_array_get(lib, i));
    ah_set_member(m->vm, game_object(m), "_local_music", ah_objv(next));
    return 1;
}
void menu_jukebox_refresh(menu *m)
{
    if (m) jukebox_folders_fill(m);
}
int menu_jukebox_folders(menu *m, const int *const ids[2], const int counts[2])
{
    int root = m ? menu_active_root(m) : -1;
    if (!m || (root >= 0 && m->entered[root]))
        return 0;
    m->jukebox_folders = ids && counts;
    for (int k = 0; k < 2; k++) {
        m->jukebox_folder_count[k] = 0;
        for (int i = 0; m->jukebox_folders && i < counts[k] && i < 256; i++)
            m->jukebox_folder_ids[k][m->jukebox_folder_count[k]++] = (short)ids[k][i];
    }
    return 1;
}

/* init_music_list (L327..329) fills the Library from default_library_playlist */
int menu_jukebox_library_order(menu *m, const int *order, int count)
{
    ah_val main_;
    if (!m || !m->jukebox_external || count < 0 || (count && !order) ||
        !ah_get_member(m->vm, ah_root(m->vm), "main", &main_))
        return 0;
    ah_obj *list = ah_array_new(m->vm);
    for (int i = 0; i < count; i++)
        ah_array_push(m->vm, list, ah_int(order[i]));
    ah_set_member(m->vm, ah_as_obj(main_, AO_MODULE), "default_library_playlist", ah_objv(list));
    return 1;
}

int menu_jukebox_folder_result(menu *m, int folder, int *ids, int capacity)
{
    ah_val lists;
    if (!m || !m->jukebox_folders || !m->jukebox_root || folder < 0 || folder > 1 || !ids ||
        !ah_get_member(m->vm, m->jukebox_root, "music_list", &lists))
        return -1;
    ah_obj *list = ah_as_obj(ah_array_get(ah_as_obj(lists, AO_ARRAY), 3 + folder), AO_ARRAY);
    int n = list ? ah_array_size(list) : -1;
    if (n > capacity)
        return -1;
    for (int i = 0; i < n; i++)
        ids[i] = (int)ah_num(ah_array_get(ah_as_obj(ah_array_get(list, i), AO_ARRAY), 3));
    return n;
}

/* Leaving the slide project: on the PS2 exit_kick_project destroys the project
   and its pages finalize, so MusicRoot.onFinalize (apply_bgm_playlist /
   apply_slide_playlist, slide.ad L245..252) runs before the save. The cached
   project here finalizes its stack instead, once; the next visit starts anew. */
void menu_jukebox_finish(menu *m)
{
    if (!m || strcmp(ah_module_name(m->project), "SlideProject")) return;
    while (m->depth) {
        int root = m->pages[--m->depth];
        if (m->entered[root])
            finalize_page(m, root);
    }
    jukebox_store(m);
    album_music_store(m);
}
static void album_music_bind(menu *m)
{
    /* ../slide/musiccontrol.h uses the original config-main init_slideplaylist
    and getAllMusicList order. Music runs through the existing PCM worker. */
    ah_vm *vm = m->vm;
    ah_obj *game = game_object(m);
    ah_val option;
    ah_get_member(vm, game, "option", &option);
    ah_set_member(vm, game, "getAllMusicList",
                  ah_native(vm, "getAllMusicList", album_music_list, NULL));
    gt4_music_playlist p;
    if (!gt4_save_option_blob_get("slide_playlist_ps2", p.bytes, sizeof p.bytes) ||
        p.bytes[0] > 128 || p.bytes[1] || p.bytes[2] || p.bytes[3])
        if (!gt4_music_playlist_slide(&p, m->vol, 0))
            return;
    m->album_music_ready = 1;
    m->album_music_snapshot = p;
    ah_obj *playlist = ah_module_new(vm, "slide_playlist", NULL, NULL, NULL),
           *rows = ah_array_new(vm);
    for (unsigned i = 0; i < p.bytes[0]; i++) {
        const uint8_t *e = p.bytes + 16 + i * 16;
        ah_obj *row = ah_array_new(vm);
        ah_array_push(vm, row, ah_int(e[11]));
        ah_array_push(vm, row, ah_int(e[8]));
        ah_array_push(vm, row, ah_int(e[9]));
        ah_array_push(vm, row, ah_int(e[10]));
        ah_array_push(vm, rows, ah_objv(row));
    }
    ah_set_member(vm, playlist, "play_data", ah_objv(rows));
    ah_set_member(vm, playlist, "shuffle", ah_int(p.bytes[6]));
    ah_set_member(vm, option.u.o, "slide_playlist", ah_objv(playlist));
    ah_set_member(vm, option.u.o, "slide_bgm_volume",
                  ah_float((float)menu_option_number(m, "slide_bgm_volume_raw", 120) / 128.f));
}

/* musiccontrol.h exposes the original 004379F0 playlist rows. Persist the
   selection/order/shuffle separately from the race Jukebox. */
static void album_music_store(menu *m)
{
    if (!m || !m->album_music_ready || m->jukebox_external)
        return;
    ah_vm *vm = m->vm;
    ah_val option, playlist, rows, shuffle;
    if (!ah_get_member(vm, game_object(m), "option", &option) ||
        !ah_get_member(vm, ah_as_obj(option, AO_MODULE), "slide_playlist", &playlist) ||
        !ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "play_data", &rows))
        return;
    ah_obj *a = ah_as_obj(rows, AO_ARRAY);
    if (!a)
        return;
    gt4_music_playlist p = m->album_music_snapshot;
    if (ah_array_size(a) != p.bytes[0])
        return;
    for (int i = 0; i < ah_array_size(a); i++) {
        ah_obj *row = ah_as_obj(ah_array_get(a, i), AO_ARRAY);
        if (!row || ah_array_size(row) != 4)
            return;
        uint8_t *e = p.bytes + 16 + i * 16;
        e[11] = (uint8_t)ah_num(ah_array_get(row, 0));
        e[8] = (uint8_t)ah_num(ah_array_get(row, 1));
        e[9] = (uint8_t)ah_num(ah_array_get(row, 2));
        e[10] = (uint8_t)ah_num(ah_array_get(row, 3));
    }
    if (ah_get_member(vm, ah_as_obj(playlist, AO_MODULE), "shuffle", &shuffle))
        p.bytes[6] = (uint8_t)ah_truthy(shuffle);
    if (memcmp(p.bytes, m->album_music_snapshot.bytes, sizeof p.bytes)) {
        gt4_save_option_blob_set("slide_playlist_ps2", p.bytes, sizeof p.bytes);
        gt4_career_blob_set(vm, ah_as_obj(option, AO_MODULE), "slide_playlist_ps2", p.bytes,
                            sizeof p.bytes);
        m->album_music_snapshot = p;
        m->options_dirty = 1;
    }
}
