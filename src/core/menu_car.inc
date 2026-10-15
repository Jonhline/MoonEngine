static int nat_version_branch(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_str(vm, "main");
    return 0;
} /* func_001AC120: D_00693258 */

/* Script course definitions supply metres. Metric presentation is provisional
   until saved unit preferences are available. */
static int nat_course_units(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    double metres = argc ? ah_num(argv[0]) : 0;
    char value[64];
    if (!isfinite(metres) || metres < 0 || metres > 1e7)
        return ah_error(vm, "invalid course dimension");
    if (d)
        snprintf(value, sizeof value, "%.1f m", metres);
    else
        snprintf(value, sizeof value, "%.2f km", metres / 1000);
    *ret = ah_str(vm, value);
    return 0;
}

static int nat_project_load(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    menu *m = data;
    const char *name = argc ? ah_cstr(argv[0]) : NULL;
    char path[96];
    if (name && (!strcmp(name, "us_carlist") || !strcmp(name, "/projects/arcade/us_carlist")))
        snprintf(path, sizeof path, "/projects/arcade/US_carlist.adc");
    else if (name && strlen(name) == 19 && !strncmp(name, "usedcar/us_used", 15) &&
             strspn(name + 15, "0123456789") == 4 && name[19] == 0)
        snprintf(path, sizeof path, "/usedcar/US_used%.4s.adc", name + 15);
    else
        return ah_error(vm, "unsupported project script: %s", name ? name : "");
    /* _tmp_gtmode.ad L5593 loads the authored weekly stock into UsedCarModule. */
    size_t size = 0;
    uint8_t *bytes = gt4_vol_read(m->vol, path, &size);
    if (!bytes)
        return ah_error(vm, "missing US car catalog script");
    char error[256];
    ah_code *code = ah_load(bytes, size, error, sizeof error);
    free(bytes);
    if (!code)
        return ah_error(vm, "car catalog: %s", error);
    *ret = ah_nil();
    return ah_run_module(vm, code, self ? self : m->project);
}
/* Module.load is an engine method, not an authored static. UsedCarRoot
   L5701 clears the weekly table before L5593 loads the next stock. */
static int project_loader_get(ah_vm *vm, ah_obj *self, const char *name, ah_val *out)
{
    if (strcmp(name, "load"))
        return 0;
    *out = ah_bind(vm, self, ah_native(vm, name, nat_project_load, ah_user(vm)));
    return 1;
}
static const ah_host project_loader_host = {"project loader", project_loader_get, NULL, NULL, NULL};
static unsigned car_u32(const uint8_t *b)
{
    return b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24;
}
static void load_car_tables(menu *m)
{
    if (m->car_load_attempted)
        return;
    m->car_load_attempted = 1;
    m->car_labels = gt4_vol_read(m->vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &m->car_labels_size);
    m->car_db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/GENERIC_CAR.dbt", &m->car_db_size);
    m->car_names =
        gt4_vol_read(m->vol, "/specdb/GT4_US2560/CAR_NAME_american.dbt", &m->car_names_size);
    m->car_strings =
        gt4_vol_read(m->vol, "/specdb/GT4_US2560/american_StrDB.sdb", &m->car_strings_size);
    m->car_links =
        gt4_vol_read(m->vol, "/specdb/GT4_US2560/CAR_VARIATION_american.dbt", &m->car_links_size);
    m->car_variants =
        gt4_vol_read(m->vol, "/specdb/GT4_US2560/VARIATIONamerican.dbt", &m->car_variants_size);
}
/* A car's paints in their menu order (VARIATION rows of its model, by the
   order word +4): name (+12), chip colour and kind (+36: r, g, b, then
   3 metallic / 1 solid). The same table and order as the showroom's. */
typedef struct {
    unsigned order;
    const char *name;
    uint8_t rgb[3];
    int kind;
    uint8_t chip[4][4];
} car_paint;
static int car_paints(menu *m, const char *code, car_paint *out, int max)
{
    load_car_tables(m);
    uint32_t id;
    uint8_t r[256];
    if (!code || !gt4_specdb_label(m->car_labels, m->car_labels_size, code, &id) ||
        gt4_specdb_row(m->car_links, m->car_links_size, id, r, sizeof r) != 8 ||
        gt4_specdb_row(m->car_variants, m->car_variants_size, car_u32(r), r, sizeof r) != 52)
        return 0;
    const char *model = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(r));
    if (!model || m->car_variants_size < 16)
        return 0;
    unsigned rows = car_u32(m->car_variants + 8);
    int n = 0;
    for (unsigned i = 0; i < rows && n < max; i++) {
        if (gt4_specdb_row_at(m->car_variants, m->car_variants_size, i, r, sizeof r) != 52)
            continue;
        const char *name = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(r));
        if (!name || strcmp(name, model))
            continue;
        car_paint *c = &out[n++];
        c->order = car_u32(r + 4);
        c->name = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(r + 12));
        memcpy(c->rgb, r + 36, 3);
        c->kind = r[39] & 2 ? 1 : 2;
        memcpy(c->chip, r + 36, 16);
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (out[j].order < out[i].order) {
                car_paint t = out[j];
                out[j] = out[i];
                out[i] = t;
            }
    return n;
}
static int nat_car_paints(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    menu *m = ah_user(vm);
    ah_val code;
    car_paint paints[128];
    int n = car_paints(m, car_label_of(vm, ah_objv(self)), paints, 128);
    if (!d) {
        ah_obj *a = ah_array_new(vm);
        for (int i = 0; i < n; i++)
            ah_array_push(vm, a, ah_int(i));
        *ret = ah_objv(a);
        return 0;
    }
    ah_val current = ah_nil();
    ah_get_member(vm, self, "color_index", &current);
    int i = argc ? (int)ah_num(argv[0]) : (int)ah_num(current);
    *ret = ah_str(vm, i >= 0 && i < n && paints[i].name && *paints[i].name ? paints[i].name : "-");
    return 0;
}
/* ColorChipFace.setData(car_data, index): the chip shows that paint. */
static int nat_chip_set_data(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)d;
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    car_paint paints[128];
    /* refreshCar passes the owned car alone; showroom chips pass an explicit
       paint index. Read the owned color_index in the one-argument form. */
    ah_obj *data = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    ah_val current = ah_nil();
    if (data)
        ah_get_member(vm, data, "color_index", &current);
    int i = argc > 1 ? (int)ah_num(argv[1]) : (int)ah_num(current);
    int n = argc ? car_paints(m, car_label_of(vm, argv[0]), paints, 128) : 0;
    if (w) {
        w->chip_kind = 0;
        w->chip_band_count = 0;
    }
    if (w && i >= 0 && i < n) {
        w->chip_kind = paints[i].kind;
        memcpy(w->chip_rgb, paints[i].rgb, 3);
        for (int b = 0; b < 4; b++)
            if (paints[i].chip[b][3] & 1)
                memcpy(w->chip_bands[w->chip_band_count++], paints[i].chip[b], 3);
    }
    *ret = ah_nil();
    return 0;
}
static int nat_set_color_index(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)d;
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    /* arcade.ad SplitRoot.set_car invokes setColorIndex separately on
       P1.CarFace and P2.CarFace. Retaining only the last menu paint aliases them. */
    if (argc && isfinite(ah_num(argv[0]))) {
        m->car_color = (int)ah_num(argv[0]);
        if (w)
            w->car_color_index = m->car_color;
    }
    *ret = ah_nil();
    return 0;
}
static const char *car_name(menu *m, const char *code)
{
    load_car_tables(m);
    uint32_t id;
    uint8_t row[256];
    if (gt4_specdb_label(m->car_labels, m->car_labels_size, code, &id) &&
        gt4_specdb_row(m->car_names, m->car_names_size, id, row, sizeof row) == 16)
        return gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(row));
    return NULL;
}
/* func_00447BB8: GENERIC_CAR exact label, then TUNED_CAR exact label. */
static int nat_calendar_car_key(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    (void)self;
    menu *m = data;
    const char *label = argc ? ah_cstr(argv[0]) : NULL;
    if (!label)
        return ah_error(vm, "car key requires a label");
    load_car_tables(m);
    uint32_t id;
    if (gt4_specdb_label(m->car_labels, m->car_labels_size, label, &id)) {
        *ret = ah_int(id);
        return 0;
    }
    size_t size = 0;
    uint8_t *labels = gt4_vol_read(m->vol, "/specdb/GT4_US2560/TUNED_CAR.idi", &size);
    int found = gt4_specdb_label(labels, size, label, &id);
    free(labels);
    *ret = ah_int(found ? (int64_t)(UINT64_C(0x2600000000) | id) : -1);
    return 0;
}
/* func_004409D8 / 00440B18: original WHEEL/WING namespace lookup. */
static int nat_calendar_custom_key(ah_vm *vm, ah_obj *self, void *data, int argc,
                                   const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    int wing = (int)(intptr_t)data;
    const char *label = argc ? ah_cstr(argv[0]) : NULL;
    if (!label) {
        *ret = ah_int(-1);
        return 0;
    }
    char path[96];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.idi", wing ? "WING" : "WHEEL");
    size_t size = 0;
    uint8_t *labels = gt4_vol_read(m->vol, path, &size);
    uint32_t id;
    int found = gt4_specdb_label(labels, size, label, &id);
    free(labels);
    *ret = ah_int(found ? (int64_t)(((uint64_t)(wing ? 30 : 29) << 32) | id) : -1);
    return 0;
}
/* GarageRoot L12612 always queries riding_car.getGarageIndex, including
   after selling the selected car. Keep native profile nil semantics while
   exposing the original empty garage view (index -1) to authored UI. */
