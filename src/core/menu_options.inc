typedef struct {
    const char *name;
    int valid, type, count;
    double number[16];
    char text[64];
} saved_option;
static saved_option saved_options[] = {
    {"pad_buttons_1p"},
    {"pad_buttons_2p"},
    {"pad_analog_1p"},
    {"pad_analog_2p"},
    {"limit_favorite_car_only"},
    {"limit_favorite_course_only"},
    {"game_zone"},
    {"special_arcade_tuner"},
    {"wide_mode"},
    {"automatic_gear"},
    {"race_display_config"},
    {"race_display_map"},
    {"race_display_steering"},
    {"can_watch_ending"},
    {"sound_type"},
    {"enableAC3"},
    {"timeout_count_to_autodemo"},
    {"replay_display_enable"},
    {"replay_memory_card_slot"},
    {"opening_after_autoload"},
    {"demo_movie_interval"},
    {"auto_save"},
    {"race_laps"},
    {"tire_damage"},
    {"assist_asm"},
    {"assist_tcs"},
    {"arcade_time_limit"},
    {"split_race_laps"},
    {"split_tire_damage"},
    {"split_handicap"},
    {"split_boost_level"},
    {"album_slide_effect"},
    {"album_slide_play_time"},
    {"album_slide_transition_time"},
    {"album_memory_card_slot"},
    {"photo_shutter"},
    {"photo_aspect"},
    {"photo_professional"},
    {"photo_manual_focus"},
    {"vibration_1p"},
    {"vibration_2p"},
    {"active_steering_1p"},
    {"active_steering_2p"},
    {"menu_bgm_volume_raw"},
    {"menu_se_volume_raw"},
    {"race_bgm_volume_raw"},
    {"race_bgm"},
    {"race_se_volume_raw"},
    {"replay_bgm_volume_raw"},
    {"replay_bgm"},
    {"replay_se_volume_raw"},
    {"slide_bgm_volume_raw"},
    {"narration"},
    {"replay_mode"},
    {"replay_play_mode"}, /* labomode.ad commandPlayMode: STRAIGHT or SHUFFLE */
    {"penalty_type"},
    {"photo_quality"},
    {"ustorage_photo_quality"},
    {"steering_assist_1p"},
    {"steering_assist_2p"},
    {"unit_velocity"},
    {"unit_power"},
    {"unit_torque"},
    {"race_color_correction"},
    {"replay_color_correction"},
};
static char option_path[1100];
static void options_restore(menu *m);

void menu_options_storage(const char *path)
{
    demo_movie_count = 0;
    attract_index = attract_left = attract_cancelled = 0;
    attract_pending[0] = attract_origin[0] = 0;
    snprintf(option_path, sizeof option_path, "%s", path ? path : "");
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
        saved_options[i].valid = 0;
#ifdef __SWITCH__
    if (option_path[0])
        plat_switch_recover_file(option_path);
#endif
    FILE *f = option_path[0] ? fopen(option_path, "r") : NULL;
    if (!f)
        return;
    char line[512], key[64], payload[256];
    int type;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%63s %d %255[^\n]", key, &type, payload) == 3) {
            for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
                if (!strcmp(key, saved_options[i].name)) {
                    saved_option *v = &saved_options[i];
                    v->type = type;
                    v->count = 0;
                    v->valid = 0;
                    if (type == 1) {
                        if (strlen(payload) < sizeof v->text) {
                            strcpy(v->text, payload);
                            v->valid = 1;
                        }
                    } else if (type == 0 || type == 2) {
                        char *at = payload, *end;
                        do {
                            double n = strtod(at, &end);
                            if (at == end || !isfinite(n) || fabs(n) > 100000) {
                                v->count = 0;
                                break;
                            }
                            v->number[v->count++] = n;
                            at = end;
                            while (*at == ' ' || *at == ',')
                                at++;
                        } while (*at && v->count < 16);
                        v->valid = v->count > 0 && !*at;
                    }
                }
        }
    fclose(f);
}
static void options_capture(menu *m)
{
    ah_obj *game = game_object(m);
    ah_val option;
    if (!ah_get_member(m->vm, game, "option", &option))
        return;
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++) {
        saved_option *v = &saved_options[i];
        ah_val value;
        if (!ah_get_member(m->vm, ah_as_obj(option, AO_MODULE), v->name, &value))
            continue;
        const char *str = ah_cstr(value);
        ah_obj *array = ah_as_obj(value, AO_ARRAY);
        if (str) {
            if (strlen(str) >= sizeof v->text)
                continue;
            v->type = 1;
            strcpy(v->text, str);
        } else if (array) {
            int count = ah_array_size(array);
            if (count < 1 || count > 16)
                continue;
            v->type = 2;
            v->count = count;
            for (int k = 0; k < count; k++)
                v->number[k] = ah_num(ah_array_get(array, k));
        } else if (value.t == AV_INT || value.t == AV_FLOAT || value.t == AV_BOOL) {
            v->type = 0;
            v->count = 1;
            v->number[0] = ah_num(value);
        } else
            continue;
        v->valid = 1;
    }
    if (!option_path[0])
        return;
    char temp[1120];
    snprintf(temp, sizeof temp, "%s.tmp", option_path);
    FILE *f = fopen(temp, "w");
    if (!f)
        return;
    int ok = 1;
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++) {
        saved_option *v = &saved_options[i];
        if (!v->valid)
            continue;
        if (fprintf(f, "%s %d ", v->name, v->type) < 0)
            ok = 0;
        if (v->type == 1) {
            if (fprintf(f, "%s", v->text) < 0)
                ok = 0;
        } else
            for (int k = 0; k < v->count; k++)
                if (fprintf(f, "%s%.9g", k ? "," : "", v->number[k]) < 0)
                    ok = 0;
        if (fputc('\n', f) == EOF)
            ok = 0;
    }
    if (fclose(f) != 0)
        ok = 0;
