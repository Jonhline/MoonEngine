#include "save_ui.h"
#include "save.h"
#include "save_package.h"
#include "save_ps2.h"
#include "ps2_media.h"
#include "plat_sdl.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
static save_ui *screen;
/* Hidden-window fixtures inject only the host file choice, not transfer logic. */
static int package_fixture_path(char path[1200])
{
    const char *hidden=getenv("MOON_WINDOW_HIDDEN");
    if (!hidden || strcmp(hidden,"1")) return 0;
#ifdef _WIN32
    /* Match the real wide-character picker, including non-ASCII paths. */
    wchar_t value[1200];DWORD count=GetEnvironmentVariableW(L"MOON_TEST_SAVE_PACKAGE",value,1200);
    return count && count<1200 && WideCharToMultiByte(CP_UTF8,0,value,-1,path,1200,NULL,NULL)>0;
#else
    const char *value=getenv("MOON_TEST_SAVE_PACKAGE");
    if (!value || !*value || strlen(value)>=1200) return 0;
    snprintf(path,1200,"%s",value);return 1;
#endif
}
static int choose_export(char path[1200])
{
    if (package_fixture_path(path)) return 1;
#ifdef _WIN32
    wchar_t chosen[1200]=L"Player.moonplayer";
    char suggested[160]; snprintf(suggested,sizeof suggested,"%s",gt4_save_player());
    for (char *p=suggested;*p;p++) if (strchr("<>:\"/\\|?*",*p)) *p='_';
    if (suggested[0]) strncat(suggested,".moonplayer",sizeof suggested-strlen(suggested)-1);
    if (suggested[0]) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,suggested,-1,chosen,1200);
    OPENFILENAMEW dialog={0}; dialog.lStructSize=sizeof dialog;
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(plat_sdl_window(),&info)) dialog.hwndOwner=info.info.win.window;
    dialog.lpstrFilter=L"Complete Moon Engine player package\0*.moonplayer\0";
    dialog.lpstrFile=chosen; dialog.nMaxFile=1200;
    dialog.lpstrDefExt=L"moonplayer"; dialog.lpstrTitle=L"Export complete player, photos, replays, films and lists";
    dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&dialog)) return 0;
    return WideCharToMultiByte(CP_UTF8,0,chosen,-1,path,1200,NULL,NULL)!=0;
#else
    (void)path;
    return 0;