static int garage_ui_get(ah_vm *vm, ah_obj *self, const char *name, ah_val *out)
{
    if (strcmp(name, "riding_car"))
        return 0;
    ah_val index, cars;
    ah_get_member(vm, self, "riding_index", &index);
    ah_get_member(vm, self, "cars", &cars);
    *out = ah_array_get(cars.u.o, (int)ah_num(index));
    if (out->t != AV_NIL)
        return 1;
    ah_val empty;
    if (!ah_get_member(vm, self, "_empty_riding_car", &empty)) {
        ah_obj *car = ah_module_new(vm, "empty riding car", NULL, NULL, NULL);
        gt4_game_bind_car(vm, car);
        ah_set_member(vm, car, "garage_index", ah_int(-1));
        ah_set_member(vm, car, "car_code", ah_str(vm, ""));
        empty = ah_objv(car);
        ah_set_member(vm, self, "_empty_riding_car", empty);
    }
    gt4_game_bind_car(vm, empty.u.o);
    *out = empty;
    return 1;
}
static int garage_ui_set(ah_vm *vm, ah_obj *self, const char *name, ah_val value)
{
    (void)value;
    if (strcmp(name, "riding_car"))
        return 0;
    ah_remove_member(vm, self, name);
    return 1;
}
static const ah_host garage_ui_host = {"garage view", garage_ui_get, garage_ui_set, NULL, NULL, 1};
/* 00170088/00170278/001705E0: occupied slots, optional filters,
   country/tuner enumeration ordered by their GENERIC_CAR numeric IDs. */
static int nat_garage_pickup(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    menu *m = ah_user(vm);
    load_car_tables(m);
    int mode = (int)(intptr_t)data;
    ah_module_attach_host(self, &garage_ui_host);
    ah_remove_member(vm, self, "riding_car");
    ah_val cars;
    ah_get_member(vm, self, "cars", &cars);
    ah_obj *out = ah_array_new(vm);
    ah_val enums[256];
    unsigned char used[256] = {0};
    for (int i = 0; i < ah_array_size(cars.u.o); i++) {
        ah_val car = ah_array_get(cars.u.o, i);
        if (car.t == AV_NIL)
            continue;
        const char *label = car_label_of(vm, car);
        uint32_t id;
        uint8_t row[24];
        if (!label || !gt4_specdb_label(m->car_labels, m->car_labels_size, label, &id) ||
            gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) != 24)
            return ah_error(vm, "garage catalogue unavailable");
        ah_val arg = ah_str(vm, label), country, tuner;
        if (nat_car_metadata(vm, NULL, "GetCountry", 1, &arg, &country) < 0 ||
            nat_car_metadata(vm, NULL, "GetTuner", 1, &arg, &tuner) < 0)
            return -1;
        const char *values[] = {ah_cstr(country), ah_cstr(tuner),
                                gt4_arcade_drivetrain(arcade_of(m), label)};
        int matches = 1;
        int filters = mode == 2 ? argc : mode == 1 ? argc : 0;
        for (int j = 0; j < filters && j < 3; j++) {
            const char *filter = ah_cstr(argv[j]);
            if (filter && strcmp(filter, "all") && (!values[j] || strcmp(filter, values[j])))
                matches = 0;
        }
        if (!matches)
            continue;
        if (mode == 2)
            ah_array_push(vm, out, car);
        else {
            unsigned key = row[mode == 0 ? 14 : 16];
            used[key] = 1;
            enums[key] = mode == 0 ? country : tuner;
        }
    }
    if (mode != 2)
        for (int i = 0; i < 256; i++)
            if (used[i])
                ah_array_push(vm, out, enums[i]);
    *ret = ah_objv(out);
    return 0;
}
/* 001467A0: turbo first (00445650, TURBINEKIT byte +9),
   then original generic supercharger flag or equipped category 28 stage 1. */
static int nat_garage_aspiration(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    (void)data;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    uint8_t body[0x4b8], row[256];
    if (!gt4_game_car_body(vm, self, body))
        return ah_error(vm, "garage aspiration body unavailable");
    unsigned slot = car_u32(body + 0x488);
    if (slot >= 3)
        return ah_error(vm, "garage setup unavailable");
    uint64_t key;
    memcpy(&key, body + slot * 0x178 + 0xb0, 8);
    if (key != UINT64_MAX) {
        ah_val arg = ah_int((int64_t)key), bytes;
        if (ah_call_member(vm, self, "_getSettingPartRow", 1, &arg, &bytes) != 1)
            return -1;
        if (ah_array_size(bytes.u.o) > 9 && ah_num(ah_array_get(bytes.u.o, 9))) {
            *ret = ah_int(1);
            return 0;
        }
    }
    load_car_tables(m);
    uint32_t id;
    const char *label = car_label_of(vm, ah_objv(self));
    if (!gt4_specdb_label(m->car_labels, m->car_labels_size, label, &id) ||
        gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) != 24)
        return -1;
    ah_val type = ah_int(28), stage;
    if (row[23]) {
        *ret = ah_int(2);
        return 0;
    }
    if (ah_call_member(vm, self, "getEquipedParts", 1, &type, &stage) != 1)
        return -1;
    *ret = ah_int(stage.u.i == 1 ? 2 : 0);
    return 0;
}
/* Registration00141B90 +0x2004/+0x20AC ->0013F598/0013F668.
   0013F528 +0x30..0x58 tests GENERIC_CAR+0x17, then the equipped
   supercharger stage (0043F4C8, type28) against1. NA additionally
   rejects TURBINEKIT+9 via00445650 +0xC..0x30. Queries only. */