#ifdef _WIN32
    if (ok)
        ok =
            MoveFileExA(temp, option_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#elif defined(__SWITCH__)
    /* P: fsdev rename cannot overwrite an existing preferences file. */
    if (ok)
        ok = plat_switch_replace_file(temp, option_path);
#else
    if (ok)
        ok = rename(temp, option_path) == 0;
#endif
    if (!ok)
        ah_logf(m->vm, "failed to save game option preferences");
}
static void options_restore(menu *m)
{
    ah_val option;
    if (!ah_get_member(m->vm, game_object(m), "option", &option))
        return;
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++) {
        saved_option *v = &saved_options[i];
        if (!v->valid)
            continue;
        ah_val value;
        if (v->type == 1)
            value = ah_str(m->vm, v->text);
        else if (v->type == 0)
            value = ah_int((int)v->number[0]);
        else {
            ah_obj *a = ah_array_new(m->vm);
            for (int k = 0; k < v->count; k++)
                ah_array_push(m->vm, a, ah_float(v->number[k]));
            value = ah_objv(a);
        }
        ah_set_member(m->vm, ah_as_obj(option, AO_MODULE), v->name, value);
    }
}
static int nat_option_apply(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    /* MOption.apply -> 004365B8 +4C..68 publishes the three unit enums to
       MUnit (006244D8), after option.ad UnitRoot.apply L4134..4136 writes
       them. Native projects share preferences through the save bridge. */
    options_capture(m);
    gt4_save_capture(vm, game_object(m), 1);
    *ret = ah_nil();
    return 0;
}
double menu_option_number(menu *m, const char *name, double fallback)
{
    if (!m) {
        for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
            if (saved_options[i].valid && saved_options[i].type == 0 &&
                !strcmp(saved_options[i].name, name))
                return saved_options[i].number[0];
        return gt4_save_option_number(name, fallback);
    }
    ah_val option, value;
    if (!ah_get_member(m->vm, game_object(m), "option", &option) ||
        !ah_get_member(m->vm, ah_as_obj(option, AO_MODULE), name, &value) || value.t == AV_NIL)
        return fallback;
    return ah_num(value);
}
void menu_pad_configuration(menu *m, int port, gt4_pad_config *out)
{
    ah_val option;
    if (m && ah_get_member(m->vm, game_object(m), "option", &option)) {
        gt4_pad_option(m->vm, ah_as_obj(option, AO_MODULE), port, out);
        return;
    }
    gt4_pad_defaults(out, port);
    /* Applied options survive switching cached projects and a race restart. */
    for (int analog = 0; analog < 2; analog++) {
        const char *name = analog ? (port ? "pad_analog_2p" : "pad_analog_1p")
                                  : (port ? "pad_buttons_2p" : "pad_buttons_1p");
        int *values = analog ? out->analog : out->button, count = analog ? 6 : 16;
        /* A loaded profile's option takes precedence over standalone preferences. */
        if (!gt4_save_option_array_get(name, values, count))
            for (unsigned k = 0; k < sizeof saved_options / sizeof *saved_options; k++) {
                saved_option *v = &saved_options[k];
                if (!v->valid || v->type != 2 || strcmp(name, v->name) || v->count != count)
                    continue;
                for (int j = 0; j < count; j++)
                    values[j] = (int)v->number[j];
            }
        for (int j = 0; j < count; j++)
            if (values[j] < -2 || values[j] >= GT4_PAD_ACTIONS)
                values[j] = -1;
    }
}
const char *menu_option_text(menu *m, const char *name, const char *fallback)
{
    ah_val option, value;
    const char *s;
    if (!m) {
        for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
            if (saved_options[i].valid && saved_options[i].type == 1 &&
                !strcmp(saved_options[i].name, name))
                return saved_options[i].text;
        return fallback;
    }
    if (!ah_get_member(m->vm, game_object(m), "option", &option) ||
        !ah_get_member(m->vm, ah_as_obj(option, AO_MODULE), name, &value) || !(s = ah_cstr(value)))
        return fallback;
    return s;
}

static int nat_need_save(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    ah_set_member(vm, game_object(data), "need_save", ah_int(1));
    ((menu *)data)->options_dirty = 1;
    *ret = ah_nil();
    return 0;
}