#endif
}
static int action(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static void heading(save_ui *, ui_widget *, const char *);
static void welcome_question(save_ui *s);
static int enter_cards(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int welcome_answer(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int complete_first_run(save_ui *s)
{
    if (!s->onboarding)
        return 0;
    if (!s->autosave_explained && gt4_save_active() && gt4_save_auto_enabled()) {
        /* boot.ad ExplainRoot L780..785: notice, icon example, safe removal. */
        s->first_run = 1;
        s->welcome_step = 4;
        if (menu_active_root(s->owner) == s->root) {
            ah_val result;
            enter_cards(NULL, NULL, s, 0, NULL, &result);
        } else
            menu_native_page(s->owner, s->root);
        return 1;
    }
    s->first_run = s->onboarding = 0;
    menu_request_project(s->owner, menu_option_number(s->owner, "opening_after_autoload", 1)
                                       ? "boot:MovieRoot"
                                       : "arcade");
    return 1;
}
static void text(ui_widget *w, const char *s)
{
    if (w) {
        free(w->text);
        w->text = strdup(s);
    }
}
static void tip(save_ui *s, const char *value)
{
    ui_set_tooltips(menu_active_root(s->owner) == s->name_root ? s->entry : s->page, value);
}
static int none(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    *ret = ah_int(0);
    return 0;
}
/* P: Caps Lock (and Shift held) on the PC keyboard, as the text it types. */
static int typing_capitals(const save_ui *s)
{
    SDL_Keymod mod = SDL_GetModState();
    return s->caps ^ !!(mod & KMOD_CAPS) ^ !!(mod & KMOD_SHIFT);
}
/* P: the keyboard shows the capitals that the next key types: the first letter (append()
   capitalizes it) and every letter while caps is on; otherwise lower case. The focus stays
   on the same key across the two layouts. */
static void show_case(save_ui *s)
{
    if (!s->keyboard)
        return;
    ui_widget *lower = ui_widget_child(s->keyboard, "LowerCase"),
              *upper = ui_widget_child(s->keyboard, "UpperCase");
    if (!lower || !upper)
        return;
    int capitals = typing_capitals(s) || !s->name[0];
    if (upper->visible == capitals && lower->visible == !capitals)
        return;
    const char *focus = s->editing ? menu_focus_name(s->owner) : NULL;
    char key[64] = "";
    if (focus)
        snprintf(key, sizeof key, "%s", focus);
    lower->visible = !capitals;
    upper->visible = capitals;
    ui_widget *same = key[0] ? ui_widget_child(capitals ? upper : lower, key) : NULL;
    if (s->editing)
        menu_native_focus(s->owner,
                          same ? same : ui_widget_child(capitals ? upper : lower, "key_q"));
}
void save_ui_name(save_ui *s, const char *name)
{
    snprintf(s->name, sizeof s->name, "%s", name ? name : "");
    text(s->name_label, s->name);
    show_case(s);
}
static void refresh(save_ui *s)
{
    menu_save_refresh(s->owner);
    for (int i = 0; i < 3; i++) {
        int slot = s->first + i + 1;
        ui_widget *card = s->cards[i];
        card->visible = slot <= GT4_SAVE_SLOTS;
        gt4_save_info info;
        int status = gt4_save_info_get(slot, &info);
        char title[160], details[192];
        snprintf(title, sizeof title, "%s",
                 status > 0   ? info.name
                 : status < 0 ? "Savegame Unavailable"
                              : "Create New Savegame");
        if (status > 0) {
            time_t t = (time_t)info.timestamp;
            struct tm *local = localtime(&t);
            char date[80] = "";
            if (local)
                strftime(date, sizeof date, "%d/%m/%Y  %H:%M", local);
            snprintf(details, sizeof details, "Slot %d  |  %s%s%s", slot,
                     slot == gt4_save_active() ? "Current player  |  " : "", date,
                     status == 2 ? "  (backup)" : "");
        } else
            snprintf(details, sizeof details, "Slot %d  |  %s", slot,
                     status < 0 ? "Keep this file for recovery" : "Start a new player profile");
        text(ui_widget_child(card, "label"), title);
        text(ui_widget_child(card, "details"), details);
    }
    char page[64];
    snprintf(page, sizeof page, "%d / 3", s->first / 3 + 1);
    text(s->pagination, page);
}
static int enter_cards(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                       ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    save_ui *s = data;
    s->editing = 0;
    SDL_StopTextInput();
    for (int i = 0; i < s->page->nkids; i++) {
        ui_widget *child = s->page->kids[i];
        int decoration =
            child->name && (!strcmp(child->name, "bg") || !strcmp(child->name, "ToolTip"));
        child->visible = decoration ? 1 : child == s->welcome ? s->first_run : !s->first_run;
        if (child->name && !strcmp(child->name, "Export"))
            child->visible = !s->first_run && s->transfer_tools;
    }
    if (s->first_run) {
        ui_widget *title = ui_widget_child(s->page, "Title");
        if (title)
            title->visible = 0;
        welcome_question(s);
        *ret = ah_int(0);
        return 0;
    }
    heading(s, s->page, "Save Games");
    refresh(s);
    menu_native_focus(s->owner, s->cards[0]);
    tip(s, "Select a savegame to load, or an empty card to create a new player.");
    *ret = ah_int(0);
    return 0;
}
static int enter_name(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                      ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    save_ui *s = data;
    s->editing = 1;
    s->typed = 0;
    SDL_StartTextInput();
    text(s->name_label, s->name);
    ui_widget *lower = ui_widget_child(s->keyboard, "LowerCase"),
              *upper = ui_widget_child(s->keyboard, "UpperCase");
    s->caps = 0;
    lower->visible = 1;
    upper->visible = 0;
    menu_native_focus(s->owner, ui_widget_child(lower, "key_q"));
    show_case(s);
    tip(s, "Enter your name using the keyboard. Select ENTER to confirm; Escape cancels.");
    *ret = ah_int(0);
    return 0;
}
static void backspace(save_ui *s)
{
    size_t n = strlen(s->name);
    if (n) {
        do {
            n--;
        } while (n && ((unsigned char)s->name[n] & 0xc0) == 0x80);
        s->name[n] = 0;
        text(s->name_label, s->name);
        show_case(s);
    }
}
static void append(save_ui *s, const char *input)
{
    size_t n = strlen(s->name), add = strlen(input);
    if (n + add >= sizeof s->name) {
        tip(s, "The player name is too long.");
        return;
    }
    char candidate[97];
    snprintf(candidate, sizeof candidate, "%s%s", s->name, input);
    if (!n && candidate[0] >= 'a' && candidate[0] <= 'z')
        candidate[0] -= 'a' - 'A';
    int chars = 0;
    for (const unsigned char *p = (const unsigned char *)candidate; *p; p++)
        if ((*p & 0xc0) != 0x80)
            chars++;
    if (chars > 24) {
        tip(s, "Maximum 24 characters.");
        return;
    }
    save_ui_name(s, candidate);
    menu_play_effect(s->owner, "param");
}
static void name_done(save_ui *s)
{
    if (!gt4_save_valid_name(s->name)) {
        tip(s, "Enter a player name (1 to 24 characters).");
        menu_play_effect(s->owner, "disable");
        return;
    }
    menu_save_capture(s->owner);
    int ok = menu_save_profile_change(s->owner, s->rename ? MENU_PROFILE_RENAME : MENU_PROFILE_CREATE,
                                      s->slot, s->name);
    if (!ok) {
        tip(s, gt4_save_error());
        menu_play_effect(s->owner, "disable");
        return;
    }
    s->editing = 0;
    SDL_StopTextInput();
    menu_play_effect(s->owner, "select");
    if (complete_first_run(s))
        return;
    if (s->enter_gtmode) {
        s->enter_gtmode = 0;
        menu_request_project(s->owner, "gtmode");
    } else
        menu_native_page(s->owner, s->root);
}
static int key_action(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                      ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    save_ui *s = screen;
    const char *key = data;
    if (!strcmp(key, "key_enter"))
        name_done(s);
    else if (strstr(key, "backspace")) {
        backspace(s);
        menu_play_effect(s->owner, "cancel");
    } else if (strstr(key, "caps")) {
        s->caps = !s->caps;
        show_case(s);
        menu_play_effect(s->owner, "param");
    } else if (!strncmp(key, "key_", 4) && key[4]) {
        char c[2] = {key[4], 0};
        if (typing_capitals(s) && c[0] >= 'a' && c[0] <= 'z')
            c[0] -= 32;
        append(s, c);
    }
    *ret = ah_int(2);
    return 0;
}
static int cancel_name(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                       ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    save_ui *s = data;
    s->editing = 0;
    SDL_StopTextInput();
    menu_native_page(s->owner, s->root);
    menu_play_effect(s->owner, "cancel");
    *ret = ah_int(2);
    return 0;
}
static void bind_keys(save_ui *s, ui_widget *w)
{
    if (w->name && !strncmp(w->name, "key_", 4))
        menu_native_method(s->owner, w, "onActivate", key_action, (void *)w->name);
    for (int i = 0; i < w->nkids; i++)
        bind_keys(s, w->kids[i]);
}
static int choose_import(char path[1200], int media)
{
    if (!media && package_fixture_path(path)) return 1;
#ifdef _WIN32
    wchar_t chosen[1200] = L"";
    OPENFILENAMEW open = {0};
    open.lStructSize = sizeof open;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(plat_sdl_window(), &info))
        open.hwndOwner = info.info.win.window;
    open.lpstrFilter = media ? L"GT4 media archives\0*.psu;*.ps2\0All files\0*.*\0"
                             : L"GT4 savegames\0*.moonplayer;*.ogt4;*.psu;*.ps2;*GAMEDATA\0All files\0*.*\0";
    open.lpstrFile = chosen;
    open.nMaxFile = 1200;
    open.lpstrTitle = media ? L"Import GT4 Media" : L"Import GT4 Savegame";
    open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&open))
        return 0;
    return WideCharToMultiByte(CP_UTF8, 0, chosen, -1, path, 1200, NULL, NULL) != 0;
#else
    (void)path;
    return 0;
#endif
}
static int action(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)argc;
    (void)argv;
    (void)self;
    save_ui *s = screen;
    const char *op = data;
    int ok = 0;
    if (!strcmp(op, "Begin")) {
        s->slot = 0;
        for (int i = 1; i <= GT4_SAVE_SLOTS; i++) {
            gt4_save_info info;
            if (gt4_save_info_get(i, &info) == 0) {
                s->slot = i;
                break;
            }
        }
        if (!s->slot) {
            tip(s, "No empty profile slots. Select an existing player profile.");
            goto end;
        }
        s->rename = 0;
        save_ui_name(s, "");
        menu_native_page(s->owner, s->name_root);
        menu_play_effect(s->owner, "select");
        goto end;
    }
    if (!strcmp(op, "Profiles")) {
        s->first_run = 0;
        enter_cards(vm, self, s, 0, NULL, ret);
        goto end;
    }
    if (!strcmp(op, "Skip")) {
        complete_first_run(s);
        menu_play_effect(s->owner, "select");
        goto end;
    }
    if (!strcmp(op, "Back") && s->onboarding) {
        if (s->first_run)
            return welcome_answer(vm, self, NULL, argc, argv, ret);
        s->first_run = 1;
        s->welcome_step = 0;
        enter_cards(vm, self, s, 0, NULL, ret);
        goto end;
    }
    if (!strcmp(op, "Back")) {
        ah_val main, game, origin;
        const char *project = "arcade";
        if (ah_get_member(vm, ah_root(vm), "main", &main) &&
            ah_get_member(vm, main.u.o, "game", &game) &&
            ah_get_member(vm, game.u.o, "last_menu_project", &origin) && ah_cstr(origin) &&
            strcmp(ah_cstr(origin), "save"))
            project = ah_cstr(origin);
        menu_request_project(s->owner, project);
        menu_play_effect(s->owner, "cancel");
        goto end;
    }
    if (!strcmp(op, "Previous") || !strcmp(op, "Next")) {
        s->first = (s->first + (!strcmp(op, "Next") ? 3 : 6)) % 9;
        refresh(s);
        menu_native_focus(s->owner, s->cards[0]);
        menu_play_effect(s->owner, "cursor");
        goto end;
    }
    if (!strcmp(op, "Card")) {
        int slot = s->slot;
        gt4_save_info info;
        int status = gt4_save_info_get(slot, &info);
        if (status < 0) {
            tip(s, "This savegame could not be read. Its recovery files were preserved.");
            goto end;
        }
        if (gt4_save_dirty() &&
            !menu_confirm(s->owner, "Save Game", "Discard unsaved changes and switch player?"))
            goto end;
        if (!status) {
            s->rename = 0;
            menu_native_page(s->owner, s->name_root);
            menu_play_effect(s->owner, "select");
            goto end;
        }
        ok = menu_save_load(s->owner, slot);
    } else if (!strcmp(op, "Save")) {
        menu_save_capture(s->owner);
        ok = menu_save_store(s->owner);
    } else if (!strcmp(op,"Export")) {
        if (!s->transfer_tools || !gt4_save_active()) {
            tip(s,"Load a player profile first."); goto end;
        }
        menu_save_capture(s->owner);
        if (gt4_save_dirty()) {
            if (!menu_confirm(s->owner,"Export Savegame","Save current progress before exporting this player?")) goto end;
            if (!menu_save_store(s->owner)) {tip(s,gt4_save_error()); goto end;}
        }
        char path[1200];
        if (!choose_export(path)) goto end;
        ok=menu_save_profile_change(s->owner,MENU_PROFILE_EXPORT_PACKAGE,gt4_save_active(),path);
    } else if (!strcmp(op, "Rename")) {
        if (!gt4_save_active()) {
            tip(s, "Load a player profile first.");
            goto end;
        }
        gt4_save_info info;
        gt4_save_info_get(gt4_save_active(), &info);
        save_ui_name(s, info.name);
        s->rename = 1;
        s->slot = gt4_save_active();
        menu_native_page(s->owner, s->name_root);
        menu_play_effect(s->owner, "select");
        goto end;
    } else if (!strcmp(op, "Delete")) {
        if (!gt4_save_active()) {
            tip(s, "Load the player profile you want to remove.");
            goto end;
        }
        if (!menu_confirm(s->owner, "Delete Savegame",
                          "Remove the current player? A recovery copy will be kept."))
            goto end;
        ok = gt4_save_delete_slot(gt4_save_active());
    } else if (!strcmp(op, "Media")) {
        char path[1200];
        if (!choose_import(path, 1))
            goto end;
        gt4_media_import report;
        int imported = menu_import_ps2_media(s->owner, path, &report);
        char message[512];
        if (imported)
            snprintf(
                message, sizeof message,
                "Imported: %d replays, %d photos, %d films. Open films in Photo Lab to develop. Skipped: %d.",
                report.replays, report.photos, report.films, report.skipped);
        else
            snprintf(message, sizeof message, "%s", report.error);
        tip(s, message);
        menu_play_effect(s->owner, imported ? "select" : "disable");
        goto end;
    } else if (!strcmp(op, "Import")) {
        int slot = 0;
        for (int i = 1; i <= 8; i++) {
            gt4_save_info info;
            if (!gt4_save_info_get(i, &info)) {
                slot = i;
                break;
            }
        }
        if (!slot) {
            tip(s, "All save slots are occupied.");
            goto end;
        }
        if (gt4_save_dirty() &&
            !menu_confirm(s->owner, "Import Savegame",
                          "Discard unsaved changes and load the imported player?"))
            goto end;
        char path[1200];
        if (!choose_import(path, 0))
            goto end;
        menu_save_capture(s->owner);
        ok = menu_save_profile_change(s->owner, moon_save_package_is_file(path) ? MENU_PROFILE_IMPORT_PACKAGE : MENU_PROFILE_IMPORT, slot, path);
        if (ok)
            s->first = ((slot - 1) / 3) * 3;
    }
    if (ok && (!strcmp(op, "Card") || !strcmp(op, "Import")) && complete_first_run(s))
        goto end;
    if (ok && s->enter_gtmode && (!strcmp(op, "Card") || !strcmp(op, "Import"))) {
        s->enter_gtmode = 0;
        menu_play_effect(s->owner, "select");
        menu_request_project(s->owner, "gtmode");
        goto end;
    }
    if (ok) {
        refresh(s);
        tip(s, gt4_save_error()[0]     ? gt4_save_error()
               : !strcmp(op, "Save")   ? "Game saved successfully."
               : !strcmp(op, "Export") ? "Complete player package exported: progress, shared photos, replays, films and their lists."
               : !strcmp(op, "Delete") ? "Savegame removed. A recovery copy is in the save folder."
                                       : "Player loaded successfully.");
        menu_play_effect(s->owner, "select");
    } else {
        tip(s, gt4_save_error());
        menu_play_effect(s->owner, "disable");
    }