static int nat_car_aspiration_predicate(ah_vm *vm, ah_obj *self, void *data, int argc,
                                        const ah_val *argv, ah_val *ret)
{
    (void)argv;
    menu *m = ah_user(vm);
    uint8_t body[0x4b8], row[24];
    if (argc || !gt4_game_car_body(vm, self, body))
        return ah_error(vm, "aspiration predicate requires owned car");
    unsigned slot = car_u32(body + 0x488);
    if (slot >= 3)
        return ah_error(vm, "invalid aspiration setup");
    uint64_t key;
    memcpy(&key, body + slot * 0x178, 8);
    load_car_tables(m);
    if ((key >> 32) == 0x26) {
        size_t n;
        uint8_t *db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/ENEMY_CARS.dbt", &n), base[24];
        int ok = gt4_specdb_row(db, n, (uint32_t)key, base, sizeof base) >= 8;
        free(db);
        if (!ok)
            return ah_error(vm, "original prepared-car key unavailable");
        memcpy(&key, base, 8);
    }
    if ((key >> 32) ||
        gt4_specdb_row(m->car_db, m->car_db_size, (uint32_t)key, row, sizeof row) != 24)
        return ah_error(vm, "original aspiration car row unavailable");
    int super = row[23] != 0;
    ah_val type = ah_int(28), stage;
    if (!super) {
        if (ah_call_member(vm, self, "getEquipedParts", 1, &type, &stage) < 0)
            return -1;
        super = ah_num(stage) == 1;
    }
    if (data == (void *)1) {
        *ret = ah_bool(super);
        return 0;
    }
    memcpy(&key, body + slot * 0x178 + 0xb0, 8);
    int turbo = 0;
    if (key != UINT64_MAX) {
        ah_val arg = ah_int((int64_t)key), bytes;
        if (ah_call_member(vm, self, "_getSettingPartRow", 1, &arg, &bytes) < 0)
            return -1;
        if (ah_array_size(bytes.u.o) > 9)
            turbo = ah_num(ah_array_get(bytes.u.o, 9)) != 0;
    }
    *ret = ah_bool(data == (void *)2 ? turbo : !super && !turbo);
    return 0;
}
static int garage_name_compare(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return (unsigned char)*a<(unsigned char)*b ? -1 : (unsigned char)*a>(unsigned char) * b;
}
/* 0043DD10 / 0043DDB0: HSV rank, saturation threshold 0.25,
   hue rotation 120 degrees, grey range 152 and final scale 1/512. */
static float garage_color_rank(menu *m, ah_vm *vm, ah_val car)
{
    car_paint paints[128];
    int count = car_paints(m, car_label_of(vm, car), paints, 128);
    ah_val color;
    ah_get_member(vm, car.u.o, "color_index", &color);
    int index = (int)ah_num(color);
    if (index < 0 || index >= count)
        return 0;
    car_paint *paint = &paints[index];
    float r = paint->rgb[0] / 255.0f, g = paint->rgb[1] / 255.0f, b = paint->rgb[2] / 255.0f;
    float maximum = fmaxf(r, fmaxf(g, b)), minimum = fminf(r, fminf(g, b)),
          delta = maximum - minimum;
    float saturation = maximum ? delta / maximum : 0, hue = 0;
    if (delta) {
        if (maximum == r)
            hue = (g - b) / delta;
        else if (maximum == g)
            hue = (b - r) / delta + 2;
        else
            hue = (r - g) / delta + 4;
        hue *= 60;
        if (hue < 0)
            hue += 360;
    }
    float rank;
    if (saturation >= 0.25f) {
        hue += 120;
        if (hue > 360)
            hue -= 360;
        rank = hue + 152;
    } else
        rank = (1 - maximum) * 152;
    rank *= 1.0f / 512.0f;
    if (!(paint->chip[0][3] & 4) && (paint->chip[1][3] & 1))
        rank += 1;
    return rank;
}
/* 00146D80..001479B8: compact power/mass/date and catalogue keys;
   00146F38 lowercases the original short name before comparing it.
   A: stable insertion order for equal keys awaits the PS2 sort oracle. */
static int garage_sort_key(ah_vm *vm, ah_val car, int mode, double *number, const char **text)
{
    uint8_t unit[32], row[24];
    menu *m = ah_user(vm);
    *text = NULL;
    *number = 0;
    if (!gt4_game_car_unit(vm, car.u.o, unit))
        return -1;
    unsigned ps = (car_u32(unit + 8) >> 9) & 0x3ffff, mass = car_u32(unit + 12) & 8191;
    switch (mode) {
    case 0:
        *number = car_u32(unit + 24) & 1023;
        return 0;
    case 1:
        *number = ps;
        return 0;
    case 2:
        *number = mass;
        return 0;
    case 3:
        *number = (float)mass / ((float)ps / 100.0f);
        return 0;
    case 4:
        *number = car_u32(unit + 16) & 0xffffff;
        return 0;
    case 9: {
        ah_val name;
        ah_get_member(vm, car.u.o, "short_name", &name);
        *text = ah_cstr(name);
        return *text ? 0 : -1;
    }
    case 10:
        *text = gt4_arcade_drivetrain(arcade_of(m), car_label_of(vm, car));
        return *text ? 0 : -1;
    case 11:
        *number = garage_color_rank(m, vm, car);
        return 0;
    case 12: {
        ah_val aspiration;
        if (nat_garage_aspiration(vm, car.u.o, NULL, 0, NULL, &aspiration) < 0)
            return -1;
        *number = ah_num(aspiration);
        return 0;
    }
    }
    load_car_tables(m);
    uint32_t id;
    if (!gt4_specdb_label(m->car_labels, m->car_labels_size, car_label_of(vm, car), &id) ||
        gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) != 24)
        return -1;
    if (mode == 5)
        *number = (int32_t)car_u32(row + 8);
    else if (mode == 6)
        *number = (int16_t)(row[12] | row[13] << 8);
    else if (mode == 7)
        *number = row[14];
    else if (mode == 8)
        *number = row[16];
    else
        return -1;
    return 0;
}
/* 00146E88 -> 00146A98: unsigned ten-bit ride-history ranks. */
static int nat_garage_sort_history(ah_vm *vm, ah_obj *self, void *data, int argc,
                                   const ah_val *argv, ah_val *ret)
{
    (void)self;
    int mode = (int)(intptr_t)data;
    if (!argc)
        return ah_error(vm, "garage sort requires an array");
    ah_obj *list = ah_as_obj(argv[0], AO_ARRAY);
    if (!list)
        return ah_error(vm, "garage sort requires an array");
    int up = argc < 2 || ah_num(argv[1]);
    for (int i = 1; i < ah_array_size(list); i++) {
        ah_val value = ah_array_get(list, i);
        double rank;
        const char *text;
        if (garage_sort_key(vm, value, mode, &rank, &text) < 0)
            return ah_error(vm, "garage sort key unavailable");
        int j = i;
        while (j) {
            ah_val prior = ah_array_get(list, j - 1);
            double previous;
            const char *prior_text;
            if (garage_sort_key(vm, prior, mode, &previous, &prior_text) < 0)
                return ah_error(vm, "garage sort key unavailable");
            int order = text && prior_text ? garage_name_compare(prior_text, text)
                        : previous < rank  ? -1
                        : previous > rank  ? 1
                                           : 0;
            if (up ? order <= 0 : order >= 0)
                break;
            ah_array_set(list, j, prior);
            j--;
        }
        ah_array_set(list, j, value);
    }
    *ret = argv[0];
    return 0;
}
/* 001412A8 -> 001465D8: compact PS hundredths, rounded with +0.5.
   00435110: acquisition age plus the disc calendar epoch (2005-04-02). */
