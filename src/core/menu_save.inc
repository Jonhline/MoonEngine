static int nat_pc_autosave(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    menu *m = data;
    int ok = gt4_save_capture(vm, game_object(m), 1);
    if (ok && gt4_save_active() && gt4_save_auto_enabled())
        ok = gt4_save_store();
    if (argc > 1) {
        ah_val progress;
        if (ah_get_member(vm, ah_as_obj(argv[1], AO_MODULE), "progress", &progress))
            ah_set_member(vm, ah_as_obj(progress, AO_MODULE), "value", ah_float(ok ? 1 : 0));
    }
    if (!ok)
        return ah_error(vm, "native Auto Save: %s", gt4_save_error());
    if (argc > 3)
        ah_reference_set(vm, argv[3], ah_int(1));
    *ret = ah_bool(1);
    return 0;
}
static int nat_open_save(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    /* P: Save writes the current PC profile; ManageSaves opens the browser. */
    if (!gt4_save_active()) {
        menu_request_project(m, "save");
        *ret = ah_int(2);
        return 0;
    }
    int ok = gt4_save_capture(vm, game_object(m), 1) && gt4_save_store();
    menu_play_effect(m, ok ? "select" : "disable");
    const char *message = ok ? "Game saved." : gt4_save_error();
    if (m->present_dialog)
        m->present_dialog(m, "Save Game", message, 0, "OK", NULL);
    else
        plat_dialog("Save Game", message, 0);
    *ret = ah_int(2);
    return 0;
}
static int nat_enter_gtmode(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    m->gtmode_save_entry = !gt4_save_active();
    /* arcade.adc: GTmode.onActivate L488 uses ok before profile creation;
       startGTmode L585 uses return with existing game data. */
    menu_play_effect(m, m->gtmode_save_entry ? "ok" : "return");
    menu_request_project(m, m->gtmode_save_entry ? "save" : "gtmode");
    *ret = ah_int(2);
    return 0;
}
int menu_take_gtmode_save_entry(menu *m)
{
    int result = m ? m->gtmode_save_entry : 0;
    if (m)
        m->gtmode_save_entry = 0;
    return result;
}
static int nat_pc_menu_action(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    const char *action = data;
    if (!strcmp(action, "QuitGame")) {
        if (menu_confirm(m, "Exit Moon Engine", "Exit the game?")) {
            menu_save_capture(m);
            if (gt4_save_active() && gt4_save_dirty() && gt4_save_auto_enabled() &&
                !gt4_save_store())
                menu_confirm(m, "Save Game", gt4_save_error());
            else
                m->quit_requested = 1;
        }
    } else {
        menu_save_capture(m);
        if (gt4_save_active() && gt4_save_dirty() && gt4_save_auto_enabled() && !gt4_save_store()) {
            menu_confirm(m, "Save Game", gt4_save_error());
            *ret = ah_int(2);
            return 0;
        }
        m->new_career_entry = !strcmp(action, "NewCareer");
        menu_play_effect(m, "ok");
        menu_request_project(m, "save");
    }
    *ret = ah_int(2);
    return 0;
}
int menu_take_new_career(menu *m)
{
    int result = m ? m->new_career_entry : 0;
    if (m)
        m->new_career_entry = 0;
    return result;
}
int menu_take_labo_arg(menu *m, char *arg, size_t size)
{
    if (!m || !m->labo_arg[0])
        return 0;
    snprintf(arg, size, "%s", m->labo_arg);
    m->labo_arg[0] = 0;
    return 1;
}
void menu_memcard_return(const char *project)
{
    snprintf(memcard_origin, sizeof memcard_origin, "%s", project ? project : "");
}
/* labomode.ad onLoad: startPage by main.game.next_menu_arg, a card mode
   (MODE_SAVE_FULL_REPLAY: SaveReplayRoot) or a page's name */
