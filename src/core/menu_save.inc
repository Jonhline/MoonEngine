#include "save_ps2.h"

static ui_widget *save_access_widget(ui_widget *w)
{
    if (w->name && !strcmp(w->name, "Memorycard"))
        return w;
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *found = save_access_widget(w->kids[i]);
        if (found)
            return found;
    }
    return NULL;
}
void menu_save_access(menu *m, int mode)
{
    if (!m || menu_active_root(m) < 0)
        return;
    ui_widget *root = ui_root_widget(m->ui, menu_active_root(m));
    ui_widget *indicator = save_access_widget(root);
    if (!indicator && mode) {
        ui_widget *source = ui_widget_from_project(m->ui, menu_active_root(m), "boot", "LoadRoot");
        indicator = source ? save_access_widget(source) : NULL;
        if (indicator) {
            ui_widget_detach(indicator);
            ui_widget_append(root, indicator);
        }
    }
    if (!indicator)
        return;
    indicator->visible = 1;
    /* share/memorycard.ad L26..50: clear both actors/faces, then bind
       MBlinkActor with period 0.1 to the selected access face. This also
       serves host pages that do not import memorycard.ad themselves. */
    const char *names[] = {"save", "load"};
    for (int i = 0; i < 2; i++) {
        ui_widget *face = ui_widget_child(indicator, names[i]);
        if (!face)
            continue;
        /* The same native hard-disk artwork used by Replay Theater. */
        ui_widget_set_image(face, "image/labo/memory_header.png");
        ah_obj *module = widget_module(m, face);
        for (menu_actor *old = m->actors; old; old = old->next)
            if (old->kind == 2 && old->target == module)
                old->running = 0;
        ah_set_member(m->vm, module, "actor", ah_nil());
        face->visible = mode == i + 1;
        if (face->visible) {
            ah_val target = ah_objv(module), actor;
            if (nat_actor(m->vm, NULL, "MBlinkActor", 1, &target, &actor) == 0) {
                ah_set_member(m->vm, actor.u.o, "period", ah_float(0.10000000149011612f));
                ah_set_member(m->vm, module, "actor", actor);
                face->opacity = 1;
            }
        }
    }
}
void menu_set_profile_change_presenter(menu *m, int (*fn)(menu *, int, int, const char *))
{
    if (m)
        m->present_profile_change = fn;
}
int menu_save_profile_change(menu *m, int operation, int slot, const char *value)
{
    if (m && m->present_profile_change) {
        menu_save_access(m, 1);
        int ok = m->present_profile_change(m, operation, slot, value);
        menu_save_access(m, 0);
        if (ok && operation==MENU_PROFILE_IMPORT_PACKAGE) menu_storage_library_changed();
        return ok;
    }
    switch (operation) {
    case MENU_PROFILE_CREATE:
        return gt4_save_create(slot, value);
    case MENU_PROFILE_RENAME:
        return gt4_save_rename(value);
    case MENU_PROFILE_IMPORT:
        return gt4_save_import_ps2(slot, value);
    default:
        return 0;
    }
}
void menu_set_save_presenter(menu *m, int (*fn)(menu *))
{
    if (m)
        m->present_save = fn;
}
void menu_set_load_presenter(menu *m, int (*fn)(menu *, int))
{
    if (m)
        m->present_load = fn;
}
int menu_save_load(menu *m, int slot)
{
    if (!m || !m->present_load)
        return gt4_save_load_slot(slot);
    menu_save_access(m, 2);
    int ok = m->present_load(m, slot);
    menu_save_access(m, 0);
    return ok;
}
int menu_save_store(menu *m)
{
    if (!m || !m->present_save)
        return gt4_save_store();
    menu_save_access(m, 1);
    int ok = m->present_save(m);
    menu_save_access(m, 0);
    return ok;
}