static int nat_owned_summary(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)argc;
    (void)argv;
    uint8_t unit[32];
    if (!gt4_game_car_unit(vm, self, unit))
        return ah_error(vm, "owned car summary unavailable");
    if (!(intptr_t)data) {
        uint32_t bits = car_u32(unit + 8);
        *ret = ah_int((int)((float)((bits >> 9) & 0x3ffff) / 100.0f + 0.5f));
    } else if ((intptr_t)data == 2) { /* 00146448: mass / PS, rounded in thousandths. */
        uint32_t ps = (car_u32(unit + 8) >> 9) & 0x3ffff, mass = car_u32(unit + 12) & 8191;
        if (!ps || !mass)
            *ret = ah_str(vm, "----");
        else {
            int value = (int)((float)mass / ((float)ps / 100.0f) * 1000.0f + 0.5f);
            char text[40];
            snprintf(text, sizeof text, "%d.%03d", value / 1000, value % 1000);
            *ret = ah_str(vm, text);
        }
    } else {
        ah_val main_, menu_, calendar, epoch, args[] = {ah_int(2005), ah_int(4), ah_int(2)};
        if (!ah_get_member(vm, ah_root(vm), "main", &main_) ||
            !ah_get_member(vm, main_.u.o, "menu", &menu_) ||
            !ah_get_member(vm, menu_.u.o, "MCalendar", &calendar) ||
            ah_call_member(vm, calendar.u.o, "dateToDay", 3, args, &epoch) != 1)
            return ah_error(vm, "garage acquisition epoch unavailable");
        *ret = ah_int((car_u32(unit + 16) & 0xffffff) + epoch.u.i);
    }
    return 0;
}
/* func_001402A8 -> 0044A2E0. CARS is the original counted parts list;
   namespace names and switch targets come from the user's CORE.GT4. */
static int nat_owned_parts_price(ah_vm *vm, ah_obj *self, void *user, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    (void)user;
    menu *m = ah_user(vm);
    ah_val id;
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (!m || !rules || argc < 2 || (argv[0].t != AV_INT && argv[0].t != AV_LONG) ||
        (argv[1].t != AV_INT && argv[1].t != AV_LONG) || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(
            vm, "getPartsPrice requires original part data, a resolved car and integer type/stage");
    char path[160];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/CARS/%016llX",
             (unsigned long long)(uint32_t)id.u.i);
    size_t size = 0;
    uint8_t *list = gt4_vol_read(m->vol, path, &size);
    uint64_t key = gt4_career_part_key(list, size, argv[0].u.i, argv[1].u.i);
    free(list);
    *ret = ah_int(-1);
    if (key == UINT64_MAX || argv[0].u.i < 1 || argv[0].u.i > 31)
        return 0;
    uint32_t branch = rules->price_branches[(unsigned)argv[0].u.i - 1];
    if (branch == 0x44a458 || branch == 0x44a45c) {
        *ret = ah_int(gt4_career_part_row_price(branch, NULL, 0));
        return 0;
    }
    unsigned table = (unsigned)(key >> 32);
    if (table >= 32)
        return 0;
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.dbt", rules->table_names[table]);
    uint8_t *db = gt4_vol_read(m->vol, path, &size), row[4096];
    int width = gt4_specdb_row(db, size, (uint32_t)key & 65535u, row, sizeof row);
    free(db);
    if (width > 0)
        *ret = ah_int(gt4_career_part_row_price(branch, row, (size_t)width));
    return 0;
}
/* 001401A8 -> 0044A048: an existing part is any result other than
   UINT64_MAX. Category 31 is the original special TIRESIZE key. */
static int nat_owned_parts_exist(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    menu *m = data;
    ah_val id;
    if (argc < 2 || (argv[0].t != AV_INT && argv[0].t != AV_LONG) ||
        (argv[1].t != AV_INT && argv[1].t != AV_LONG) || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "isExistParts requires resolved car and type/stage");
    char path[160];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/CARS/%016llX",
             (unsigned long long)(uint32_t)id.u.i);
    size_t size = 0;
    uint8_t *list = gt4_vol_read(m->vol, path, &size);
    uint64_t key = gt4_career_part_key(list, size, (int32_t)(uint32_t)argv[0].u.i,
                                       (int32_t)(uint32_t)argv[1].u.i);
    free(list);
    *ret = ah_int(key != UINT64_MAX);
    return 0;
}
/* 001403A0 resolves type/stage through the same original CARS lookup. */
static int nat_setting_part_key(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    menu *m = data;
    ah_val id;
    if (argc != 2 || !ah_get_member(vm, self, "spec_id", &id) ||
        (argv[0].t != AV_INT && argv[0].t != AV_LONG) ||
        (argv[1].t != AV_INT && argv[1].t != AV_LONG))
        return ah_error(vm, "part key requires car/type/stage");
    char path[160];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/CARS/%016llX",
             (unsigned long long)(uint32_t)id.u.i);
    size_t size = 0;
    uint8_t *list = gt4_vol_read(m->vol, path, &size);
    uint64_t key = gt4_career_part_key(list, size, (int32_t)(uint32_t)argv[0].u.i,
                                       (int32_t)(uint32_t)argv[1].u.i);
    free(list);
    int64_t signed_key;
    memcpy(&signed_key, &key, 8);
    *ret = ah_int(signed_key);
    return 0;
}
/* 00147D08 -> 00449E10 +0x40..0xD4: cache this car's original CARS
   entries on beginSetting. This is disc data, never synthetic defaults. */
static int nat_cache_setting_parts(ah_vm *vm, ah_obj *self, void *data, int argc,
                                   const ah_val *argv, ah_val *ret)
{
    (void)argv;
    menu *m = data;
    ah_val id;
    if (argc || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "setting car key unavailable");
    char path[160];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/CARS/%016llX",
             (unsigned long long)(uint32_t)id.u.i);
    size_t size = 0;
    uint8_t *list = gt4_vol_read(m->vol, path, &size);
    int ok = list && size >= 8 && car_u32(list) <= ((size - 8) / 16) &&
             gt4_career_blob_set(vm, self, "_setting_car_parts_ps2", list, size);
    free(list);
    if (!ok)
        return ah_error(vm, "original CARS list unavailable");
    *ret = ah_nil();
    return 0;
}
/* 00443ED0 -> 00443F00: signed low32 row id, high32 namespace.
   Missing table/row leaves the caller's existing buffer untouched. */