end:
    *ret = ah_int(2);
    return 0;
}
static int card_activate(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    screen->slot = screen->first + (int)(intptr_t)data + 1;
    return action(vm, self, "Card", argc, argv, ret);
}
void save_ui_update(save_ui *s)
{
    if (s->editing)
        show_case(s);
}
int save_ui_event(save_ui *s, int key, const char *input)
{
    if (!s->editing)
        return 0;
    if (input && *input) {
        s->typed = 1;
        append(s, input);
        return 1;
    }
    if (key == SDLK_BACKSPACE) {
        backspace(s);
        menu_play_effect(s->owner, "cancel");
        return 1;
    }
    if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && s->typed) {
        name_done(s);
        return 1;
    }
    return 0;
}
/* Reuse Options' authored tile: its bands, rounded outline and shadow are baked in. */
static ui_widget *button(save_ui *s, ui_widget *parent, const char *name, const char *label,
                         float x, float y, float width, float height, int card)
{
    ui_project *ui = menu_ui(s->owner);
    if (!card) {
        ui_widget *languages = ui_root_widget(ui, 1), *box = ui_widget_child(languages, "VBox"),
                  *sample = ui_widget_child(box, "GB");
        ui_widget *b = ui_widget_copy(ui, sample);
        b->name = name;
        b->x = x;
        b->y = y;
        b->w = width;
        b->h = height;
        b->visible = 1;
        for (int i = 0; i < b->nkids; i++) {
            b->kids[i]->w = width;
            b->kids[i]->h = height;
        }
        ui_widget *face = ui_widget_child(b, "label");
        face->scale_x = face->scale_y = 0.7f;
        text(face, label);
        ui_text_adjust_scale(ui, face);
        ui_widget_append(parent, b);
        return b;
    }
    char description[2500];
    snprintf(
        description, sizeof description,
        "Composite { name string {\"%s\"} can_focus digit {1} geometry rectangle {%g %g %g %g} cursor_align digit {0} children [5] {"
        "ImageFace {name string {\"bg\"} image_path string {\"image/option/opt_window2L.png\"} slice rectangle {4 0 4 0} }"
        "ColorFace {name string {\"body\"} geometry rectangle {80 9 %g %g} color [1] {RGBA {154 154 161 255}} }"
        "ImageFace {name string {\"icon\"} geometry rectangle {16 17 58 54} image_path string {\"image/gtmode/home/status.png\"} }"
        "TextFace {name string {\"label\"} geometry rectangle {88 14 %g 30} text_color RGBA {0 0 0 255} font string {\"hm_16\"} align digit {3} scale_x digit {0.95} scale_y digit {0.95} }"
        "TextFace {name string {\"details\"} geometry rectangle {88 46 %g 22} text_color RGBA {45 45 48 255} font string {\"hm_16\"} align digit {3} scale_x digit {0.62} scale_y digit {0.62} } } }",
        name, x, y, width, height, width - 84, height - 14, width - 104, width - 104);
    ui_widget *b = ui_widget_from_text(ui, s->name_root, description);
    if (b) {
        ui_widget *bg = ui_widget_child(b, "bg");
        bg->w = width;
        bg->h = height;
        bg->has_geometry = 1;
        text(ui_widget_child(b, "label"), label);
        ui_widget_append(parent, b);
    }
    return b;
}
static void heading(save_ui *s, ui_widget *page, const char *label)
{
    ui_widget *old = ui_widget_child(page, "Title");
    if (old)
        ui_widget_detach(old);
    ui_widget *w = ui_widget_from_text(
        menu_ui(s->owner), s->name_root,
        "TextFace {name string {\"Title\"} geometry rectangle {30 16 580 42} font string {\"hb_24\"} align digit {1} scale_x digit {1.1} scale_y digit {1.1} shadow digit {1.5} shadow_color RGBA {0 0 0 152} }");
    text(w, label);
    ui_widget_append(page, w);
}
void save_ui_attach(save_ui *s, menu *m)
{
    memset(s, 0, sizeof *s);
    screen = s;
    s->owner = m;
    s->root = 0;
    s->name_root = 3;
    ui_project *ui = menu_ui(m);
    s->page = ui_root_widget(ui, s->root);
    s->entry = ui_root_widget(ui, s->name_root);
    s->keyboard = ui_widget_child(s->entry, "Keyboard");
    s->name_label = ui_widget_child(s->keyboard, "input");
    s->name_label->type = "TextFace";
    s->name_label->scale_x = s->name_label->scale_y = 0.9f;
    heading(s, s->entry, "Create New Savegame");
    while (s->page->nkids)
        ui_widget_detach(s->page->kids[s->page->nkids - 1]);
    for (int i = 0; i < s->entry->nkids; i++)
        if (s->entry->kids[i] != s->keyboard)
            ui_widget_append(s->page, ui_widget_copy(ui, s->entry->kids[i]));
    heading(s, s->page, "Save Games");
    ui_load_package(ui, "/projects/gtmode", "HomeRoot");
    ui_load_package(ui, "/projects/option", "OptionRoot");
    for (int i = 0; i < 3; i++) {
        const char *names[] = {"SaveCard1", "SaveCard2", "SaveCard3"};
        s->cards[i] =
            button(s, s->page, names[i], "Create New Savegame", 116, 76 + i * 92, 408, 80, 1);
        ui_text_adjust_scale(ui, ui_widget_child(s->cards[i], "label"));
        menu_native_method(m, s->cards[i], "onActivate", card_activate, (void *)(intptr_t)i);
    }
    const char *names[] = {"Previous", "Next", "Save", "Rename", "Delete", "Import", "Media", "Export"},
               *labels[] = {"<",           ">", "Save", "Rename", "Delete", "Import Savegame",
                            "Import Media", "Export"};
    float xs[] = {252, 352, 116, 260, 404, 116, 328, 260}, ys[] = {352, 352, 382, 382, 382, 414, 414, 414},
          ws[] = {36, 36, 120, 120, 120, 196, 196, 120};
    for (int i = 0; i < 8; i++) {
        ui_widget *b = button(s, s->page, names[i], labels[i], xs[i], ys[i], ws[i], 23, 0);
        menu_native_method(m, b, "onActivate", action, (void *)names[i]);
        if (!strcmp(names[i],"Export")) b->visible=0;
    }
    ui_widget *exit = ui_widget_from_text(
        ui, s->root,
        "Composite {name string {\"Back\"} can_focus digit {1} geometry rectangle {567 390 58 40} children [2] {ImageFace {name string {\"bg\"} image_path string {\"image/labo/exit.png\"}} ImageFace {name string {\"bgf\"} visible_condition digit {2} visible_compare digit {2} image_path string {\"image/labo/exit_over.png\"}}}}");
    ui_widget_append(s->page, exit);
    menu_native_method(m, exit, "onActivate", action, "Back");
    s->pagination = ui_widget_copy(ui, s->name_label);
    s->pagination->type = "TextFace";
    s->pagination->x = 290;
    s->pagination->y = 352;
    s->pagination->w = 60;
    s->pagination->h = 24;
    s->pagination->scale_x = s->pagination->scale_y = 0.65f;
    ui_widget_append(s->page, s->pagination);
    bind_keys(s, s->keyboard);
    for (int i = 0; i < 2; i++) {
        ui_widget *page = i ? s->entry : s->page;
        menu_native_method(m, page, "onLoad", none, s);
        menu_native_method(m, page, "onInitialize", none, s);
        menu_native_method(m, page, "onFinalize", none, s);
        menu_native_method(m, page, "enter", i ? enter_name : enter_cards, s);
        menu_native_method(m, page, "onCancel", i ? cancel_name : action,
                           i ? (void *)s : (void *)"Back");
    }
    menu_enter(m, s->root);
}

