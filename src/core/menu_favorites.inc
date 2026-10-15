static uint64_t favorite_resolve(void *context, const char *label)
{
    menu *m = context;
    ah_val value;
    /* Original: func_001C01D0 +0xA0..0xC4: try both car tables before COURSE. */
    ah_val argument = ah_str(m->vm, label);
    nat_calendar_car_key(m->vm, NULL, m, 1, &argument, &value);
    uint64_t key = (uint64_t)value.u.i;
    if (key == UINT64_MAX)
        course_key(m, label, &key);
    return key;
}
static ah_val favorite_label(ah_vm *vm, void *context, uint64_t key)
{
    menu *m = context;
    /* Original: func_00447CA0 +0x18..0x40; func_00443E60 +0x0C..0x28:
       namespace is the upper word; an unknown key produces an empty string. */
    const char *table = (key >> 32) == 0 ? "GENERIC_CAR" :
                        (key >> 32) == 0x26 ? "TUNED_CAR" :
                        (key >> 32) == 0x22 ? "COURSE" : NULL;
    if (!table)
        return ah_str(vm, "");
    char path[96];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.idi", table);
    size_t size = 0;
    uint8_t *data = gt4_vol_read(m->vol, path, &size);
    const char *label = gt4_specdb_label_for_id(data, size, (uint32_t)key);
    ah_val result = ah_str(vm, label ? label : "");
    free(data);
    return result;
}
static void favorite_changed(void *context)
{
    /* The original mutates game_data directly. The PC profile must be captured
       before autosave; changing favorites is not an options-menu edit. */
    (void)context;
    gt4_save_mark_dirty();
}
static void menu_bind_favorites(menu *m, ah_obj *game)
{
    /* Original: func_001651B0 +0x4C; func_00165250 +0x4C: independent lists. */
    m->favorite_labels.context = m;
    m->favorite_labels.changed = favorite_changed;
    m->favorite_labels.resolve = favorite_resolve;
    m->favorite_labels.label = favorite_label;
    gt4_favorites_bind(m->vm, profile_module(m->vm, game, "favorite_car"), &m->favorite_labels);
    gt4_favorites_bind(m->vm, profile_module(m->vm, game, "favorite_course"), &m->favorite_labels);
}
