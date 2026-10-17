#ifndef MOON_DESKTOP_INPUT_H
#define MOON_DESKTOP_INPUT_H
#include "menu.h"
#include "plat_sdl.h"
/* P: presentation/input boundary only. Existing pad/key adapters remain intact. */
void desktop_input_hints(menu *m, int enabled, const plat_key_event *ev);
int desktop_input(menu *m, int enabled, plat_key_event *ev);
/* Returns -1 for no selection, otherwise the hovered/clicked button index.
   result: 0 hover, 1 accept, 2 back. Uses the dialog's own painted widgets. */
int desktop_dialog_input(ui_project *ui, int enabled, ui_widget **items, int count,
                         plat_key_event *ev, int *result);
#endif