static int nat_setting_part_row(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                ah_val *ret)
{
    (void)self;
    menu *m = data;
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (argc != 1 || !rules || (argv[0].t != AV_INT && argv[0].t != AV_LONG))
        return ah_error(vm, "setting part key unavailable");
    uint64_t key = (uint64_t)argv[0].u.i;
    unsigned table = (unsigned)(key >> 32);
    *ret = ah_nil();
    if (table >= 39 || !rules->table_names[table][0])
        return 0;
    char path[160];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.dbt", rules->table_names[table]);
    size_t size = 0;
    uint8_t *db = gt4_vol_read(m->vol, path, &size), row[256];
    int32_t id;
    uint32_t bits = (uint32_t)key;
    memcpy(&id, &bits, 4);
    int index = gt4_specdb_row_index(db, size, (uint64_t)(int64_t)id);
    int width = index < 0 ? 0 : gt4_specdb_row_at(db, size, (uint32_t)index, row, sizeof row);
    free(db);
    if (width <= 0)
        return 0;
    ah_obj *bytes = ah_array_new(vm);
    for (int i = 0; i < width; i++)
        ah_array_push(vm, bytes, ah_int(row[i]));
    *ret = ah_objv(bytes);
    return 0;
}
/* 004462E0 -> 00443ED0: TRANSMISSION row byte +0x3a. */
static int nat_transmission_type(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    ah_val row;
    if (nat_setting_part_row(vm, self, data, argc, argv, &row) < 0)
        return -1;
    ah_obj *bytes = ah_as_obj(row, AO_ARRAY);
    if (!bytes || ah_array_size(bytes) <= 0x3a)
        return ah_error(vm, "transmission row unavailable");
    *ret = ah_array_get(bytes, 0x3a);
    return 0;
}
/* 00140FA8 +0x70..+0x98 prepends GENERIC_CAR +0x10 tuner unless
   its enum is "nothing"; +0x170..+0x180 appends TUNER_LIST bytes,
   004478F8 +0x4c..+0xa4 stops at zero, with at most ten entries. */
static int nat_car_tuners(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)argv;
    menu *m = data;
    ah_val id;
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (argc || !rules || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "tuner list requires resolved original car");
    load_car_tables(m);
    uint8_t car[24], tuners[10];
    ah_obj *list = ah_array_new(vm);
    if (gt4_specdb_row(m->car_db, m->car_db_size, (uint32_t)id.u.i, car, sizeof car) != 24)
        return ah_error(vm, "original tuner car row unavailable");
    const char *name = rules->tuner_names[car[16]];
    if (!name[0])
        return ah_error(vm, "unknown original car tuner enum");
    if (strcmp(name, "nothing"))
        ah_array_push(vm, list, ah_str(vm, name));
    size_t size = 0;
    uint8_t *db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/TUNER_LIST.dbt", &size);
    int count = gt4_specdb_row(db, size, (uint32_t)id.u.i, tuners, sizeof tuners);
    free(db);
    if (count > 0 && count != 10)
        return ah_error(vm, "invalid original tuner list row");
    for (int i = 0; i < count && tuners[i]; i++) {
        name = rules->tuner_names[tuners[i]];
        if (!name[0])
            return ah_error(vm, "unknown original tuner enum");
        ah_array_push(vm, list, ah_str(vm, name));
    }
    *ret = ah_objv(list);
    return 0;
}
/* car.canTune(tuner, ...) (00140D68): whether any named tuner (00448DC8,
   the enum by name; unknown names are -1 and never match) is the car's
   own, GENERIC_CAR +0x10 (00448788, "nothing" included), or one of its
   TUNER_LIST entries (004478F8). */
static int nat_car_can_tune(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    menu *m = data;
    ah_val id;
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (!rules || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "canTune requires resolved original car");
    load_car_tables(m);
    uint8_t car[24], tuners[10];
    if (gt4_specdb_row(m->car_db, m->car_db_size, (uint32_t)id.u.i, car, sizeof car) != 24)
        return ah_error(vm, "original tuner car row unavailable");
    size_t size = 0;
    uint8_t *db = gt4_vol_read(m->vol, "/specdb/GT4_US2560/TUNER_LIST.dbt", &size);
    int count = gt4_specdb_row(db, size, (uint32_t)id.u.i, tuners, sizeof tuners);
    free(db);
    if (count < 0)
        count = 0;
    if (count > 10)
        return ah_error(vm, "invalid original tuner list row");
    int found = 0;
    for (int a = 0; a < argc && !found; a++) {
        char text[64];
        const char *name = ah_to_text(argv[a], text, sizeof text);
        int tuner = -1;
        for (int k = 0; k < 256 && name; k++)
            if (rules->tuner_names[k][0] && !strcmp(rules->tuner_names[k], name)) {
                tuner = k;
                break;
            }
        if (tuner < 0)
            continue;
        if (car[16] == tuner)
            found = 1;
        for (int i = 0; i < count && !found; i++)
            if (tuners[i] == tuner)
                found = 1;
    }
    *ret = ah_bool(found);
    return 0;
}
/* GT Auto owned-car API: 0013F168/0013F288 -> 00448980/00448970;
   0014D9B0/0014DBA0 -> 00440938/00440988 (live +0x470);
   0014DF50/0014E0B8 -> 00440A10/00440A68 (setup +0x100).
   Body serialization omits the original eight-byte class header. */
static int nat_car_customization(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    menu *m = ah_user(vm);
    const char *op = data;
    ah_val id;
    uint8_t body[0x4b8];
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (!m || !rules || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "customization requires original car data");
    *ret = ah_nil();
    if (!strcmp(op, "isChangeWheelDisabled") || !strcmp(op, "isChangeWingDisabled")) {
        uint8_t row[24];
        load_car_tables(m);
        if (argc ||
            gt4_specdb_row(m->car_db, m->car_db_size, (uint32_t)id.u.i, row, sizeof row) != 24)
            return ah_error(vm, "customization flags unavailable");
        *ret = ah_int(row[!strcmp(op, "isChangeWheelDisabled") ? 21 : 22] != 0);
        return 0;
    }
    int wheel = strstr(op, "Wheel") != NULL;
    if (argc != (wheel ? 2 : 1) || !gt4_game_car_body(vm, self, body))
        return ah_error(vm, "customization requires owned car and selection");
    unsigned category = 0;
    if (wheel) {
        const char *cat = ah_cstr(argv[0]);
        for (; category < 4; category++)
            if (cat && !strcmp(cat, rules->wheel_categories[category]))
                break;
        if (category == 4)
            return ah_error(vm, "invalid wheel category");
    }
    const char *label = ah_cstr(argv[wheel ? 1 : 0]);
    /* Original wrappers map w000/p000 to a null label (factory equipment). */
    if (label && !strcmp(label, wheel ? "w000" : "p000"))
        label = NULL;
    uint64_t key = UINT64_MAX;
    if (label) {
        char path[128];
        snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.idi", wheel ? "WHEEL" : "WING");
        size_t size = 0;
        uint8_t *idi = gt4_vol_read(m->vol, path, &size);
        uint32_t piece;
        int found = gt4_specdb_label(idi, size, label, &piece);
        free(idi);
        if (found)
            key = ((uint64_t)(wheel ? 29 : 30) << 32) | piece;
    }
    uint32_t slot = car_u32(body + 0x488);
    if (slot >= 3)
        return ah_error(vm, "invalid customization setup");
    unsigned offset = wheel ? 0x468 + category * 8 : slot * 0x178 + 0x100;
    uint64_t current;
    memcpy(&current, body + offset, 8);
    if (!strncmp(op, "have", 4)) {
        *ret = ah_int(current == key);
        return 0;
    }
    if (wheel)
        memcpy(body + offset, &key, 8);
    else
        for (unsigned i = 0; i < 3; i++)
            memcpy(body + i * 0x178 + 0x100, &key, 8);
    if (!gt4_career_blob_set(vm, self, "full_car_ps2", body, sizeof body))
        return ah_error(vm, "customization write failed");
    ah_val callback;
    if (ah_get_member(vm, self, "_markDirty", &callback))
        ah_call(vm, callback, 0, NULL, ret);
    *ret = ah_nil();
    return 0;
}
/* Original loadPartsInfo (0013FE78 -> 00449E10) loads CARS for the
   selected car. Native catalog methods read that same file directly. */
