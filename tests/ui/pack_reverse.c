/* Authored dialog order and live linear-box end packing. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char error[256];
    ui_project *p = ui_project_load(vol, "cursor", error, sizeof error);
    CHECK(p);
    ui_widget *root = ui_root_widget(p, 0);
    CHECK(root);
    const char *description =
        "HBox { name string {\"PackTest\"} packing digit {1} geometry rectangle {0 0 200 40} "
        "children [3] { Composite { name string {\"A\"} packing digit {1} geometry rectangle {0 0 30 20} } "
        "Composite { name string {\"B\"} packing digit {1} pack_reverse digit {1} geometry rectangle {0 0 50 20} } "
        "Composite { name string {\"C\"} packing digit {1} geometry rectangle {0 0 20 20} } } }";
    ui_widget *box = ui_widget_from_text(p, 0, description);
    CHECK(box);
    ui_widget_append(root, box);
    ui_draw_root(p, 0, NULL, NULL);
    CHECK(ui_widget_child(box,"A")->x == 0);
    CHECK(ui_widget_child(box,"B")->x == (argc > 2 ? 30 : 150));
    CHECK(ui_widget_child(box,"C")->x == 30);
    description =
        "VBox { name string {\"VerticalTest\"} packing digit {1} geometry rectangle {0 0 100 100} "
        "children [3] { Composite { name string {\"A\"} packing digit {1} geometry rectangle {0 0 30 20} } "
        "Composite { name string {\"B\"} packing digit {1} pack_reverse digit {1} geometry rectangle {0 0 30 50} } "
        "Composite { name string {\"C\"} packing digit {1} geometry rectangle {0 0 30 30} } } }";
    ui_widget *vertical = ui_widget_from_text(p, 0, description);
    CHECK(vertical);
    ui_widget_append(root, vertical);
    ui_draw_root(p, 0, NULL, NULL);
    CHECK(ui_widget_child(vertical,"A")->y == 0);
    CHECK(ui_widget_child(vertical,"B")->y == 50);
    CHECK(ui_widget_child(vertical,"C")->y == 20);
    int checked = 0;
    for (int i = 0; i < ui_root_count(p); i++) {
        if (strcmp(ui_root_name(p, i), "ConfirmDialog"))
            continue;
        ui_widget *dialog = ui_widget_child(ui_root_widget(p, i), "Dialog");
        CHECK(dialog);
        ui_widget *buttons = ui_widget_child(dialog, "Buttons");
        CHECK(buttons && mp_number(buttons->src,"pack_reverse",0)==1);
        ui_widget *pair = ui_widget_child(buttons, "YesNo");
        CHECK(pair);
        pair->visible = 1;
        ui_widget_child(buttons, "OK")->visible = 0;
        ui_draw_root(p, i, NULL, NULL);
        CHECK(ui_widget_child(pair,"No")->x==0);
        CHECK(ui_widget_child(pair,"Yes")->x==120);
        CHECK(pair->x==40);
        checked++;
    }
    CHECK(checked==1);
    ui_project_free(p);
    gt4_vol_close(vol);
    puts("PASS horizontal/vertical end packing and authored No/Yes dialog order");
    return 0;
}
