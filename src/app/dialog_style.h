/* Authored ConfirmDialog.open, cursor.ad L169..203. */
#ifndef MOON_DIALOG_STYLE_H
#define MOON_DIALOG_STYLE_H
#include "ui.h"
#include <stdlib.h>
#include <string.h>
static const char *moon_dialog_style(ui_project *ui, ui_widget *page, int type, const char *title)
{
    ui_widget *pane = ui_widget_child(page, "Dialog");
    ui_widget *heading = ui_widget_child(pane, "Title");
    ui_widget *body = ui_widget_child(pane, "Message");
    int warning = type == 2;
    ui_widget_child(heading, "title")->visible = !warning;
    ui_widget_child(heading, "bg")->visible = !warning;
    ui_widget_child(heading, "titleE")->visible = warning;
    ui_widget_child(heading, "bgE")->visible = warning;
    ui_widget_child(body, "caution")->visible = warning;
    if (!title || !*title)
        title = ui_translate(ui, page->root, NULL,
                             warning     ? "Incomplete"
                             : type == 0 ? "Confirm"
                                         : "Dialog");
    if (!title)
        title = "";
    ui_widget *text = ui_widget_child(heading, warning ? "titleE" : "title");
    free(text->text);
    free(text->text_key);
    text->text_key = NULL;
    text->text = strdup(title);
    return text->text;
}
#endif
