static int course_capacity(menu *m, const char *label)
{
    if (!label)
        return 0;
    if (!m->course_load_attempted) {
        m->course_load_attempted = 1;
        m->course_db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/COURSE.dbt", &m->course_db_size);
        m->course_labels =
            gt4_vol_read(m->vol, "/specdb/GT4_US2560/COURSE.idi", &m->course_labels_size);
    }
    uint32_t id;
    uint8_t row[256];
    if (!gt4_specdb_label(m->course_labels, m->course_labels_size, label, &id))
        return 0;
    int bytes = gt4_specdb_row(m->course_db, m->course_db_size, id, row, sizeof row);
    return bytes == 22 ? row[17] : 0; /* US2560 COURSE.EntryMax */
}
static int nat_course_capacity(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm;
    (void)self;
    *ret = ah_int(argc ? course_capacity(data, ah_cstr(argv[0])) : 0);
    return 0;
}
static int course_key(menu *m, const char *label, uint64_t *key)
{
    if (!label)
        return 0;
    course_capacity(m, label);
    uint32_t id;
    if (!gt4_specdb_label(m->course_labels, m->course_labels_size, label, &id))
        return 0;
    /* 00447C78 +0x14: COURSE namespace 0x22. */
    *key = UINT64_C(0x2200000000) | id;
    return 1;
}
static ah_obj *profile_module(ah_vm *vm, ah_obj *parent, const char *name)
{
    ah_val v;
    if (ah_get_member(vm, parent, name, &v) && ah_as_obj(v, AO_MODULE))
        return v.u.o;
    return submodule(vm, parent, name);
}
static void course_store(ah_vm *vm, ah_obj *available, const uint64_t keys[128])
{
    ah_obj *courses = ah_array_new(vm);
    for (unsigned i = 0; i < 128; i++) {
        ah_obj *entry = ah_module_new(vm, "course", NULL, NULL, NULL);
        ah_set_member(vm, entry, "code", ah_int((int32_t)keys[i]));
        ah_set_member(vm, entry, "table", ah_int((int32_t)(keys[i] >> 32)));
        ah_array_push(vm, courses, ah_objv(entry));
    }
    ah_set_member(vm, available, "courses", ah_objv(courses));
}
static int course_saved(menu *m, ah_obj *available, uint64_t keys[128])
{
    ah_vm *vm = m->vm;
    ah_val value;
    for (unsigned i = 0; i < 128; i++)
        keys[i] = UINT64_MAX;
    if (ah_get_member(vm, available, "courses", &value)) {
        ah_obj *courses = ah_as_obj(value, AO_ARRAY);
        if (!courses)
            return 0;
        for (int i = 0; i < ah_array_size(courses); i++) {
            ah_obj *entry = ah_as_obj(ah_array_get(courses, i), AO_MODULE);
            ah_val code, table;
            if (!entry || !ah_get_member(vm, entry, "code", &code) ||
                !ah_get_member(vm, entry, "table", &table))
                return 0;
            gt4_career_course_unlock(keys,
                                     (uint32_t)code.u.i | ((uint64_t)(uint32_t)table.u.i << 32));
        }
        return 1;
    }
    /* boot.ad initAvailable: authored US_carlist.course_carlist, plus
       the reverse only when CourseData[course].has_reverse is set. */
    size_t size = 0;
    uint8_t *bytes = gt4_vol_read(m->vol, "/projects/boot/US_carlist.adc", &size);
    char error[256];
    ah_code *script = bytes ? ah_load(bytes, size, error, sizeof error) : NULL;
    free(bytes);
    if (!script)
        return 0;
    ah_vm *seed = ah_vm_new();
    int ok = ah_run_script(seed, script) >= 0;
    ah_val init, list, course_data;
    ok = ok && ah_get_member(seed, ah_root(seed), "InitData", &init) &&
         ah_get_member(seed, init.u.o, "course_carlist", &list) &&
         ah_get_member(vm, m->project, "CourseData", &course_data);
    if (ok)
        for (int i = 0; i < ah_array_size(list.u.o); i++) {
            const char *label = ah_cstr(ah_array_get(list.u.o, i));
            ah_val info, reverse;
            uint64_t key;
            if (label && ah_get_member(vm, course_data.u.o, label, &info) &&
                course_key(m, label, &key)) {
                gt4_career_course_unlock(keys, key);
                if (ah_get_member(vm, info.u.o, "has_reverse", &reverse) && ah_truthy(reverse)) {
                    char name[128];
                    snprintf(name, sizeof name, "r_%s", label);
                    if (course_key(m, name, &key))
                        gt4_career_course_unlock(keys, key);
                }
            }
        }
    ah_vm_free(seed);
    if (ok) {
        course_store(vm, available, keys);
        ah_set_member(vm, game_object(m), "need_save", ah_int(1));
        m->options_dirty = 1;
    }
    return ok;
}
static int nat_course_available(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    (void)self;
    menu *m = data;
    uint64_t key, keys[128];
    ah_obj *available = profile_module(vm, game_object(m), "available");
    ah_val all;
    *ret = ah_bool(0);
    if (!argc || !course_key(m, ah_cstr(argv[0]), &key))
        return 0;
    if (!course_saved(m, available, keys))
        return ah_error(vm, "original course availability unavailable");
    *ret = ah_bool(gt4_career_course_available(
        keys, key, ah_get_member(vm, available, "all", &all) && ah_truthy(all)));
    return 0;
}
static int nat_course_unlock(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    menu *m = data;
    uint64_t key, keys[128];
    *ret = ah_nil();
    if (!argc || !course_key(m, ah_cstr(argv[0]), &key))
        return 0;
    ah_obj *available = profile_module(vm, game_object(m), "available");
    if (!course_saved(m, available, keys))
        return ah_error(vm, "original course availability unavailable");
    gt4_career_course_unlock(keys, key);
    course_store(vm, available, keys);
    ah_set_member(vm, game_object(m), "need_save", ah_int(1));
    m->options_dirty = 1;
    ret->t = AV_VOID;
    return 0;
}
void menu_preview_courses(menu *m, int enabled)
{
    if (m)
        m->preview_courses = enabled;
}