static int nat_car_parts_catalog(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    menu *m = ah_user(vm);
    const char *op = data;
    ah_val id;
    const gt4_career_part_data *rules = gt4_game_part_data();
    if (!m || !rules || !ah_get_member(vm, self, "spec_id", &id))
        return ah_error(vm, "parts catalog unavailable");
    /* 0013E370 -> 00445D90: active CHASSIS +0 length in millimetres,
       formatted as decimal text by the wrapper. */
    if (!strcmp(op, "getLength")) {
        uint8_t body[0x4b8];
        if (argc || !gt4_game_car_body(vm, self, body))
            return ah_error(vm, "car length requires owned car");
        uint32_t slot = car_u32(body + 0x488);
        if (slot >= 3)
            return ah_error(vm, "invalid length setup");
        int64_t key;
        memcpy(&key, body + slot * 0x178 + 0x30, 8);
        ah_val arg = ah_int(key), row;
        if (nat_setting_part_row(vm, self, m, 1, &arg, &row) < 0)
            return -1;
        ah_obj *bytes = ah_as_obj(row, AO_ARRAY);
        if (ah_array_size(bytes) < 2)
            return ah_error(vm, "original chassis row unavailable");
        char text[24];
        snprintf(text, sizeof text, "%u",
                 (unsigned)ah_array_get(bytes, 0).u.i |
                     ((unsigned)ah_array_get(bytes, 1).u.i << 8));
        *ret = ah_str(vm, text);
        return 0;
    }
    if (!strcmp(op, "getPartsTypeMax")) {
        if (argc != 1)
            return ah_error(vm, "getPartsTypeMax requires category");
        int64_t type = (int64_t)ah_num(argv[0]);
        *ret = ah_int(type >= 1 && type <= 32 ? rules->type_max[type - 1] : 0);
        return 0;
    }
    char path[128];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/CARS/%016llX",
             (unsigned long long)(uint32_t)id.u.i);
    size_t size = 0;
    uint8_t *list = gt4_vol_read(m->vol, path, &size);
    if (!list)
        return ah_error(vm, "original parts list missing");
    if (!strcmp(op, "loadPartsInfo")) {
        free(list);
        *ret = ah_nil();
        return 0;
    }
    /* 001408A0 -> 0043F3E0: only lightweight type 8 has sequential
       ownership prerequisites. Missing preceding stage also permits it. */
    if (!strcmp(op, "isAllowedParts")) {
        if (argc != 2) {
            free(list);
            return ah_error(vm, "isAllowedParts requires type/stage");
        }
        int32_t type = (int32_t)(uint32_t)argv[0].u.i, stage = (int32_t)(uint32_t)argv[1].u.i;
        int allowed = 1;
        if (type == 8 && stage > (rules->normal_flags[7] ? 0 : 1)) {
            ah_val args[] = {ah_int(type), ah_int((int32_t)((uint32_t)stage - 1))}, owned;
            if (ah_call_member(vm, self, "isHavingParts", 2, args, &owned) != 1) {
                free(list);
                return ah_error(vm, "lightweight ownership unavailable");
            }
            allowed = ah_num(owned) != 0;
            if (!allowed)
                allowed = stage > 0 &&
                          gt4_career_part_key(list, size, type, (int32_t)((uint32_t)stage - 1)) ==
                              UINT64_MAX;
        }
        free(list);
        *ret = ah_int(allowed);
        return 0;
    }
    if (argc < 1) {
        free(list);
        return ah_error(vm, "parts type required");
    }
    int64_t type = (int64_t)ah_num(argv[0]);
    unsigned count = 0;
    if (size >= 8 && car_u32(list) <= (size - 8) / 16)
        for (unsigned i = 0; i < car_u32(list); i++)
            if ((int32_t)car_u32(list + 8 + i * 16 + 4) == type)
                count++;
    /* 0044A128: one-entry categories are available only without stage 0. */
    *ret = ah_int(count == 1 ? gt4_career_part_key(list, size, type, 0) == UINT64_MAX : count);
    free(list);
    return 0;
}
/* 00141B90 +0x1D14 registers isOpenCar -> 0013F040 +0x30..0x64:
   00441248 selects the active setup, 004454C0 reads its GENERIC_CAR key,
   00448820 resolves the row and 004489A0 +0..C returns byte +0x12 bit 4.
   This is an inscription predicate, with no vehicle-physics mutation. */
