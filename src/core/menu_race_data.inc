/* func_00447C50 resolves namespace 0x23; func_004483B8 uses the
   low 16 identifier bits and func_00449190 maps to the record ordinal. */
static void load_race_tables(menu *m)
{
    if (m->race_load_attempted)
        return;
    m->race_load_attempted = 1;
    m->race_labels = gt4_vol_read(m->vol, "/specdb/GT4_US2560/RACE.idi", &m->race_labels_size);
    m->race_db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/RACE.dbt", &m->race_db_size);
}
static int nat_race_record_index(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    (void)self;
    menu *m = user;
    const char *label = argc == 1 ? ah_cstr(argv[0]) : NULL;
    if (!label)
        return ah_error(vm, "race record requires a label");
    load_race_tables(m);
    *ret = ah_int(gt4_career_race_record_index(gt4_game_race_lookup_data(), m->race_labels,
                                               m->race_labels_size, m->race_db, m->race_db_size,
                                               label));
    return 0;
}
/* func_00447C50: calendar stores the exact RACE key; no record fallback. */
static int nat_calendar_race_key(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    (void)self;
    menu *m = user;
    const char *label = argc == 1 ? ah_cstr(argv[0]) : NULL;
    if (!label)
        return ah_error(vm, "race calendar requires a label");
    load_race_tables(m);
    uint32_t id;
    *ret = ah_int(gt4_specdb_label(m->race_labels, m->race_labels_size, label, &id)
                      ? (int64_t)(UINT64_C(0x2300000000) | id)
                      : -1);
    return 0;
}
/* MRaceData's boxed object has a 0x10-byte prefix. _raceRow contains only
   the 151 decoded SpecDB bytes, beginning at original object+0x10.
   These adapters expose data; they do not enforce entry requirements. */