/* ---------------------------------------------------------------- the Arcade's race setup (arcade.h) */

static gt4_arcade *arcade_of(menu *m)
{
    if (!m->arcade) {
        m->arcade = gt4_arcade_open(m->vol);
        if (m->arcade)
            gt4_arcade_seed(m->arcade, (uint32_t)time(NULL) * 2654435761u);
    }
    return m->arcade;
}
/* the original's toInt: numbers truncated, strings read as a number (0 when not one) */
static int to_int(ah_val v)
{
    const char *s = ah_cstr(v);
    if (s)
        return (int)strtol(s, NULL, 10);
    return ah_is_num(v) ? (int)ah_num(v) : 0;
}
static const char *car_label_of(ah_vm *vm, ah_val v)
{
    ah_obj *o = ah_as_obj(v, AO_MODULE);
    ah_val code;
    if (!o)
        return ah_cstr(v);
    if (ah_get_member(vm, o, "car_code", &code) && ah_cstr(code))
        return ah_cstr(code);
    return ah_get_member(vm, o, "code", &code) ? ah_cstr(code) : NULL;
}
/* MCourseData.GetType (func_001B3030), GetAttrString (func_001B3320) */
static int nat_course_text(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    gt4_arcade *a = arcade_of(m);
    const char *crs = argc ? ah_cstr(argv[0]) : NULL;
    if (!argc) {
        *ret = ah_nil();
        return 0;
    }
    *ret = ah_str(vm, d ? gt4_arcade_course_attr(a, crs) : gt4_arcade_course_type(a, crs));
    return 0;
}
/* MCarData.GetRaceCode (func_0012E150), IsTireNormal (func_0012E388),
   IsDirtRunnable (func_0012E498, 0012E4F0),
   RaceForbidden (func_00130420), ShiftCarCode (func_0012F3E0) */