int menu_open_labo(menu *m, const char *mode)
{
    ah_val ctx, result;
    if (!m || !mode)
        return 0;
    /* PhotoRoot stands for the empty argument (the photo_labo sequence) */
    ah_set_member(m->vm, game_object(m), "next_menu_arg",
                  ah_str(m->vm, strcmp(mode, "PhotoRoot") ? mode : ""));
    replay_theater = !strcmp(mode, "ReplayTheaterRoot");
    while (m->depth)
        finalize_page(m, m->pages[--m->depth]);
    ctx = ah_objv(m->context);
    return ah_call_member(m->vm, m->project, "onLoad", 1, &ctx, &result) >= 0;
}
const char *menu_car_name(menu *m, const char *label)
{
    return m && label ? car_name(m, label) : NULL;
}
int menu_take_quit(menu *m)
{
    int result = m ? m->quit_requested : 0;
    if (m)
        m->quit_requested = 0;
    return result;
}
static void pc_label(ui_widget *w, const char *label)
{
    if (w) {
        free(w->text_key);
        w->text_key = NULL;
        free(w->text);
        w->text = strdup(label);
    }
}
static void pc_menu_entries(menu *m, ui_widget *root)
{
    if (!strcmp(root->name, "MakerRoot")) {
        /* P: one local-profile chooser replaces two fixed card-slot buttons. */
        ui_widget *tab = ui_widget_child(root, "Tab2p"), *a = ui_widget_child(tab, "Slot1"),
                  *b = ui_widget_child(tab, "Slot2");
        if (a && b) {
            b->visible = 0;
            a->w = 224;
            ui_widget *bg = ui_widget_child(a, "bg"), *label = ui_widget_child(a, "label");
            if (bg)
                bg->w = 224;
            if (label) {
                label->w = 224;
                pc_label(label, "Player Profiles");
            }
        }
    }

    if (!strcmp(ah_module_name(m->project), "ArcadeProject") && !strcmp(root->name, "TopRoot")) {
        ui_widget *list = ui_widget_child(ui_widget_child(root, "StartScroll"), "SelectBox");
        ui_widget *sample = ui_widget_child(ui_widget_child(root, "hidden"), "GTmode");
        if (!list || !sample || ui_widget_child(list, "NewCareer"))
            return;
#ifdef __SWITCH__
        const char *names[] = {"NewCareer"}, *labels[] = {"New Career"};
#else
        const char *names[] = {"NewCareer", "QuitGame"}, *labels[] = {"New Career", "Exit Game"};
#endif
        for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
            ui_widget *item = ui_widget_copy(m->ui, sample);
            item->name = names[i];
            copy_script(m, sample, item);
            pc_label(ui_widget_child(item, "label"), labels[i]);
            ah_val arg = maybe_module(m, item), result;
            ah_call_member(m->vm, widget_module(m, root), "setButtonActor", 1, &arg, &result);
            ui_widget_append(list, item);
            menu_native_method(m, item, "onActivate", nat_pc_menu_action, (void *)names[i]);
        }
        /* SelectBox uses child order for both layout and navigation. */
        ui_widget *after = ui_widget_child(list, "GTmode");
        const char *ordered[] = {"Save", "NewCareer"};
        for (int i = 0; after && i < 2; i++) {
            ui_widget *item = ui_widget_child(list, ordered[i]);
            if (!item)
                continue;
            ui_widget_detach(item);
            ui_widget_append(list, item);
            int index = 0;
            while (index < list->nkids && list->kids[index] != after)
                index++;
            if (index < list->nkids - 1) {
                memmove(&list->kids[index + 2], &list->kids[index + 1],
                        (list->nkids - index - 2) * sizeof *list->kids);
                list->kids[index + 1] = item;
            }
            after = item;
        }
    } else if (!strcmp(ah_module_name(m->project), "OptionProject") &&
               !strcmp(root->name, "OptionRoot")) {
        ui_widget *scroll = ui_widget_child(root, "ScrollWindow");
        ui_widget *space = ui_widget_child(ui_widget_child(scroll, "clip"), "space");
        if (!space || ui_widget_child(space, "ManageSaves"))
            return;
        ui_widget *sample = NULL, *photo = NULL;
        float bottom = 0;
        for (int i = 0; i < space->nkids; i++) {
            ui_widget *item = space->kids[i];
            bottom = fmaxf(bottom, item->y + item->h);
            ui_widget *label = ui_widget_child(item, "label");
            if (!sample && label && item->w < 120)
                sample = item;
            if (label && label->text && !strcmp(label->text, "Photo Lab"))
                photo = item;
        }
        if (!sample)
            return;
        ui_widget *group_sample = ui_widget_child(ui_widget_child(root, "hidden"), "Group");
        float section_y = photo ? photo->y + photo->h + 4 : bottom + 4;
        float group_h = group_sample ? group_sample->h : 24;
        float added = group_h + sample->h + 8;
        for (int i = 0; i < space->nkids; i++)
            if (space->kids[i]->y >= section_y)
                space->kids[i]->y += added;
        space->h += added;
        if (group_sample) {
            ui_widget *group = ui_widget_copy(m->ui, group_sample);
            group->name = "PlayerProfiles";
            group->x = 0;
            group->y = section_y;
            pc_label(ui_widget_child(group, "label"), "Player Profiles");
            ui_widget_append(space, group);
        }
        ui_widget *card = ui_widget_copy(m->ui, sample);
        card->name = "ManageSaves";
        copy_script(m, sample, card);
        card->x = 0;
        card->y = section_y + group_h;
        pc_label(ui_widget_child(card, "label"), "Save Games");
        ui_load_package(m->ui, "/projects/gtmode", "HomeRoot");
        ui_widget_set_image(ui_widget_child(card, "bg"), "image/gtmode/home/garage.png");
        ui_widget_append(space, card);
        menu_native_method(m, card, "onActivate", nat_pc_menu_action, "ManageSaves");
    }
}

static void bind_save_buttons(menu *m, ui_widget *w)
{
    if (!w)
        return;
    if (w->name && !strcmp(w->name, "GTmode") && !strcmp(w->type, "Composite"))
        menu_native_method(m, w, "onActivate", nat_enter_gtmode, m);
    if (w->name && !strcmp(w->name, "Save") &&
        (!strcmp(w->type, "Button") || mp_number(w->src, "can_focus", 0)))
        menu_native_method(m, w, "onActivate", nat_open_save, m);
    for (int i = 0; i < w->nkids; i++)
        bind_save_buttons(m, w->kids[i]);
}
void menu_request_project(menu *m, const char *name)
{
    if (m && name && strlen(name) < sizeof m->next_project)
        snprintf(m->next_project, sizeof m->next_project, "%s", name);
}
int menu_confirm(menu *m, const char *title, const char *message)
{
    int r = m && m->present_dialog ? m->present_dialog(m, title, message, 1, "OK", "Cancel")
                                   : plat_dialog(title, message, 1);
    return r == 1;
}
