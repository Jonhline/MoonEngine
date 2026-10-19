/* Original option-menu logical-item enumeration used by arcade.ad L900..905. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char error[256];
    menu *m = menu_open(vol, "option", error, sizeof error);
    CHECK(m);
    int checked = 0;
    for (int root = 0; root < ui_root_count(m->ui); root++) {
        ui_widget *tree = ui_root_widget(m->ui, root);
        ui_widget *stack[2048];
        int count = 0;
        stack[count++] = tree;
        while (count) {
            ui_widget *w = stack[--count];
            if (!strcmp(w->type, "OptionMenu")) {
                int n = ui_option_count(w);
                ah_obj *object = widget_module(m, w);
                ah_val ret;
                CHECK(ah_call_member(m->vm,object,"Count",0,NULL,&ret)>=0);
                CHECK(ah_num(ret)==n);
                for (int i = 0; i < n; i++) {
                    ah_val index = ah_int(i);
                    CHECK(ah_call_member(m->vm,object,"getItem",1,&index,&ret)>=0);
                    ui_widget *expected = ui_option_item(w, i);
                    if (argc > 2 && n > 1)
                        expected = ui_option_item(w, (i + 1) % n);
                    CHECK(ah_as_obj(ret,AO_MODULE)==widget_module(m,expected));
                    int saved = expected->visible;
                    expected->visible = 0;
                    CHECK(ah_call_member(m->vm,object,"Count",0,NULL,&ret)>=0 && ah_num(ret)==n);
                    expected->visible = saved;
                    checked++;
                }
            }
            for (int k = 0; k < w->nkids; k++) {
                CHECK(count<2048);
                stack[count++] = w->kids[k];
            }
        }
    }
    CHECK(checked>100);
    menu_close(m);
    gt4_vol_close(vol);
    printf("PASS %d authored logical option items, identity and hidden-item counts\n", checked);
    return 0;
}