static int nat_race_field(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                          ah_val *ret)
{
    const char *method = user;
    ah_val stored;
    uint8_t row[151];
    if (!self || !ah_get_member(vm, self, "_raceRow", &stored) ||
        ah_array_size(ah_as_obj(stored, AO_ARRAY)) != 151)
        return ah_error(vm, "MRaceData has no decoded RACE row");
    for (unsigned i = 0; i < 151; i++)
        row[i] = (uint8_t)ah_array_get(stored.u.o, i).u.i;
    if (!strcmp(method, "getAdditionalDriverPoints")) {
        if (argc < 1 || (argv[0].t != AV_INT && argv[0].t != AV_LONG) ||
            (uint64_t)argv[0].u.i >= 6 || !gt4_game_race_lookup_data())
            return ah_error(vm,
                            "getAdditionalDriverPoints requires original data and position 0..5");
        *ret = ah_int(gt4_career_driver_points(gt4_game_race_lookup_data(), (unsigned)argv[0].u.i));
        return 0;
    }
    if (!strcmp(method, "getPrize")) {
        if (argc < 1)
            return ah_error(vm, "getPrize requires a position");
        *ret = ah_int(gt4_career_race_prize(row, (uint64_t)argv[0].u.i));
        return 0;
    }
    if (argc)
        return ah_error(vm, "%s takes no arguments", method);
    if (!strcmp(method, "getNeedCountry")) {
        const char *names[8];
        unsigned n = gt4_career_countries(row[0x96], names);
        ah_obj *countries = ah_array_new(vm);
        for (unsigned i = 0; i < n; i++)
            ah_array_push(vm, countries, ah_str(vm, names[i]));
        *ret = ah_objv(countries);
        return 0;
    }
    if (!strcmp(method, "getAllowedCarList")) {
        char labels[60][128];
        unsigned count;
        menu *m = ah_user(vm);
        *ret = ah_nil();
        if (!gt4_career_allowed_cars(m->vol, row, labels, &count))
            return 0;
        ah_obj *cars = ah_array_new(vm);
        for (unsigned i = 0; i < count; i++)
            ah_array_push(vm, cars, ah_str(vm, labels[i]));
        *ret = ah_objv(cars);
        return 0;
    }
    if (!strcmp(method, "getNeedYearRange")) {
        ah_obj *years = ah_array_new(vm);
        ah_array_push(vm, years, ah_int(gt4_career_race_field(row, GT4_RACE_YEAR_MIN)));
        ah_array_push(vm, years, ah_int(gt4_career_race_field(row, GT4_RACE_YEAR_MAX)));
        *ret = ah_objv(years);
        return 0;
    }
    static const struct {
        const char *name;
        gt4_career_race_field_id field;
    } fields[] = {{"getNeedLicense", GT4_RACE_LICENSE},
                  {"getNeedDrivetrain", GT4_RACE_DRIVETRAIN},
                  {"getNeedAspiration", GT4_RACE_ASPIRATION},
                  {"getNeedCarType", GT4_RACE_CAR_TYPE},
                  {"getNeedTire", GT4_RACE_TIRE},
                  {"getNeedPs", GT4_RACE_PS},
                  {"getNeedPrice", GT4_RACE_PRICE},
                  {"getLimitedWeight", GT4_RACE_WEIGHT},
                  {"getLimitedLength", GT4_RACE_LENGTH},
                  {"getNeedCountryID", GT4_RACE_COUNTRY},
                  {"getNumberOfLaps", GT4_RACE_LAPS},
                  {"getLimitTime", GT4_RACE_LIMIT_TIME},
                  {"getPrizeGC", GT4_RACE_PRIZE_GC}};
    for (unsigned i = 0; i < sizeof fields / sizeof fields[0]; i++)
        if (!strcmp(method, fields[i].name)) {
            *ret = ah_int(gt4_career_race_field(row, fields[i].field));
            return 0;
        }
    return ah_error(vm, "unsupported MRaceData field %s", method);
}
static int nat_race_available(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)vm;
    (void)self;
    menu *m = user;
    uint32_t id;
    load_race_tables(m);
    const char *label = argc ? ah_cstr(argv[0]) : NULL;
    /* func_001A2A90 -> func_00447C50: label existence, without decoding. */
    *ret = ah_bool(label && gt4_specdb_label(m->race_labels, m->race_labels_size, label, &id));
    return 0;
}
static int nat_race_init(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                         ah_val *ret)
{
    menu *m = user;
    uint8_t row[151];
    load_race_tables(m);
    const char *label = argc ? ah_cstr(argv[0]) : NULL;
    /* func_001A0E48 -> func_00447188 -> func_004470E8 -> func_00443ED0.
       The no-argument constructor remains unported. */
    if (!gt4_career_race_row(gt4_game_race_lookup_data(), m->race_labels, m->race_labels_size,
                             m->race_db, m->race_db_size, label, row))
        return ah_error(vm, "MRaceData: original RACE row unavailable for %s",
                        label ? label : "<no label>");
    ah_obj *bytes = ah_array_new(vm);
    for (unsigned i = 0; i < sizeof row; i++)
        ah_array_push(vm, bytes, ah_int(row[i]));
    ah_set_member(vm, self, "_raceRow", ah_objv(bytes));
    /* func_001A1408 -> 004472A0. 004470E8 copies decoded RACE+0
       into the cached course key at +0xA0 (00447238). Resolve its low
       identifier in the disc COURSE label table, as 00443E60 does. */
    if (!m->course_load_attempted) {
        m->course_load_attempted = 1;
        m->course_db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/COURSE.dbt", &m->course_db_size);
        m->course_labels =
            gt4_vol_read(m->vol, "/specdb/GT4_US2560/COURSE.idi", &m->course_labels_size);
    }
    uint32_t course_id = (uint32_t)row[0] | (uint32_t)row[1] << 8;
    const char *course =
        gt4_specdb_label_for_id(m->course_labels, m->course_labels_size, course_id);
    ah_set_member(vm, self, "course_code", course ? ah_str(vm, course) : ah_nil());
    /* func_001A1370: original registered read-only scalar attribute. */
    ah_set_member(vm, self, "entry_num", ah_int(gt4_career_race_field(row, GT4_RACE_ENTRY_NUM)));
    static const char *const names[] = {"getPrize",          "getAdditionalDriverPoints",
                                        "getPrizeGC",        "getNeedYearRange",
                                        "getNeedLicense",    "getNeedDrivetrain",
                                        "getNeedAspiration", "getNeedCarType",
                                        "getNeedTire",       "getNeedPs",
                                        "getNeedPrice",      "getLimitedWeight",
                                        "getLimitedLength",  "getNeedCountryID",
                                        "getNumberOfLaps",   "getLimitTime",
                                        "getNeedCountry",    "getAllowedCarList"};
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        ah_set_member(vm, self, names[i],
                      ah_bind(vm, self, ah_native(vm, names[i], nat_race_field, (void *)names[i])));
    *ret = ah_nil();
    return 0;
}
/* 001652F0: active at championship live +28, serialized header omitted. */
static int nat_game_suspended(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    ah_val champ = ah_nil(), stored = ah_nil();
    ah_get_member(vm, game_object(m), "championship", &champ);
    ah_get_member(vm, ah_as_obj(champ, AO_MODULE), "context_ps2", &stored);
    uint8_t context[0x88] = {0};
    if (stored.t != AV_NIL &&
        !gt4_career_blob_get(vm, champ.u.o, "context_ps2", context, sizeof context))
        return ah_error(vm, "invalid championship context");
    *ret = ah_bool(context[0x20] != 0);
    return 0;
}
void menu_save_refresh(menu *m)
{
    if (!m || m->jukebox_external)
        return;
    ah_obj *game = game_object(m);
    gt4_save_restore(m->vm, game);
    menu_bind_favorites(m, game);
    /* gtmode.ad getRaceEventRecordStatistics L701..706 caches both values
       for the original project lifetime. Native cached VMs survive a race
       project switch: restoring newer records must invalidate this snapshot
       before HomeRoot L3982 reads it again. Licences are not in L712..767's
       event/mission denominator; do not invent licence completion points. */
    if (!strcmp(ah_module_name(m->project), "GTmodeProject")) {
        ah_set_member(m->vm, m->project, "cached_achievement", ah_nil());
        ah_set_member(m->vm, m->project, "cached_aspec_score", ah_nil());
    }
    if (m->album_music_ready)
        album_music_bind(m);
    /* The restore above replaced game.option with the save's: the Jukebox's
       bgm_playlist and replay_bgm_volume went with it (musiccontrol.h then
       found no play_data and nothing played). */
    if (m == jukebox_menu)
        jukebox_bind(m);
    {
        ah_val option;
        if (ah_get_member(m->vm, game, "option", &option))
            gt4_pad_bind_option(m->vm, ah_as_obj(option, AO_MODULE));
    }
    const char *project = ah_module_name(m->project);
    if (!strcmp(project, "RaceEventProject") || !strcmp(project, "ChampionshipProject") ||
        !strcmp(project, "MissionProject")) {
        /* share/prize_action.ad L52 uses the same main.game.incMoney
           as GT Mode. Native projects have separate VMs, so restore
           its gameplay bindings as well as the saved profile fields. */
        gt4_game_bind(m->vm, game);
        /* 00110498 -> 004312B0 checks the same indexed licence records
           as GT Mode. Restored quick projects must resolve those records,
           rather than create empty per-code units for the admission check. */
        ah_val race_record;
        if (ah_get_member(m->vm, game, "race_record", &race_record))
            ah_set_member(m->vm, ah_as_obj(race_record, AO_MODULE), "_getRecordIndex",
                          ah_native(m->vm, "_getRecordIndex", nat_race_record_index, m));
        ah_set_member(m->vm, game, "_markDirty", ah_native(m->vm, "_markDirty", nat_need_save, m));
    }
    if (!strcmp(ah_module_name(m->project), "GTmodeProject") ||
        !strcmp(ah_module_name(m->project), "LicenseProject") ||
        !strcmp(ah_module_name(m->project), "SettingProject")) {
        gt4_game_bind(m->vm, game);
        ah_set_member(m->vm, game, "isSuspended",
                      ah_native(m->vm, "isSuspended", nat_game_suspended, m));
        ah_val race_record;
        if (ah_get_member(m->vm, game, "race_record", &race_record))
            ah_set_member(m->vm, ah_as_obj(race_record, AO_MODULE), "_getRecordIndex",
                          ah_native(m->vm, "_getRecordIndex", nat_race_record_index, m));
        ah_val calendar;
        ah_get_member(m->vm, game, "calendar", &calendar);
        ah_set_member(m->vm, ah_as_obj(calendar, AO_MODULE), "_getCarKey",
                      ah_native(m->vm, "_getCarKey", nat_calendar_car_key, m));
        ah_set_member(m->vm, ah_as_obj(calendar, AO_MODULE), "_getRaceKey",
                      ah_native(m->vm, "_getRaceKey", nat_calendar_race_key, m));
        ah_set_member(
            m->vm, calendar.u.o, "_getWheelKey",
            ah_native(m->vm, "_getWheelKey", nat_calendar_custom_key, (void *)(intptr_t)0));
        ah_set_member(
            m->vm, calendar.u.o, "_getWingKey",
            ah_native(m->vm, "_getWingKey", nat_calendar_custom_key, (void *)(intptr_t)1));
        ah_val garage;
        ah_get_member(m->vm, game, "garage", &garage);
        ah_module_attach_host(garage.u.o, &garage_ui_host);
        ah_remove_member(m->vm, garage.u.o, "riding_car");
        const char *queries[] = {"pickupCountry", "pickupTuner", "pickup"};
        for (int i = 0; i < 3; i++)
            ah_set_member(
                m->vm, garage.u.o, queries[i],
                ah_bind(m->vm, garage.u.o,
                        ah_native(m->vm, queries[i], nat_garage_pickup, (void *)(intptr_t)i)));
        ah_val change;
        ah_get_member(m->vm, garage.u.o, "changeRidingCar", &change);
        ah_set_member(m->vm, garage.u.o, "_changeRidingCarNative", change);
        ah_set_member(
            m->vm, garage.u.o, "changeRidingCar",
            ah_bind(m->vm, garage.u.o, ah_native(m->vm, "changeRidingCar", nat_pc_change_car, m)));
        ah_set_member(m->vm, ah_as_obj(garage, AO_MODULE), "_getCarName",
                      ah_native(m->vm, "_getCarName", nat_car_name, m));
        ah_set_member(m->vm, ah_as_obj(garage, AO_MODULE), "_bindCar",
                      ah_native(m->vm, "_bindCar", nat_bind_game_car, m));
        ah_val car_array;
        if (ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "cars", &car_array)) {
            for (int i = 0; i < ah_array_size(ah_as_obj(car_array, AO_ARRAY)); i++) {
                ah_val car = ah_array_get(car_array.u.o, i), result;
                if (car.t != AV_NIL)
                    nat_bind_game_car(m->vm, NULL, m, 1, &car, &result);
            }
        }
        ah_val dirty = ah_native(m->vm, "_markDirty", nat_need_save, m);
        ah_set_member(m->vm, game, "_markDirty", dirty);
        const char *owners[] = {"garage", "calendar", "presents", "race_record"};
        for (int i = 0; i < 4; i++) {
            ah_val owner;
            ah_get_member(m->vm, game, owners[i], &owner);
            ah_set_member(m->vm, ah_as_obj(owner, AO_MODULE), "_markDirty", dirty);
        }
    }
}
void menu_save_capture(menu *m)
{
    if (m && m->jukebox_external)
        return;
    if (!m)
        return;
    album_music_store(m);
    /* Cached menus must not overwrite either playlist with an older copy. */
    uint8_t playlist[0x810];
    ah_val option;
    if (ah_get_member(m->vm, game_object(m), "option", &option)) {
        const char *names[] = {"bgm_playlist_ps2", "slide_playlist_ps2"};
        for (int i = 0; i < 2; i++)
            if (gt4_save_option_blob_get(names[i], playlist, sizeof playlist))
                gt4_career_blob_set(m->vm, ah_as_obj(option, AO_MODULE), names[i], playlist,
                                    sizeof playlist);
    }
    /* options the scripts marked (setNeedSave) but no page change applied
       yet, as when the game is closed on the options page: kept too */
    int dirty = m->options_dirty;
    if (dirty) {
        options_capture(m);
        m->options_dirty = 0;
    } else {
        /* Only setNeedSave/apply commits preferences. An untouched cached
           menu still owns an old option copy; do not publish it on exit. */
        if (!gt4_save_restore_options(m->vm, game_object(m)))
            options_restore(m);
    }
    gt4_save_capture(m->vm, game_object(m), dirty);
}