static int nat_arcade_car(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    gt4_arcade *a = arcade_of(m);
    const char *op = d, *car = argc ? car_label_of(vm, argv[0]) : NULL;
    *ret = ah_nil();
    if (!argc)
        return 0;
    if (!strcmp(op, "GetRaceCode")) {
        if (argc < 2)
            return 0;
        const char *race =
            gt4_arcade_race_code(a, car, to_int(argv[1]), argc > 2 ? to_int(argv[2]) : 0);
        if (race)
            *ret = ah_str(vm, race);
        return 0;
    }
    if (!strcmp(op, "IsTireNormal")) {
        *ret = ah_bool(gt4_arcade_tire_normal(a, car));
        return 0;
    }
    if (!strcmp(op, "IsDirtRunnable")) {
        *ret = ah_bool(gt4_arcade_dirt_runnable(a, car));
        return 0;
    }
    if (!strcmp(op, "IsTestCar")) {
        *ret = ah_bool(gt4_arcade_test_car(a, car));
        return 0;
    }
    if (!strcmp(op, "RaceForbidden")) {
        *ret = ah_bool(gt4_arcade_race_forbidden(a, car));
        return 0;
    }
    if (!strcmp(op, "ShiftCarCode")) {
        const char *shifted = gt4_arcade_shift_car(a, car, argc > 1 ? ah_cstr(argv[1]) : NULL);
        *ret = ah_str(vm, shifted ? shifted : car ? car : "");
        return 0;
    }
    return 0;
}
/* MCarGarage (and CarFace.car_data): getDTrainType (func_0013CC28),
   isAutomaticOnly (func_0013ED58), getColorNum (func_00445828) */
