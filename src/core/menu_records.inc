/* MCourseRecord (CourseRecordT<128>, 0x238-byte units keyed by the course
   code, 00433AA0/00433C10): ten entries per course sorted by time, each
   with its date, car and passcode (00433028..00433708); best_score is the
   Arcade's score byte (+0x230). Kept as game.course_record.ranking. */
#define COURSE_RECORD_EMPTY 0x157529ffu
static unsigned car_u32(const uint8_t *b);
static void load_car_tables(menu *m);
static ah_val record_field(ah_vm *vm, ah_obj *o, const char *name)
{
    ah_val v = ah_nil();
    if (o)
        ah_get_member(vm, o, name, &v);
    return v;
}
static ah_obj *course_record_entries(ah_vm *vm, ah_obj *unit)
{
    return ah_as_obj(record_field(vm, unit, "entries"), AO_ARRAY);
}
static ah_obj *course_record_entry_new(ah_vm *vm, uint32_t time, uint32_t date, const char *car,
                                       const char *passcode)
{
    ah_obj *e = ah_module_new(vm, "entry", NULL, NULL, NULL);
    ah_set_member(vm, e, "time", ah_int((int64_t)time));
    ah_set_member(vm, e, "date", ah_int((int64_t)date));
    ah_set_member(vm, e, "car", ah_str(vm, car ? car : ""));
    ah_set_member(vm, e, "passcode", ah_str(vm, passcode ? passcode : ""));
    return e;
}
static uint32_t course_record_time(ah_vm *vm, ah_obj *entries, int i)
{ /* 004334F8 */
    ah_obj *e = entries && i >= 0 && i < 10 && i < ah_array_size(entries)
                    ? ah_as_obj(ah_array_get(entries, i), AO_MODULE)
                    : NULL;
    ah_val t = record_field(vm, e, "time");
    return e && ah_is_num(t) ? (uint32_t)ah_num(t) : COURSE_RECORD_EMPTY;
}
static uint64_t record_u64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--)
        v = v << 8 | p[i];
    return v;
}
/* A PS2 save's course records (records_ps2, 132 units) for one course. */
static void course_record_import(menu *m, ah_obj *course_record, ah_obj *unit, uint64_t key)
{
    ah_vm *vm = m->vm;
    enum { UNITS = 132, SIZE = 0x238 };
    if (record_field(vm, course_record, "records_ps2").t == AV_NIL)
        return;
    uint8_t *all = malloc(UNITS * SIZE);
    if (!all)
        return;
    if (gt4_career_blob_get(vm, course_record, "records_ps2", all, UNITS * SIZE)) {
        load_car_tables(m);
        for (int u = 0; u < UNITS; u++) {
            const uint8_t *p = all + u * SIZE;
            if (record_u64(p) != key)
                continue;
            ah_obj *entries = course_record_entries(vm, unit);
            for (int i = 0; i < 10; i++) {
                const uint8_t *e = p + 8 + i * 0x30;
                uint32_t time = car_u32(e), date = car_u32(e + 4);
                uint64_t car = record_u64(e + 8);
                const char *label =
                    (car >> 32) == 0
                        ? gt4_specdb_label_for_id(m->car_labels, m->car_labels_size, (uint32_t)car)
                        : NULL;
                char passcode[33];
                memcpy(passcode, e + 0x10, 32);
                passcode[32] = 0;
                ah_array_set(
                    entries, i,
                    ah_objv(course_record_entry_new(vm, time, date, label,
                                                    time == COURSE_RECORD_EMPTY ? "" : passcode)));
            }
            gt4_career_blob_set(vm,unit,"b_spec_skill_ps2",p+0x228,8);
            ah_set_member(vm, unit, "best_score", ah_int(p[0x230]));
            break;
        }
    }
    free(all);
}
static ah_obj *course_record_unit(menu *m, const char *course)
{
    ah_vm *vm = m->vm;
    uint64_t key;
    if (!course_key(m, course, &key))
        return NULL;
    ah_obj *course_record = profile_module(vm, game_object(m), "course_record");
    ah_obj *ranking = ah_as_obj(record_field(vm, course_record, "ranking"), AO_ARRAY);
    if (!ranking) {
        ranking = ah_array_new(vm);
        ah_set_member(vm, course_record, "ranking", ah_objv(ranking));
    }
    for (int i = 0; i < ah_array_size(ranking); i++) {
        ah_obj *unit = ah_as_obj(ah_array_get(ranking, i), AO_MODULE);
        const char *name = ah_cstr(record_field(vm, unit, "course"));
        if (unit && name && !strcmp(name, course) && course_record_entries(vm, unit))
            return unit;
    }
    ah_obj *unit = ah_module_new(vm, "MCourseRecordUnit", NULL, NULL, NULL),
           *entries = ah_array_new(vm);
    ah_set_member(vm, unit, "course", ah_str(vm, course));
    ah_set_member(vm, unit, "best_score", ah_int(0));
    ah_set_member(vm, unit, "entries", ah_objv(entries));
    for (int i = 0; i < 10; i++)
        ah_array_push(
            vm, entries,
            ah_objv(course_record_entry_new(vm, COURSE_RECORD_EMPTY, 0, "", ""))); /* 00432E40 */
    course_record_import(m, course_record, unit, key);
    ah_array_push(vm, ranking, ah_objv(unit));
    return unit;
}
/* 00433178 + 00433028: the slot before the first slower or empty entry
   (an equal time goes after); the later entries move down, the tenth drops. */