/* Original selection stack: 00169214..00169410 registers the methods
   00165EE0/00165FC0/00166100/00166278; the byte rules have an EE oracle. */
static int nat_menu_selection(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    static const char *names[] = {"menuPush", "menuRef", "menuPop", "menuClearStack"};
    menu *m = ah_user(vm);
    return gt4_game_menu_stack_action(vm, game_object(m), names[(intptr_t)data], argc, argv, ret);
}

static ah_val color_value(ah_vm *vm, const double rgba[4])
{
    ah_obj *color = ah_module_new(vm, "MColorObject", NULL, NULL, NULL);
    const char *keys[] = {"r", "g", "b", "a"};
    for (int i = 0; i < 4; i++)
        ah_set_member(vm, color, keys[i], ah_float(rgba[i]));
    return ah_objv(color);
}
static int nat_color(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    const char *keys[] = {"r", "g", "b", "a"};
    double rgba[4] = {0, 0, 0, 1};
    if (!data) {
        if (argc != 3 && argc != 4)
            return ah_error(vm, "color requires RGB and optional alpha");
        for (int i = 0; i < argc; i++) {
            if (!ah_is_num(argv[i]) || !isfinite(ah_num(argv[i])))
                return ah_error(vm, "invalid color component");
            rgba[i] = fmin(1, fmax(0, ah_num(argv[i])));
        }
        for (int i = 0; i < 4; i++)
            ah_set_member(vm, self, keys[i], ah_float(rgba[i]));
        *ret = ah_objv(self);
    } else {
        if (argc != 3 || !ah_as_obj(argv[0], AO_MODULE) || !ah_as_obj(argv[1], AO_MODULE) ||
            !ah_is_num(argv[2]) || !isfinite(ah_num(argv[2])))
            return ah_error(vm, "invalid color interpolation");
        double t = fmin(1, fmax(0, ah_num(argv[2])));
        for (int i = 0; i < 4; i++) {
            ah_val a, b;
            if (!ah_get_member(vm, argv[0].u.o, keys[i], &a) ||
                !ah_get_member(vm, argv[1].u.o, keys[i], &b) || !ah_is_num(a) || !ah_is_num(b))
                return ah_error(vm, "invalid color");
            /* 00203A60..00203ABC: first*t + second*(1-t).
               OilChangeRoot supplies dirty first, clear second. */
            rgba[i] = ah_num(a) * t + ah_num(b) * (1 - t);
        }
        *ret = color_value(vm, rgba);
    }
    return 0;
}