static void welcome_question(save_ui *s)
{
    static const char *questions[] = {
        "Would you like to create a player profile?",
        "Would you like to import a GT4 savegame?",
        "Would you like to load an existing player profile?",
        "Continue as a guest?\nYour progress will not be saved.",
        "This game uses Auto Save to save your progress.",
        "This icon flashes while your game is being saved.",
        "Do not close the game or disconnect storage while this icon is flashing.",
        "Earlier Gran Turismo save data can be transferred. Would you like instructions?",
        "You can transfer up to 100,000 credits from a US Gran Turismo 3 save, once per profile.",
        "The US edition also transfers completed B and A licenses from GT3. GT4 Prologue saves are not supported here.",
        "Place extracted GT3 data in the profile imports folder. In GT Mode, open Status and press START to transfer."};
    ui_project *ui = menu_ui(s->owner);
    ui_widget *pane = ui_widget_child(s->welcome, "Dialog");
    ui_widget *body = ui_widget_child(pane, "Message");
    ui_widget *message = ui_widget_child(body, "message");
    text(message, questions[s->welcome_step]);
    ui_text_wrap(ui, message, 300);
    ui_text_adjust_scale(ui, message);
    int explaining = s->welcome_step >= 4;
    int information = explaining && s->welcome_step != 7;
    text(ui_widget_child(ui_widget_child(pane, "Title"), "title"),
         s->welcome_step >= 7 ? "Transfer Game Data"
         : explaining         ? "Auto Save"
                              : "Welcome to Moon Engine");
    ui_widget *buttons = ui_widget_child(pane, "Buttons");
    ui_widget *pair = ui_widget_child(buttons, "YesNo");
    ui_widget *ok = ui_widget_child(buttons, "OK");
    pair->visible = !information;
    ok->visible = information;
    text(ui_widget_child(ok, "label"), "OK");
    menu_native_focus(s->owner, information ? ok : ui_widget_child(pair, "Yes"));
    if (explaining) {
        if (!s->welcome_arrow) {
            ui_widget *source = ui_widget_from_project(ui, s->root, "boot", "ExplainRoot");
            s->welcome_arrow = source ? ui_widget_child(source, "Arrow") : NULL;
            ui_widget *indicator = source ? ui_widget_child(source, "Memorycard") : NULL;
            if (indicator) {
                ui_widget *old = ui_widget_child(s->page, "Memorycard");
                if (old)
                    ui_widget_detach(old);
                ui_widget_detach(indicator);
                ui_widget_append(s->page, indicator);
            }
            if (s->welcome_arrow) {
                ui_widget_detach(s->welcome_arrow);
                if (indicator)
                    /* boot.mproject Arrow/bg: x=8, width=22. The native
                       30px disk body (ui.c disk_cover) spans x=4.5..20.5.
                       Align visible centres, not the enclosing composites. */
                    s->welcome_arrow->x = indicator->x + 12.5f - (8 + 22 * .5f);
                ui_widget_append(s->page, s->welcome_arrow);
            }
        }
        if (s->welcome_arrow)
            s->welcome_arrow->visible = s->welcome_step >= 5 && s->welcome_step <= 6;
        menu_save_access(s->owner, s->welcome_step >= 5 && s->welcome_step <= 6 ? 1 : 0);
    }
    tip(s, explaining             ? ""
           : s->welcome_step == 0 ? "Choose No to import an existing GT4 savegame instead."
           : s->welcome_step == 3 ? "Guest play does not create a savegame."
                                  : "Choose Yes to continue, or No for another way to start.");
}
static int welcome_answer(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    save_ui *s = screen;
    if (s->welcome_step >= 4) {
        menu_save_access(s->owner, 0);
        /* boot.ad ExplainRoot L789..800 explains legacy transfer after
           the access notice. These are instructions, not a transfer itself. */
        if (s->welcome_step < 10 && (s->welcome_step != 7 || data)) {
            s->welcome_step++;
            welcome_question(s);
        } else {
            if (s->welcome_arrow)
                s->welcome_arrow->visible = 0;
            s->autosave_explained = 1;
            complete_first_run(s);
        }
        menu_play_effect(s->owner, "ok");
        *ret = ah_int(2);
        return 0;
    }
    if (!data) {
        s->welcome_step = (s->welcome_step + 1) % 4;
        welcome_question(s);
        menu_play_effect(s->owner, "cursor");
        *ret = ah_int(2);
        return 0;
    }
    static const char *actions[] = {"Begin", "Import", "Profiles", "Skip"};
    return action(vm, self, (void *)actions[s->welcome_step], argc, argv, ret);
}
void save_ui_first_run(save_ui *s)
{
    if (!s || !s->owner)
        return;
    s->first_run = s->onboarding = 1;
    s->welcome_step = 0;
    if (!s->welcome) {
        ui_project *ui = menu_ui(s->owner);
        s->welcome = ui_widget_from_project(ui, s->root, "cursor", "ConfirmDialog");
        if (!s->welcome) {
            s->first_run = s->onboarding = 0;
            tip(s, "Welcome dialog unavailable. Select a profile or use Back for guest play.");
            return;
        }
        s->welcome->name = "Welcome";
        ui_widget *pane = ui_widget_child(s->welcome, "Dialog");
        ui_widget *title = ui_widget_child(pane, "Title");
        text(ui_widget_child(title, "title"), "Welcome to Moon Engine");
        ui_widget_child(title, "bgE")->visible = 0;
        ui_widget_child(title, "titleE")->visible = 0;
        ui_widget *body = ui_widget_child(pane, "Message");
        ui_widget_child(body, "caution")->visible = 0;
        ui_widget *message = ui_widget_child(body, "message");
        message->w = 300;
        message->h = 50;
        body->h = 100;
        ui_widget *buttons = ui_widget_child(pane, "Buttons");
        buttons->y = body->y + body->h;
        pane->h = buttons->y + buttons->h;
        pane->y = (480 - pane->h) * .5f;
        ui_widget_child(pane, "bg")->h = pane->h;
        ui_widget *ok = ui_widget_child(buttons, "OK");
        ok->visible = 0;
        menu_native_method(s->owner, ok, "onActivate", welcome_answer, (void *)(intptr_t)1);
        ui_widget *pair = ui_widget_child(buttons, "YesNo");
        pair->visible = 1;
        const char *names[] = {"No", "Yes"};
        for (int i = 0; i < 2; i++) {
            ui_widget *button = ui_widget_child(pair, names[i]);
            text(ui_widget_child(button, "label"), names[i]);
            menu_native_method(s->owner, button, "onActivate", welcome_answer, (void *)(intptr_t)i);
        }
        ui_widget_append(s->page, s->welcome);
    }
    ah_val result;
    enter_cards(NULL, NULL, s, 0, NULL, &result);
}