static int nat_car_is_open(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)argv;
    menu *m = data;
    uint8_t body[0x4b8], row[24];
    if (argc || !gt4_game_car_body(vm, self, body))
        return ah_error(vm, "isOpenCar requires an owned car");
    uint32_t slot = car_u32(body + 0x488);
    if (slot >= 3)
        return ah_error(vm, "invalid open-car setup");
    uint64_t key;
    memcpy(&key, body + slot * 0x178, 8);
    load_car_tables(m);
    /* 00448820 ->00448A58 +0x8..0x48 resolves an ENEMY_CARS key
       through its first qword before querying the generic row. */
    if ((key >> 32) == 0x26) {
        size_t size = 0;
        uint8_t *table = gt4_vol_read(m->vol, "/specdb/GT4_US2560/ENEMY_CARS.dbt", &size),
                tuned[32];
        int found = gt4_specdb_row(table, size, (uint32_t)key, tuned, sizeof tuned) >= 8;
        free(table);
        if (!found) {
            *ret = ah_bool(0);
            return 0;
        }
        memcpy(&key, tuned, 8);
    }
    *ret =
        ah_bool((key >> 32) == 0 &&
                gt4_specdb_row(m->car_db, m->car_db_size, (uint32_t)key, row, sizeof row) == 24 &&
                ((row[18] >> 4) & 1));
    return 0;
}
static int nat_bind_game_car(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    menu *m = data;
    ah_obj *car = argc == 1 ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    if (!car)
        return ah_error(vm, "car binding requires a car");
    load_car_tables(m);
    ah_val code, number, table;
    const char *label = NULL;
    uint32_t id;
    if (ah_get_member(vm, car, "car_code", &code))
        label = ah_cstr(code);
    if (!label && ah_get_member(vm, car, "code", &number)) {
        label = ah_cstr(number);
        if (!label && (number.t == AV_INT || number.t == AV_LONG) &&
            ah_get_member(vm, car, "table", &table) && table.u.i == 0)
            label =
                gt4_specdb_label_for_id(m->car_labels, m->car_labels_size, (uint32_t)number.u.i);
    }
    if (!label || !gt4_specdb_label(m->car_labels, m->car_labels_size, label, &id))
        return ah_error(vm, "unsupported car key in imported garage");
    ah_set_member(vm, car, "spec_id", ah_int(id));
    ah_set_member(vm, car, "car_code", ah_str(vm, label));
    /* OilChangeRoot L12042 queries the owned car after save restore. */
    bind_garage_values(vm, car);
    const char *name = car_name(m, label);
    if (name) {
        ah_set_member(vm, car, "name", ah_str(vm, name));
        uint8_t name_row[16];
        const char *short_label = NULL;
        if (gt4_specdb_row(m->car_names, m->car_names_size, id, name_row, sizeof name_row) == 16)
            short_label =
                gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(name_row + 8));
        if (!short_label)
            short_label = name;
        ah_set_member(vm, car, "short_name", ah_str(vm, short_label));
        /* 0013FBF8: truncate at a backtick followed by two decimal digits. */
        char short_name[256];
        snprintf(short_name, sizeof short_name, "%s", short_label);
        char *year = strchr(short_name, 0x60);
        if (year && year[1] >= '0' && year[1] <= '9' && year[2] >= '0' && year[2] <= '9')
            *year = 0;
        ah_set_member(vm, car, "short_name_no_year", ah_str(vm, short_name));
    }
    uint8_t row[24];
    if (gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) != 24)
        return ah_error(vm, "original car price row unavailable");
    uint32_t price_bits = car_u32(row + 8);
    int32_t base;
    memcpy(&base, &price_bits, 4);
    ah_set_member(vm, car, "original_base_price", ah_int(base));
    ah_set_member(vm, car, "getPartsPrice",
                  ah_bind(vm, car, ah_native(vm, "getPartsPrice", nat_owned_parts_price, m)));
    ah_set_member(vm, car, "getTunerList",
                  ah_bind(vm, car, ah_native(vm, "getTunerList", nat_car_tuners, m)));
    ah_set_member(vm, car, "canTune",
                  ah_bind(vm, car, ah_native(vm, "canTune", nat_car_can_tune, m)));
    ah_set_member(vm, car, "isOpenCar",
                  ah_bind(vm, car, ah_native(vm, "isOpenCar", nat_car_is_open, m)));
    ah_set_member(vm, car, "isNAtune",
                  ah_bind(vm, car, ah_native(vm, "isNAtune", nat_car_aspiration_predicate, NULL)));
    ah_set_member(
        vm, car, "isSuperCharger",
        ah_bind(vm, car, ah_native(vm, "isSuperCharger", nat_car_aspiration_predicate, (void *)1)));
    /* 00143AF0 registration ->0013F488 ->00146758 ->00445650. */
    ah_set_member(
        vm, car, "isTurbo",
        ah_bind(vm, car, ah_native(vm, "isTurbo", nat_car_aspiration_predicate, (void *)2)));
    const char *custom[] = {"isChangeWheelDisabled",
                            "isChangeWingDisabled",
                            "haveTireCategoryWheel",
                            "setTireCategoryWheel",
                            "haveWing",
                            "setWing"};
    for (unsigned i = 0; i < 6; i++)
        ah_set_member(
            vm, car, custom[i],
            ah_bind(vm, car, ah_native(vm, custom[i], nat_car_customization, (void *)custom[i])));
    const char *catalog[] = {"loadPartsInfo", "getPartsTypeMax", "isExistPartsType", "getLength",
                             "isAllowedParts"};
    for (unsigned i = 0; i < 5; i++)
        ah_set_member(
            vm, car, catalog[i],
            ah_bind(vm, car, ah_native(vm, catalog[i], nat_car_parts_catalog, (void *)catalog[i])));
    gt4_game_bind_car(vm, car);
    ah_set_member(
        vm, car, "_cacheSettingParts",
        ah_bind(vm, car, ah_native(vm, "_cacheSettingParts", nat_cache_setting_parts, m)));
    ah_set_member(vm, car, "isExistParts",
                  ah_bind(vm, car, ah_native(vm, "isExistParts", nat_owned_parts_exist, m)));
    ah_set_member(vm, car, "_getSettingPartKey",
                  ah_bind(vm, car, ah_native(vm, "_getSettingPartKey", nat_setting_part_key, m)));
    ah_set_member(vm, car, "_getSettingPartRow",
                  ah_bind(vm, car, ah_native(vm, "_getSettingPartRow", nat_setting_part_row, m)));
    ah_set_member(
        vm, car, "_getTransmissionType",
        ah_bind(vm, car, ah_native(vm, "_getTransmissionType", nat_transmission_type, m)));
    ah_set_member(vm, car, "getColorName",
                  ah_bind(vm, car, ah_native(vm, "getColorName", nat_car_paints, (void *)1)));
    ah_set_member(vm, car, "_markDirty", ah_native(vm, "_markDirty", nat_need_save, m));
    *ret = argv[0];
    return 0;
}
static int nat_car_metadata(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    const char *code = argc ? ah_cstr(argv[0]) : NULL;
    const char *method = data;
    *ret = ah_nil();
    if (!code)
        return ah_error(vm, "car code required");
    load_car_tables(m);
    uint32_t id;
    uint8_t row[256];
    if (!gt4_specdb_label(m->car_labels, m->car_labels_size, code, &id) ||
        gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) != 24)
        return ah_error(vm, "unknown car %s", code);
    /* 0012FEA8 +0xa8 -> 004480D8: CAR_NAME description ID +0x0c. */
    if (!strcmp(method, "GetDescriptionID")) {
        uint8_t name[16];
        if (gt4_specdb_row(m->car_names, m->car_names_size, id, name, sizeof name) != 16)
            return ah_error(vm, "car description unavailable");
        *ret = ah_int((int32_t)car_u32(name + 12));
        return 0;
    }
    if (!strcmp(method, "GetCategory")) {
        const gt4_career_part_data *rules = gt4_game_part_data();
        if (!rules)
            return ah_error(vm, "original car category data not loaded");
        const char *category = gt4_career_car_category_name(rules, row);
        *ret = ah_str(vm, category ? category : rules->category_fallback);
        return 0;
    }
    /* 0012EBA0 -> 00448788 -> 00448DF0: tuner byte +0x10,
       which can differ from the manufacturer byte +0x0f. */
    if (!strcmp(method, "GetTuner")) {
        const gt4_career_part_data *rules = gt4_game_part_data();
        if (!rules || !rules->tuner_names[row[16]][0])
            return ah_error(vm, "original tuner enum unavailable");
        *ret = ah_str(vm, rules->tuner_names[row[16]]);
        return 0;
    }
    /* func_001300D0 -> func_00448868: signed lw GENERIC_CAR +8.
       func_00130300 -> func_004489A0(row,0): lbu +18, bit zero. */
    if (!strcmp(method, "GetPrice")) {
        uint32_t bits = car_u32(row + 8);
        int32_t price;
        memcpy(&price, &bits, 4);
        *ret = ah_int(price);
        return 0;
    }
    if (!strcmp(method, "NotForSell")) {
        *ret = ah_int(row[18] & 1u);
        return 0;
    }
    if (!strcmp(method, "GetCarLabel")) {
        *ret = ah_str(vm, code);
        return 0;
    }
    if (!strcmp(method, "GetOpenModelType")) {
        *ret = ah_int(gt4_career_car_open_type(row));
        return 0;
    }
    if (!strcmp(method, "GetModelYear")) {
        *ret = ah_int(gt4_career_car_year(row));
        return 0;
    }
    /* 0012EE08 -> 004488A8/00448E50: native country enum. */
    if (!strcmp(method, "GetCountry")) {
        const gt4_career_part_data *rules = gt4_game_part_data();
        if (!rules || !rules->country_names[row[14]][0])
            return ah_error(vm, "original country enum unavailable");
        *ret = ah_str(vm, rules->country_names[row[14]]);
        return 0;
    }
    /* Manufacturer and country labels are authored in the menu's catalog. */
    ah_val root, nations, makerdata, catalog;
    if (ah_get_member(vm, m->project, "MakerRoot", &root) &&
        ah_get_member(vm, ah_as_obj(root, AO_MODULE), "nation_list", &nations) &&
        ah_get_member(vm, m->project, "MakerData", &makerdata) &&
        ah_get_member(vm, m->project, "CarData", &catalog)) {
        ah_obj *ns = ah_as_obj(nations, AO_ARRAY);
        for (int i = 0; i < ah_array_size(ns); i++) {
            const char *nation = ah_cstr(ah_array_get(ns, i));
            char key[128];
            ah_val makers;
            snprintf(key, sizeof key, "%s_makers", nation);
            if (!ah_get_member(vm, ah_as_obj(makerdata, AO_MODULE), key, &makers))
                continue;
            ah_obj *ms = ah_as_obj(makers, AO_ARRAY);
            for (int j = 0; j < ah_array_size(ms); j++) {
                const char *maker = ah_cstr(ah_array_get(ms, j));
                ah_val cars;
                snprintf(key, sizeof key, "%s_carlist", maker);
                if (!ah_get_member(vm, ah_as_obj(catalog, AO_MODULE), key, &cars))
                    continue;
                ah_obj *cs = ah_as_obj(cars, AO_ARRAY);
                for (int k = 0; k < ah_array_size(cs); k++) {
                    const char *entry = ah_cstr(ah_array_get(cs, k));
                    if (entry && !strcmp(entry, code)) {
                        if (!strcmp(method, "GetCountry")) {
                            ah_val codes;
                            if (ah_get_member(vm, ah_as_obj(root, AO_MODULE), "nation_list2",
                                              &codes)) {
                                *ret = ah_array_get(ah_as_obj(codes, AO_ARRAY), i);
                                return 0;
                            }
                            return ah_error(vm, "missing country code for %s", code);
                        }
                        *ret = ah_str(vm, maker);
                        return 0;
                    }
                }
            }
        }
    }
    return ah_error(vm, "missing manufacturer for %s", code);
}
static int nat_language(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    if (d) {
        *ret = ah_nil();
        return 0;
    }
    *ret = ah_str(vm, "US");
    return 0;
}