static int ranking_passcode(menu *m, const char *car, uint32_t lap, char code[33])
{
    uint8_t key[16];
    uint32_t id;
    load_car_tables(m);
    if (!gt4_specdb_label(m->car_labels, m->car_labels_size, car, &id) ||
        !gt4_ranking_code_key(m->vol, key))
        return 0;
    /* 0015B7A0 +114..160 / 0015B948 +128..16C: car ordinal,
    fixed test fields, score. A: the caller does not initialize the
    unused one-bit stack field; native storage explicitly uses zero.
    004482C0 +4..24 extracts and sign-extends the low 32-bit identifier
    (lui FFFF is sign-extended before dsrl32) for 00449190. */
    int index = gt4_specdb_row_index(m->car_db, m->car_db_size, (uint64_t)(int64_t)(int32_t)id);
    if (index < 0)
        return 0;
    uint32_t fields[10] = {0, (uint32_t)index, 1, 0, 1, 1, 1, lap, 1, 1};
    gt4_ranking_code(fields, key, (uint32_t)time(NULL), code);
    return 1;
}
int menu_course_record_insert(menu *m, const char *course, const char *car, uint32_t lap)
{
    if (!m || !course || !car || lap == COURSE_RECORD_EMPTY)
        return 0;
    ah_vm *vm = m->vm;
    ah_obj *unit = course_record_unit(m, course), *entries = course_record_entries(vm, unit);
    if (!entries)
        return 0;
    int at = -1;
    for (int i = 0; i < 10; i++) {
        uint32_t t = course_record_time(vm, entries, i);
        if (t == COURSE_RECORD_EMPTY || lap < t) {
            at = i;
            break;
        }
    }
    if (at < 0)
        return 0;
    char code[33];
    if (!ranking_passcode(m, car, lap, code))
        return 0;
    for (int i = 9; i > at; i--)
        ah_array_set(entries, i, ah_array_get(entries, i - 1));
    time_t now = time(NULL);
    struct tm *t = localtime(&now); /* 00548918/00579BD8: the console clock */
    uint32_t date = t ? (uint32_t)(uint16_t)(t->tm_year + 1900) | (uint32_t)(t->tm_mon + 1) << 16 |
                            (uint32_t)t->tm_mday << 24
                      : 0;
    ah_array_set(entries, at, ah_objv(course_record_entry_new(vm, lap, date, car, code)));
    gt4_save_capture(vm, game_object(m), 1);
    return 1;
}
/* the top speed of a Power & Speed entry (00433528: 0 none) */
static float machine_record_speed(ah_vm *vm, ah_obj *entries, int i)
{
    ah_obj *e = entries && i >= 0 && i < 10 && i < ah_array_size(entries)
                    ? ah_as_obj(ah_array_get(entries, i), AO_MODULE)
                    : NULL;
    ah_val v = record_field(vm, e, "speed");
    return e && ah_is_num(v) ? (float)ah_num(v) : 0.f;
}
/* MMachineRecord (GameData +0x26640, four CourseRecordT units, 00433C78):
   0 the 400 m, 1 the 1000 m, 2 the top speed, 3 the 400 m of Las Vegas
   (00116268 / 00116440 / 001165B8; quick-mt.ad UnitData). Kept as
   game.machine_record.ranking. */
