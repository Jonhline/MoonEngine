/* Original warning/normal presentation, including switching a cached dialog. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#include "../../src/app/dialog_style.h"
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char msg[256];
    ui_project *ui = ui_project_load(vol, "cursor", msg, sizeof msg);
    CHECK(ui);
    ui_widget *page = NULL;
    for (int i = 0; i < ui_root_count(ui); i++)
        if (!strcmp(ui_root_name(ui, i), "ConfirmDialog"))
            page = ui_root_widget(ui, i);
    CHECK(page);
    ui_widget *pane = ui_widget_child(page, "Dialog"), *heading = ui_widget_child(pane, "Title"),
              *body = ui_widget_child(pane, "Message");
    int types[] = {2, 0, 1, 2, 3, 4, 0};
    for (unsigned i = 0; i < sizeof types / sizeof *types; i++) {
        int warning = types[i] == 2;
        const char *title = moon_dialog_style(ui, page, types[i], NULL);
        CHECK(title && *title);
        CHECK(!strcmp(title,ui_translate(ui,page->root,NULL,warning?"Incomplete":types[i]==0?"Confirm":"Dialog")));
        if (argc > 2 && warning)
            ui_widget_child(body, "caution")->visible = 0;
        CHECK(ui_widget_child(heading,"bgE")->visible==warning);
        CHECK(ui_widget_child(heading,"titleE")->visible==warning);
        CHECK(ui_widget_child(heading,"bg")->visible==!warning);
        CHECK(ui_widget_child(heading,"title")->visible==!warning);
        CHECK(ui_widget_child(body,"caution")->visible==warning);
        CHECK(!strcmp(moon_dialog_style(ui,page,types[i],"Explicit title"),"Explicit title"));
    }
    ui_project_free(ui);
    gt4_vol_close(vol);
    puts("PASS authored warning art and translated captions for five dialog types");
    return 0;
}