static int nat_empty_favorites(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_int(0);
    return 0;
}

static int nat_calendar_day_first(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                                  ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_int(1);
    return 0;
}

static int nat_car_available(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    menu *m = data;
    ah_val key;
    *ret = ah_bool(0);
    if (!argc || nat_calendar_car_key(vm, NULL, m, 1, argv, &key) < 0 || key.u.i == -1)
        return 0;
    ah_obj *available = profile_module(vm, game_object(m), "available");
    ah_val all, list, legacy;
    if (m->preview_courses || (ah_get_member(vm, available, "all", &all) && ah_truthy(all))) {
        *ret = ah_bool(1);
        return 0;
    }
    /* boot.ad initAvailable iterates US_carlist.InitData.car_carlist.
       Imported PS2 lists use 0042EAC8 -> 0042EA80, 1024 keys at +8,
       with the same override at +2408 as courses. Do not require the
       catalog-preview debug flag to select a car in a normal Arcade session. */
    if (!ah_get_member(vm, available, "cars", &list)) {
        size_t size = 0;
        uint8_t *bytes = gt4_vol_read(m->vol, "/projects/boot/US_carlist.adc", &size);
        char error[256];
        ah_code *script = bytes ? ah_load(bytes, size, error, sizeof error) : NULL;
        free(bytes);
        if (!script)
            return ah_error(vm, "initial Arcade car list unavailable");
        ah_vm *seed = ah_vm_new();
        ah_val init, labels;
        int ok = ah_run_script(seed, script) >= 0 &&
                 ah_get_member(seed, ah_root(seed), "InitData", &init) &&
                 ah_get_member(seed, init.u.o, "car_carlist", &labels);
        ah_obj *cars = ah_array_new(vm);
        if (ok)
            for (int i = 0; i < ah_array_size(labels.u.o); i++) {
                ah_val arg = ah_str(vm, ah_cstr(ah_array_get(labels.u.o, i))), car_key;
                if (nat_calendar_car_key(vm, NULL, m, 1, &arg, &car_key) < 0 || car_key.u.i == -1) {
                    ok = 0;
                    break;
                }
                ah_obj *entry = ah_module_new(vm, "car", NULL, NULL, NULL);
                ah_set_member(vm, entry, "code", ah_int((int32_t)car_key.u.i));
                ah_set_member(vm, entry, "table", ah_int((int32_t)((uint64_t)car_key.u.i >> 32)));
                ah_array_push(vm, cars, ah_objv(entry));
            }
        ah_vm_free(seed);
        if (!ok)
            return ah_error(vm, "invalid original initial Arcade cars");
        list = ah_objv(cars);
        ah_set_member(vm, available, "cars", list);
    }
    const char *label = ah_cstr(argv[0]);
    /* Preserve dated/purchased unlocks already recorded by the GT Mode adapter. */
    if (ah_get_member(vm, available, label, &legacy) && ah_truthy(legacy)) {
        *ret = ah_bool(1);
        return 0;
    }
    for (int i = 0; i < ah_array_size(ah_as_obj(list, AO_ARRAY)); i++) {
        ah_obj *entry = ah_as_obj(ah_array_get(list.u.o, i), AO_MODULE);
        ah_val code, table;
        if (entry && ah_get_member(vm, entry, "code", &code) &&
            ah_get_member(vm, entry, "table", &table) &&
            ((uint64_t)(uint32_t)code.u.i | ((uint64_t)(uint32_t)table.u.i << 32)) ==
                (uint64_t)key.u.i) {
            *ret = ah_bool(1);
            break;
        }
    }
    return 0;
}

/* The original Misc apply resets the demonstration schedule before updating
   Auto Save. Keep the schedule state in game.demonstration so this call cannot
   abort the remaining assignments. Playback consumes this state separately. */
static int nat_demo_reset(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    demo_movie_count = 0;
    ah_val demo;
    if (ah_get_member(vm, game_object(m), "demonstration", &demo))
        ah_set_member(vm, ah_as_obj(demo, AO_MODULE), "movie_count", ah_int(0));
    *ret = ah_nil();
    return 0;
}
