int menu_license_begin(menu *m, const char *code, gt4_career_license_attempts *visit)
{
    return m && gt4_game_license_begin(m->vm, game_object(m), code, visit);
}
int menu_license_attempt(menu *m, const char *code, gt4_career_license_attempts *visit,
                         int32_t state, uint32_t time, uint32_t date)
{
    if (!m || !gt4_game_license_attempt(m->vm, game_object(m), code, visit, state, time, date))
        return 0;
    menu_save_capture(m);
    return 1;
}
int menu_license_leave(menu *m, const char *code, const gt4_career_license_attempts *visit)
{
    if (!m || !gt4_game_license_leave(m->vm, game_object(m), code, visit))
        return 0;
    menu_save_capture(m);
    return 1;
}
void menu_license_return_prepare(menu *m)
{
    if (!m)
        return;
    /* gtmode.ad L8394 hides the outgoing context before exit_menu("license").
       LicenseDetailRoot.onInitialize L8226..8314 has no matching reveal:
       it runs in a newly loaded project on the PS2. Our cached context must
       discard that outgoing visibility override, preserving cursor position
       and interpolation while the exam page is initialized again. */
    ah_set_member(m->vm, m->context, "cursor_visible", ah_int(1));
    /* gtmode.ad LicenseDetailRoot.onInitialize L8226..8227 clears the
       outgoing Veil; L8254..8296 refreshes all exam medals. onActivate
       L8394..8398 shows that veil before exit_menu("license"). A cached
       project must re-run initialization when the examination returns. */
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "LicenseDetailRoot"))
            m->entered[i] = 0;
}
