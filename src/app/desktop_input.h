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
typedef struct {
    long long id;
    int active, dragged, index;
    ui_widget *selected;
    float x, y;
} touch_dialog_state;
int touch_dialog_input(ui_project *ui, int enabled, ui_widget **items, int count,
                       const plat_key_event *ev, touch_dialog_state *state, int *result);
typedef struct {
    touch_dialog_state tap;
    const void *context;
    int root;
} touch_back_state;
/* Consumes only contacts which began on Back. result is 1 after confirmation. */
int touch_back_input(ui_project *ui, int enabled, const void *context, int root,
                     const plat_key_event *ev, touch_back_state *state, int *result);
#endif