static int nat_garage_value(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    gt4_arcade *a = arcade_of(m);
    const char *op = d, *car = self ? car_label_of(vm, ah_objv(self)) : NULL;
    if (!strcmp(op, "getDTrainType"))
        *ret = ah_str(vm, gt4_arcade_drivetrain(a, car));
    else if (!strcmp(op, "isAutomaticOnly"))
        *ret = ah_bool(gt4_arcade_automatic_only(a, car));
    else
        *ret = ah_int(gt4_arcade_color_num(a, car));
    return 0;
}
static int nat_quick_catalog(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_catalog_value(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_owned_summary(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_car_metadata(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_garage_aspiration(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static void bind_garage_values(ah_vm *vm, ah_obj *o)
{
    ah_set_member(vm, o, "getCatPs",
                  ah_bind(vm, o, ah_native(vm, "getCatPs", nat_quick_catalog, NULL)));
    ah_set_member(vm, o, "getCatTq",
                  ah_bind(vm, o, ah_native(vm, "getCatTq", nat_quick_catalog, (void *)1)));
    ah_set_member(vm, o, "getCurrentPs",
                  ah_bind(vm, o, ah_native(vm, "getCurrentPs", nat_quick_catalog, (void *)2)));
    ah_set_member(vm, o, "getCurrentTq",
                  ah_bind(vm, o, ah_native(vm, "getCurrentTq", nat_quick_catalog, (void *)3)));
    static const char *const specs[] = {"getCatDisplacement", "getCatEngineType", "getCatEngineCam",
                                        "getCatLength",       "getCatWidth",      "getCatHeight",
                                        "getCatMs",           "getCurrentMs"};
    for (int i = 0; i < 8; i++)
        ah_set_member(
            vm, o, specs[i],
            ah_bind(vm, o, ah_native(vm, specs[i], nat_catalog_value, (void *)(intptr_t)i)));
    static const char *const summary[] = {"getCurrentPower", "getDate", "getCurrentPWR"};
    for (int i = 0; i < 3; i++)
        ah_set_member(
            vm, o, summary[i],
            ah_bind(vm, o, ah_native(vm, summary[i], nat_owned_summary, (void *)(intptr_t)i)));
    ah_set_member(vm, o, "getAspiration",
                  ah_bind(vm, o, ah_native(vm, "getAspiration", nat_garage_aspiration, NULL)));
    static const char *const names[] = {"getDTrainType", "isAutomaticOnly", "getColorNum"};
    for (int i = 0; i < 3; i++)
        ah_set_member(vm, o, names[i],
                      ah_bind(vm, o, ah_native(vm, names[i], nat_garage_value, (void *)names[i])));
}
static int nat_bind_game_car(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
/* MCarGarage().copyFrom(car): the copy made a full garage car (its part
   and setting methods), as gtmode.ad TuningDialog L7747..7755 previews a
   part on such a copy */
static int nat_garage_copy_from(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                                ah_val *ret)
{
    ah_val copy, arg, bound;
    if (!ah_get_member(vm, self, "_copyFrom", &copy) || copy.t == AV_NIL)
        return ah_error(vm, "copyFrom unavailable");
    if (ah_call(vm, copy, argc, argv, ret) < 0)
        return -1;
    /* gtmode.ad L6736..6737 snapshots the previous riding car even in
       an empty garage. 0013BE08 +0x1c..0x7c leaves the converted handle
       empty for nil; there is no car label to resolve at this boundary. */
    if (argc == 1) {
        const char *label = car_label_of(vm, argv[0]);
        if (argv[0].t == AV_NIL || (label && !*label))
            return 0;
    }
    arg = ah_objv(self);
    if (nat_bind_game_car(vm, NULL, d, 1, &arg, &bound) < 0)
        return -1;
    ah_set_member(vm, self, "copyFrom",
                  ah_bind(vm, self, ah_native(vm, "copyFrom", nat_garage_copy_from, d)));
    return 0;
}
static int nat_car_garage(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    ah_obj *o = self ? self : ah_module_new(vm, "MCarGarage", NULL, NULL, NULL);
    const char *car = argc ? car_label_of(vm, argv[0]) : NULL;
    ah_set_member(vm, o, "car_code", ah_str(vm, car ? car : ""));
    ah_set_member(vm, o, "car_color", ah_int(argc > 1 ? to_int(argv[1]) : 0));
    ah_set_member(vm, o, "color_index", ah_int(argc > 1 ? to_int(argv[1]) : 0));
    ah_set_member(vm, o, "odometer", ah_int(0));
    gt4_game_bind_car(vm, o);
    if (car && gt4_game_has_car_factory()) {
        if (!gt4_game_construct_car(vm, o, car, argc > 1 ? to_int(argv[1]) : 0, 0))
            return ah_error(vm, "MCarGarage: original car construction failed");
        ah_val arg = ah_objv(o), bound;
        if (nat_bind_game_car(vm, NULL, d, 1, &arg, &bound) < 0)
            return -1;
    }
    bind_garage_values(vm, o);
    {
        ah_val copy;
        if (ah_get_member(vm, o, "copyFrom", &copy) && copy.t != AV_NIL) {
            ah_set_member(vm, o, "_copyFrom", copy);
            ah_set_member(vm, o, "copyFrom",
                          ah_bind(vm, o, ah_native(vm, "copyFrom", nat_garage_copy_from, d)));
        }
    }
    *ret = ah_objv(o);
    return 0;
}
/* MCarData.PickUpEnemyCars(garage, list, offset, range, count, dirt)
   (func_001308C8): [[car, paint], ...] */
static int nat_pick_enemies(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = d;
    gt4_arcade *a = arcade_of(m);
    gt4_arcade_enemy out[6];
    if (argc < 2) {
        *ret = ah_nil();
        return 0;
    }
    if (!m->rate_cars)
        return ah_error(vm, "PickUpEnemyCars: no car ratings (the game's data)");
    const char *player = car_label_of(vm, argv[0]);
    ah_obj *list = ah_as_obj(argv[1], AO_ARRAY);
    int n = list ? ah_array_size(list) : 0;
    const char **labels = calloc((size_t)n + 1, sizeof *labels);
    if (!labels)
        return ah_error(vm, "out of memory");
    for (int i = 0; i < n; i++) {
        const char *c = ah_cstr(ah_array_get(list, i));
        labels[i] = c ? c : "";
    }
    float offset = argc > 2 ? (float)ah_num(argv[2]) : 0.0f,
          range = argc > 3 ? (float)ah_num(argv[3]) : 0.002f;
    int count = argc > 4 ? to_int(argv[4]) : 5, dirt = argc > 5 ? ah_truthy(argv[5]) : 0;
    int got = gt4_arcade_pick_enemies(a, m->rate_cars, m->rate_ctx, player, labels, n, offset,
                                      range, count, dirt, out);
    if (getenv("MOON_PICK_TRACE")) {
        uint64_t pk = gt4_arcade_car_key(a, player);
        float pr = 0;
        if (pk != UINT64_MAX && m->rate_cars(m->rate_ctx, &pk, 1, &pr)) {
            fprintf(stderr, "pick trace: player=%s rating=%.6f offset=%.6f range=%.6f "
                            "count=%d got=%d\n",
                    player ? player : "-", pr, offset, range, count, got);
            for (int i = 0; i < got; i++) {
                uint64_t k = gt4_arcade_car_key(a, out[i].car);
                float r = 0;
                if (k != UINT64_MAX && m->rate_cars(m->rate_ctx, &k, 1, &r))
                    fprintf(stderr, "pick trace: opponent[%d]=%s rating=%.6f\n", i,
                            out[i].car ? out[i].car : "-", r);
            }
        }
    }
    ah_obj *result = ah_array_new(vm);
    for (int i = 0; i < got; i++) {
        ah_obj *e = ah_array_new(vm);
        ah_array_push(vm, e, ah_str(vm, out[i].car));
        ah_array_push(vm, e, ah_int(out[i].color));
        ah_array_push(vm, result, ah_objv(e));
    }
    free(labels);
    if (got < 0)
        return ah_error(vm, "PickUpEnemyCars: the cars' ratings failed");
    *ret = ah_objv(result);
    return 0;
}
/* MRandom.GetValue(low, high) (func_002CB888, the game's generator) */
static int nat_random_value(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = d;
    gt4_arcade *a = arcade_of(m);
    if (argc < 2) {
        *ret = ah_nil();
        return 0;
    }
    int lo = to_int(argv[0]), hi = to_int(argv[1]);
    *ret = ah_int((int)(gt4_arcade_random01(a) * (float)(hi - lo) + (float)lo));
    return 0;
}
static int nat_random_new(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)argc;
    (void)argv;
    ah_set_member(vm, self, "getValue",
                  ah_bind(vm, self, ah_native(vm, "getValue", nat_random_value, d)));
    *ret = ah_nil();
    return 0;
}
/* MMemoryCardPlayList: the Replay Theater's playlist (labomode.ad
   addPlayList, commandTrashAlias): its replays' names in order; the
   card's "replaylist" is the save folder's playlist file here
   (gt4_replay_playlist_*). The scripts read its size as a member
   (list.size), kept with each change; files are told apart by name. */
static char photo_playlist_names[GT4_ALBUM_CAPACITY][64];
static int photo_playlist_count, photo_playlist_read;
static void photo_playlist_load(void);
static char playlist_names[128][64];
static int playlist_count, playlist_read;
static void playlist_load(void)
{
    if (!playlist_read) {
        playlist_count = gt4_replay_playlist_read(playlist_names, 128);
        playlist_read = 1;
    }
}
static const char *file_name_of(ah_vm *vm, ah_val v)
{
    ah_val n;
    ah_obj *o = ah_as_obj(v, AO_MODULE);
    return o && ah_get_member(vm, o, "_name", &n) ? ah_cstr(n) : NULL;
}
static int playlist_search(const char *name)
{
    for (int i = 0; name && i < playlist_count; i++)
        if (!strcmp(playlist_names[i], name))
            return i;
    return -1;
}
static int nat_playlist_op(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    const char *op = d;
    ah_val marker;
    int photo = ah_get_member(vm, self, "_photo_playlist", &marker) && ah_truthy(marker);
    if (photo)
        photo_playlist_load();
    else
        playlist_load();
    char(*names)[64] = photo ? photo_playlist_names : playlist_names;
    int *count = photo ? &photo_playlist_count : &playlist_count;
    *ret = ah_nil();
    if (!strcmp(op, "clear"))
        (*count) = 0;
    else if (!strcmp(op, "search")) {
        int found = -1;
        const char *name = argc ? file_name_of(vm, argv[0]) : NULL;
        for (int k = 0; name && k < *count; k++)
            if (!strcmp(names[k], name)) {
                found = k;
                break;
            }
        *ret = ah_int(found);
    } else if (!strcmp(op, "push") || !strcmp(op, "append") || !strcmp(op, "insert")) {
        int at = !strcmp(op, "insert") && argc > 1 ? (int)ah_num(argv[0]) : (*count);
        const char *name = argc ? file_name_of(vm, argv[argc - 1]) : NULL;
        if (name && strlen(name) < 64 && (*count) < (photo ? GT4_ALBUM_CAPACITY : 128)) {
            if (at < 0)
                at = 0;
            if (at > (*count))
                at = (*count);
            memmove(names[at + 1], names[at], (size_t)((*count) - at) * 64);
            snprintf(names[at], 64, "%s", name);
            (*count)++;
        }
    } else if (!strcmp(op, "remove") || !strcmp(op, "erase")) {
        int at = argc ? (int)ah_num(argv[0]) : -1;
        if (at >= 0 && at < (*count)) {
            memmove(names[at], names[at + 1], (size_t)((*count) - at - 1) * 64);
            (*count)--;
        }
    } else if (!strcmp(op, "rotate") &&
               (*count)) { /* the first n to the end (addPlayList: the new ones first) */
        int n = argc ? (int)ah_num(argv[0]) : 0;
        char copy[GT4_ALBUM_CAPACITY][64];
        n = ((n % (*count)) + (*count)) % (*count);
        memcpy(copy, names, (size_t)(*count) * 64);
        for (int i = 0; i < (*count); i++)
            memcpy(names[i], copy[(i + n) % (*count)], 64);
    }
    ah_set_member(vm, self, "size", ah_int((*count)));
    return 0;
}
static int nat_playlist_new(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    static const char *const ops[] = {"clear",  "search", "push",   "append",
                                      "remove", "erase",  "insert", "rotate"};
    (void)d;
    (void)argc;
    (void)argv;
    playlist_load();
    for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++)
        ah_set_member(vm, self, ops[i],
                      ah_bind(vm, self, ah_native(vm, ops[i], nat_playlist_op, (void *)ops[i])));
    ah_set_member(vm, self, "size", ah_int(playlist_count));
    *ret = ah_nil();
    return 0;
}
/* game.setEntryCarCode(i, car, paint) (func_00161A50), getEntryCarCode(i) */
static int nat_entry_car(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    int i = argc ? to_int(argv[0]) : -1;
    *ret = ah_nil();
    if (i < 0 || i >= 6)
        return 0;
    if (d) {
        const char *car = argc > 1 ? ah_cstr(argv[1]) : NULL;
        free(m->entry_car[i]);
        m->entry_car[i] = car ? strdup(car) : NULL;
        m->entry_color[i] = argc > 2 ? to_int(argv[2]) : 0;
        return 0;
    }
    ah_obj *e = ah_array_new(vm);
    if (!m->entry_car[i] && i == 0) {
        ah_val car, color;
        ah_obj *game = game_object(m);
        ah_array_push(vm, e, ah_get_member(vm, game, "car_code", &car) ? car : ah_nil());
        ah_array_push(vm, e, ah_get_member(vm, game, "car_color", &color) ? color : ah_int(0));
    } else {
        ah_array_push(vm, e, m->entry_car[i] ? ah_str(vm, m->entry_car[i]) : ah_nil());
        ah_array_push(vm, e, ah_int(m->entry_color[i]));
    }
    *ret = ah_objv(e);
    return 0;
}
void menu_set_car_rater(menu *m, int (*rate)(void *, const uint64_t *, int, float *), void *ctx)
{
    if (m) {
        m->rate_cars = rate;
        m->rate_ctx = ctx;
    }
}
int menu_race_entries(menu *m, const char **cars, int *colors, int max)
{
    ah_val v;
    int n = 0, others = 0;
    if (!m)
        return 0;
    if (ah_get_member(m->vm, game_object(m), "overwrite_enemy_num", &v))
        others = to_int(v);
    for (int i = 1; i <= others && i < 6 && n < max; i++)
        if (m->entry_car[i]) {
            cars[n] = m->entry_car[i];
            colors[n] = m->entry_color[i];
            n++;
        }
    return n;
}