void save_ui_new_career(save_ui *s)
{
    if (!s || !s->owner)
        return;
    s->first_run = s->onboarding = 0;
    s->enter_gtmode = 1;
    ah_val result;
    action(NULL, NULL, "Begin", 0, NULL, &result);
}

/* Optional native transfer tools retain the old two-button footer when off. */
void save_ui_transfer_tools(save_ui *s, int enabled)
{
    if (!s || !s->page) return;
#ifndef _WIN32
    enabled=0; /* Host file selection is currently implemented on Windows. */
#endif
    enabled=enabled!=0;
    ui_widget *export_button=ui_widget_child(s->page,"Export");
    int visible = enabled && !s->first_run;
    if (s->transfer_tools==enabled && export_button && export_button->visible==visible) return;
    s->transfer_tools=enabled;
    if (export_button) export_button->visible=visible;
    ui_widget *import_button=ui_widget_child(s->page,"Import"), *media=ui_widget_child(s->page,"Media");
    if (!import_button || !media) return;
    import_button->w=enabled ? 120 : 196;
    media->x=enabled ? 404 : 328; media->w=enabled ? 120 : 196;
    for (int i=0;i<2;i++) {
        ui_widget *b=i ? media : import_button;
        for (int j=0;j<b->nkids;j++) b->kids[j]->w=b->w;
        ui_widget *label=ui_widget_child(b,"label");
        text(label,i ? (enabled ? "Media" : "Import Media") : (enabled ? "Import" : "Import Savegame"));
    }
}