static ah_obj *machine_record_unit(menu *m, int index)
{
    ah_vm *vm = m->vm;
    if (index < 0 || index > 3)
        return NULL;
    ah_obj *record = profile_module(vm, game_object(m), "machine_record");
    ah_obj *ranking = ah_as_obj(record_field(vm, record, "ranking"), AO_ARRAY);
    if (!ranking) {
        ranking = ah_array_new(vm);
        ah_set_member(vm, record, "ranking", ah_objv(ranking));
    }
    while (ah_array_size(ranking) < 4) {
        ah_obj *unit = ah_module_new(vm, "MCourseRecordUnit", NULL, NULL, NULL),
               *entries = ah_array_new(vm);
        ah_set_member(vm, unit, "entries", ah_objv(entries));
        for (int i = 0; i < 10; i++) {
            ah_obj *e = course_record_entry_new(vm, COURSE_RECORD_EMPTY, 0, "", "");
            ah_set_member(vm, e, "speed", ah_float(0));
            ah_array_push(vm, entries, ah_objv(e));
        }
        ah_array_push(vm, ranking, ah_objv(unit));
    }
    return ah_as_obj(ah_array_get(ranking, index), AO_MODULE);
}
/* 00433260 (a time) / 00433348 (a top speed: before the first slower or
   empty one, 004331F8) into unit `index` */
