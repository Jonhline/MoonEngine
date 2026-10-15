int menu_race_is_license(menu *m)
{
    ah_val kind;
    return m && ah_get_member(m->vm, game_object(m), "next_sequence", &kind) && ah_cstr(kind) &&
           !strcmp(ah_cstr(kind), "license");
}
int menu_get_license_request(menu *m, char code[32])
{
    ah_val value;
    if (!m || !code || !menu_race_is_license(m) ||
        !ah_get_member(m->vm, game_object(m), "race_code", &value))
        return 0;
    const char *text = ah_cstr(value);
    if (!text || !*text || strlen(text) >= 32 || gt4_career_license_index(text) < 0)
        return 0;
    snprintf(code, 32, "%s", text);
    return 1;
}
uint32_t menu_license_best(menu *m, const char *code)
{
    return m ? gt4_game_license_best(m->vm, game_object(m), code) : 0x157529ffu;
}