static int nat_pc_autosave(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    menu *m = data;
    int ok = gt4_save_capture(vm, game_object(m), 1);
    if (ok && gt4_save_active() && gt4_save_auto_enabled())
        ok = menu_save_store(m);
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
    int ok = gt4_save_capture(vm, game_object(m), 1) && menu_save_store(m);
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
                !menu_save_store(m))
                menu_confirm(m, "Save Game", gt4_save_error());
            else
                m->quit_requested = 1;
        }
    } else {
        menu_save_capture(m);
        if (gt4_save_active() && gt4_save_dirty() && gt4_save_auto_enabled() &&
            !menu_save_store(m)) {
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
/* P: interface.controller_tiles; 0 keeps the disc's Controllers section. */
static int controller_tiles;
void menu_set_controller_tiles(int on)
{
    controller_tiles = !!on;
}
/* P: Controllers by player. On the PC every wheel model reads the same way and each port
   takes a pad or a wheel, so the six per-model wheel tiles (Driving Force Pro, GT Force,
   Driving Force; 1P and 2P) go; the DUALSHOCK 2 tiles stay, as Controller 1P/2P, with
   their own page and script. Vibration and Steering are unchanged. The tiles are told
   apart by their disc image (OptionPage's), so the labels' language does not matter. */
static void controller_section(ui_widget *space)
{
    ui_widget *keep = NULL;
    float row = -1, last = -1;
    for (int i = 0; i < space->nkids; i++) {
        ui_widget *w = space->kids[i], *bg = ui_widget_child(w, "bg");
        const char *image = bg && bg->image_path ? bg->image_path : "";
        if (strstr(image, "Controller.png"))
            keep = w;
        if (strstr(image, "CTR_port") || strstr(image, "CGR_port") || strstr(image, "JGR_port")) {
            w->visible = 0;
            last = fmaxf(last, w->y);
        }
        ui_widget *label = ui_widget_child(w, "label");
        const char *port = strstr(image, "DS2_port0") ? "1P" : strstr(image, "DS2_port1") ? "2P" : NULL;
        if (port && label && label->text) {
            const char *line = strchr(label->text, '\n'); /* "DUALSHOCK®2 \nbutton config 1P" */
            char text[96];
            snprintf(text, sizeof text, "Controller%s", line ? line : port);
            pc_label(label, text);
        }
    }
    if (!keep || last <= keep->y)
        return;
    row = keep->y;
    float shift = last - row; /* the wheels' second row, now empty */
    for (int i = 0; i < space->nkids; i++)
        if (space->kids[i]->visible && space->kids[i]->y > last)
            space->kids[i]->y -= shift;
    space->h -= shift;
}
/* Preserve the built page, including PC sections and translated labels. Reapply
   from this baseline rather than accumulating row shifts on each toggle. */
struct controller_layout_entry {
    ui_widget *widget;
    float y;
    int visible;
    char *label;
};
static void controller_layout_free(menu *m)
{
    for (int i = 0; i < m->controller_layout_count; i++)
        free(m->controller_layout[i].label);
    free(m->controller_layout);
    m->controller_layout = NULL;
    m->controller_layout_count = 0;
    m->controller_layout_space = NULL;
}
static void controller_layout_update(menu *m, ui_widget *root)
{
    if (!m || strcmp(m->project_label, "option") || !root || strcmp(root->name, "OptionRoot"))
        return;
    ui_widget *space = ui_widget_child(ui_widget_child(ui_widget_child(root, "ScrollWindow"), "clip"), "space");
    if (!space || !ui_widget_child(space, "ManageSaves"))
        return;
    if (!m->controller_layout) {
        m->controller_layout = calloc((size_t)space->nkids, sizeof *m->controller_layout);
        if (!m->controller_layout)
            return;
        m->controller_layout_space = space;
        m->controller_layout_height = space->h;
        m->controller_layout_count = space->nkids;
        m->controller_layout_mode = -1;
        for (int i = 0; i < space->nkids; i++) {
            struct controller_layout_entry *e = &m->controller_layout[i];
            ui_widget *w = space->kids[i], *label = ui_widget_child(w, "label");
            e->widget = w;
            e->y = w->y;
            e->visible = w->visible;
            e->label = label && label->text ? strdup(label->text) : NULL;
        }
    }
    if (m->controller_layout_mode == controller_tiles)
        return;
    for (int i = 0; i < m->controller_layout_count; i++) {
        struct controller_layout_entry *e = &m->controller_layout[i];
        e->widget->y = e->y;
        e->widget->visible = e->visible;
        if (e->label)
            pc_label(ui_widget_child(e->widget, "label"), e->label);
    }
    space->h = m->controller_layout_height;
    if (controller_tiles)
        controller_section(space);
    m->controller_layout_mode = controller_tiles;
    ui_widget *focus = m->focus[root->root];
    if (focus && !focus->visible) {
        for (int i = 0; i < space->nkids; i++) {
            ui_widget *bg = ui_widget_child(space->kids[i], "bg");
            if (bg && bg->image_path && strstr(bg->image_path, "DS2_port0")) {
                set_focus(m, space->kids[i]);
                scroll_to(space->kids[i], 1);
                break;
            }
        }
    }
}
/* P: a "PC Settings" section under General, built like the game's own: a Group header and
   one row of tiles, copies of the Screen tile with its script, so each opens the Screen
   page; the page shows the tile's own settings (video_settings.c reads which tile). */
static void pc_settings_section(menu *m, ui_widget *space, ui_widget *group_sample)
{
    static const char *const general[] = {"Race", "Replay", "Screen", "Picture Quality", "Sound"};
    static const char *const names[] = {"PcGraphics", "PcLighting", "PcControls", "PcMusic",
                                        "PcPresets"},
                             *const labels[] = {"Graphics", "Light & Shadow", "Controls", "Music",
                                                "Presets"},
                             /* the disc's own art: Options' camera, album photo (light) and
                                wheel, the Jukebox's Library discs, the Options' GT emblem */
                             *const icons[] = {"image/option/Camera.png",
                                               "image/option/Album.png",
                                               "image/option/Steering.png",
                                               "image/labo/folder_mus.png",
                                               "image/option/Misc.png"};
    ui_widget *row[5] = {0}, *screen = NULL;
    int n = 0;
    float top = 0;
    for (int i = 0; i < space->nkids && n < 5; i++) {
        ui_widget *label = ui_widget_child(space->kids[i], "label");
        for (int k = 0; label && label->text && k < 5; k++)
            if (!strcmp(label->text, general[k])) {
                row[n++] = space->kids[i];
                top = fmaxf(top, space->kids[i]->y + space->kids[i]->h);
                if (k == 2)
                    screen = space->kids[i];
            }
    }
    if (!screen || n < 5 || !group_sample)
        return;
    for (int a = 1; a < n; a++) /* the row's places, left to right */
        for (int b = a; b > 0 && row[b - 1]->x > row[b]->x; b--) {
            ui_widget *t = row[b];
            row[b] = row[b - 1];
            row[b - 1] = t;
        }
    top += 4;
    float group_h = group_sample->h, added = group_h + screen->h + 8;
    for (int i = 0; i < space->nkids; i++)
        if (space->kids[i]->y >= top)
            space->kids[i]->y += added;
    space->h += added;
    ui_load_package(m->ui, "/projects/slide", "MusicRoot");
    ui_widget *group = ui_widget_copy(m->ui, group_sample);
    group->name = "PcSettings";
    group->x = 0;
    group->y = top;
    pc_label(ui_widget_child(group, "label"), "PC Settings");
    ui_widget_append(space, group);
    for (int k = 0; k < 5; k++) {
        ui_widget *card = ui_widget_copy(m->ui, screen);
        card->name = names[k];
        copy_script(m, screen, card);
        card->x = row[k]->x;
        card->y = top + group_h;
        pc_label(ui_widget_child(card, "label"), labels[k]);
        ui_text_adjust_scale(m->ui, ui_widget_child(card, "label")); /* one line, as authored */
        ui_widget_set_image(ui_widget_child(card, "bg"), icons[k]);
        ui_widget_append(space, card);
        /* back from the Screen page, the script focuses the item of its page (Screen);
           back from a PC tile, that tile instead */
        if (m->focus[screen->root] == screen && !strcmp(m->last_focus[screen->root], names[k])) {
            if (screen->root == menu_active_root(m))
                set_focus(m, card);
            else
                m->focus[screen->root] = card;
            scroll_to(card, 1);
        }
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
        pc_settings_section(m, space, group_sample);
        controller_layout_free(m);
        controller_layout_update(m, root);
    }
}

static void bind_save_buttons(menu *m, ui_widget *w)
{
    if (!w)
        return;
    if (w->name && !strcmp(w->name, "GTmode") && !strcmp(w->type, "Composite"))
        menu_native_method(m, w, "onActivate", nat_enter_gtmode, m);
    /* gtmode.ad L4347..4350 owns confirmation, progress and completion.
       Replace host-only buttons, never the disc Home Save handler. */
    if (strcmp(m->project_label, "gtmode") && w->name && !strcmp(w->name, "Save") &&
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