int menu_machine_record_insert(menu *m, int index, const char *car, uint32_t lap, float speed)
{
    if (!m || !car)
        return 0;
    ah_vm *vm = m->vm;
    ah_obj *unit = machine_record_unit(m, index), *entries = course_record_entries(vm, unit);
    if (!entries)
        return 0;
    int at = -1;
    for (int i = 0; i < 10; i++) {
        if (index == 2) {
            float v = machine_record_speed(vm, entries, i);
            if (v == 0.f || v < speed) {
                at = i;
                break;
            }
        } else {
            uint32_t t = course_record_time(vm, entries, i);
            if (t == COURSE_RECORD_EMPTY || lap < t) {
                at = i;
                break;
            }
        }
    }
    if (at < 0 || (index == 2 ? !(speed > 0) : lap == COURSE_RECORD_EMPTY))
        return 0;
    char code[33];
    if (!ranking_passcode(m, car, index == 2 ? 0 : lap, code))
        return 0; /* 0015B948 +144: speed test packs zero */
    for (int i = 9; i > at; i--)
        ah_array_set(entries, i, ah_array_get(entries, i - 1));
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    uint32_t date = t ? (uint32_t)(uint16_t)(t->tm_year + 1900) | (uint32_t)(t->tm_mon + 1) << 16 |
                            (uint32_t)t->tm_mday << 24
                      : 0;
    ah_obj *e =
        course_record_entry_new(vm, index == 2 ? COURSE_RECORD_EMPTY : lap, date, car, code);
    ah_set_member(vm, e, "speed", ah_float(index == 2 ? speed : 0));
    ah_array_set(entries, at, ah_objv(e));
    gt4_save_capture(vm, game_object(m), 1);
    return 1;
}
static int course_unit_method(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    menu *m = ah_user(vm);
    const char *op = data;
    ah_obj *entries = course_record_entries(vm, self);
    int count = 0;
    for (int i = 0; i < 10; i++)
        if (course_record_time(vm, entries, i) != COURSE_RECORD_EMPTY)
            count++; /* 00433470 */
    if (!strcmp(op, "getRankNum")) {
        *ret = ah_int(count);
        return 0;
    }
    if (!strcmp(op, "getRankNumMaxSpeed")) { /* 004334B0: the entries whose speed is not 0 */
        int n = 0;
        for (int i = 0; i < 10; i++)
            if (machine_record_speed(vm, entries, i) != 0.f)
                n++;
        *ret = ah_int(n);
        return 0;
    }
    int i = argc ? (int)ah_num(argv[0]) : 0;
    ah_obj *entry = entries && i >= 0 && i < 10 && i < ah_array_size(entries)
                        ? ah_as_obj(ah_array_get(entries, i), AO_MODULE)
                        : NULL;
    char text[96];
    if (!strcmp(op, "deleteEntry") ||
        !strcmp(
            op,
            "deleteEntryMaxSpeed")) { /* 00433430 / 00433450: the later entries move up, the last is cleared */
        if (!entry)
            return *ret = ah_nil(), 0;
        for (int k = i; k < 9; k++)
            ah_array_set(entries, k, ah_array_get(entries, k + 1));
        ah_array_set(entries, 9,
                     ah_objv(course_record_entry_new(vm, COURSE_RECORD_EMPTY, 0, "", "")));
        gt4_save_capture(vm, game_object(m), 1);
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(
            op,
            "getBestSpeed")) { /* 00433528 -> 0042E568: "--%s" or "%d.%02d%s" in the option's unit */
        float v = machine_record_speed(vm, entries, i);
        int mph = strcmp(menu_option_text(m, "unit_velocity", "MPH"), "KMPH") != 0;
        const char *unit = mph ? "mph" : "km/h";
        if (v == 0.f)
            snprintf(text, sizeof text, "--%s", unit);
        else {
            float k = v * 3.6f;
            if (mph)
                k = k * 0.6213712f;
            int n = (int)(k * 100.f);
            snprintf(text, sizeof text, "%d.%02d%s", n / 100, n % 100, unit);
        }
    } else if (!strcmp(op, "getBestTime")) { /* 00433558 -> 0042E478 */
        uint32_t time = course_record_time(vm, entries, i);
        if (time == COURSE_RECORD_EMPTY)
            snprintf(text, sizeof text, "\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f");
        else if (time / 3600000)
            snprintf(text, sizeof text, "%u:%02u'%02u.%03u", time / 3600000, (time / 60000) % 60,
                     (time / 1000) % 60, time % 1000);
        else
            snprintf(text, sizeof text, "%u'%02u.%03u", time / 60000, (time / 1000) % 60,
                     time % 1000);
    } else if (!strcmp(op, "getDate")) { /* 00433598 -> 0042E660 */
        if (!entry)
            snprintf(text, sizeof text, "\x7f/\x7f/\x7f");
        else {
            uint32_t date = (uint32_t)ah_num(record_field(vm, entry, "date"));
            snprintf(text, sizeof text, "%d/%d/%d", (int16_t)date, (date >> 16) & 255, date >> 24);
        }
    } else if (!strcmp(op, "getCarName")) { /* 00433708 -> 004481B8: CAR_NAME's short name */
        const char *car = entry ? ah_cstr(record_field(vm, entry, "car")) : NULL;
        uint32_t id;
        uint8_t row[16];
        const char *name = NULL;
        load_car_tables(m);
        if (car && *car && gt4_specdb_label(m->car_labels, m->car_labels_size, car, &id) &&
            gt4_specdb_row(m->car_names, m->car_names_size, id, row, sizeof row) == 16)
            name = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(row + 8));
        snprintf(text, sizeof text, "%s", name ? name : "");
    } else { /* getPassCode(order, part): 00433618, eight characters per part */
        const char *code = entry ? ah_cstr(record_field(vm, entry, "passcode")) : NULL;
        int part = argc > 1 ? (int)ah_num(argv[1]) : 0;
        size_t n = code ? strlen(code) : 0;
        text[0] = 0;
        if (part >= 0 && part < 4 && n > (size_t)part * 8)
            snprintf(text, sizeof text, "%.8s", code + part * 8);
    }
    *ret = ah_str(vm, text);
    return 0;
}
/* game.machine_record.getMTUnit(index): quick-mt.ad RankingRoot L589 */
static int nat_machine_unit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = d;
    ah_obj *stored =
        argc && ah_is_num(argv[0]) ? machine_record_unit(m, (int)ah_num(argv[0])) : NULL;
    if (!stored) {
        *ret = ah_nil();
        return 0;
    }
    ah_obj *u = ah_module_new(vm, "MCourseRecordUnit", NULL, NULL, NULL);
    ah_set_member(vm, u, "entries", record_field(vm, stored, "entries"));
    static const char *const methods[] = {
        "getRankNum", "getRankNumMaxSpeed", "getBestTime", "getBestSpeed",       "getDate",
        "getCarName", "getPassCode",        "deleteEntry", "deleteEntryMaxSpeed"};
    for (unsigned i = 0; i < sizeof methods / sizeof *methods; i++)
        ah_set_member(
            vm, u, methods[i],
            ah_bind(vm, u, ah_native(vm, methods[i], course_unit_method, (void *)methods[i])));
    *ret = ah_objv(u);
    return 0;
}
/* game.course_record.getUnit(course): RankingRoot's list and the Arcade's best_score */
static int nat_score_unit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    menu *m = d;
    ah_obj *stored = course_record_unit(m, argc ? ah_cstr(argv[0]) : NULL);
    if (!stored) {
        *ret = ah_nil();
        return 0;
    }
    ah_obj *u = ah_module_new(vm, "MCourseRecordUnit", NULL, NULL,
                              NULL); /* the methods stay off the saved unit */
    ah_set_member(vm, u, "entries", record_field(vm, stored, "entries"));
    ah_set_member(vm, u, "best_score", record_field(vm, stored, "best_score"));
    static const char *const methods[] = {"getRankNum", "getBestTime", "getDate",
                                          "getCarName", "getPassCode", "deleteEntry"};
    for (unsigned i = 0; i < sizeof methods / sizeof *methods; i++)
        ah_set_member(
            vm, u, methods[i],
            ah_bind(vm, u, ah_native(vm, methods[i], course_unit_method, (void *)methods[i])));
    *ret = ah_objv(u);
    return 0;
}

