#ifndef MOON_SAVE_UI_H
#define MOON_SAVE_UI_H
#include "menu.h"
typedef struct {
    menu *owner;
    ui_widget *page, *entry, *keyboard, *name_label, *cards[3], *pagination;
    ui_widget *welcome, *welcome_arrow;
    int transfer_tools;
    int first_run, onboarding, welcome_step, autosave_explained;
    int root, name_root, editing, typed, rename, slot, first, enter_gtmode, caps;
    char name[97];
} save_ui;
void save_ui_first_run(save_ui *s);
void save_ui_new_career(save_ui *s);
void save_ui_attach(save_ui *s, menu *m);
int save_ui_event(save_ui *s, int key, const char *text);
void save_ui_update(save_ui *s);
void save_ui_transfer_tools(save_ui *s, int enabled);
void save_ui_name(save_ui *s, const char *name);
#endif