/* Original: func_004683A8 +0x0C / func_00433C10 +0x00 */
int menu_bspec_profile(menu *m, const char *course, uint8_t experience[0x180], uint8_t skill[8],
                       int write)
{
    if (!m || !course || !experience || !skill)
        return 0;
    /* Reverse-course suffix is removed by 00468690. */
    char name[64];
    size_t n = strlen(course);
    if (n >= sizeof name)
        return 0;
    memcpy(name, course, n + 1);
    if (n >= 2 && !strcmp(name + n - 2, "24"))
        name[n - 2] = 0;
    ah_vm *vm = m->vm;
    ah_obj *game = game_object(m);
    ah_obj *stats = profile_module(vm, game, "statistics"), *unit = course_record_unit(m, name);
    if (!unit)
        return 0;
    if (!write) {
        memset(experience, 0, 0x180);
        memset(skill, 0, 8);
        if (record_field(vm, stats, "b_spec_experience_ps2").t != AV_NIL &&
            !gt4_career_blob_get(vm, stats, "b_spec_experience_ps2", experience, 0x180))
            return 0;
        if (record_field(vm, unit, "b_spec_skill_ps2").t != AV_NIL &&
            !gt4_career_blob_get(vm, unit, "b_spec_skill_ps2", skill, 8))
            return 0;
        return 1;
    }
    if (!gt4_career_blob_set(vm, stats, "b_spec_experience_ps2", experience, 0x180) ||
        !gt4_career_blob_set(vm, unit, "b_spec_skill_ps2", skill, 8))
        return 0;
    ah_val callback = record_field(vm, game, "_markDirty"), ret;
    if (callback.t != AV_NIL)
        ah_call(vm, callback, 0, NULL, &ret);
    menu_save_capture(m);
    return 1;
}
