#define plat_draw_quad4 setting_base_quad4
#define plat_draw_rotated setting_base_rotated
#define main setting_fixture_main
#include "../gtmode/setting_menu.c"
#undef main
#undef plat_draw_rotated
#undef plat_draw_quad4
#include "career_setting.h"
static float field_x, field_y, field_width;
static int field_probe, field_bad;
static float field_required;
void plat_draw_quad4(const plat_texture *t, float x, float y, float w, float h, float u, float v,
                     float a, float b, const uint8_t c[4][4])
{
    if (field_probe && fabsf(x - field_x) < .01f && fabsf(y - field_y) < .01f)
        field_width = fminf(field_width, w);
    if (field_probe && fabsf(x - field_x) <= 1.01f && fabsf(y - field_y) <= 1.01f &&
        w < field_required - 1.01f && h > 20)
        field_bad = 1;
    setting_base_quad4(t, x, y, w, h, u, v, a, b, c);
}
static float cursor_x, cursor_y;
static unsigned cursor_draws;
void plat_draw_rotated(const plat_texture *t, float x, float y, float w, float h, float cx,
                       float cy, float a, const uint8_t c[4])
{
    if (w == 32 && h == 32) {
        cursor_x = cx;
        cursor_y = cy;
        cursor_draws++;
    }
}
static void setting_all(ui_widget *w, ui_widget **out, int *count)
{
    if (*count >= 512)
        return;
    out[(*count)++] = w;
    for (int i = 0; i < w->nkids; i++)
        setting_all(w->kids[i], out, count);
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats, factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears, factory);
    gt4_game_set_setting_reader(gt4_career_vehicle_setting, factory);

    const char *fixture_dir = getenv("GT4_SETTING_CAPTURE_DIR");
    gt4_save_init(fixture_dir ? fixture_dir : "");
    char msg[256];
    menu *source = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(source);
    menu_enter(source, 0);
    ticks(source);
    menu *m = menu_open(vol, "setting", msg, sizeof msg);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    menu_set_origin(m, "gtmode");
    size_t ids_size;
    uint8_t *ids = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &ids_size);
    CHECK(ids);
    const char *civic = NULL;
    for (unsigned i = 0; i < car_u32(ids + 4); i++) {
        const char *label = gt4_specdb_label_for_id(ids, ids_size, car_u32(ids + 20 + i * 8));
        if (label && !strcmp(label, "civic_sir2_91"))
            civic = label;
    }
    CHECK(civic);
    const char *models[] = {"ka_01", civic};
    unsigned panels = 0, labels = 0;
    for (unsigned model = 0; model < 2; model++)
        for (int upgraded = 0; upgraded < 2; upgraded++) {
            ah_vm *vm = source->vm;
            ah_obj *car = ah_module_new(vm, "Car", NULL, NULL, NULL);
            CHECK(gt4_game_construct_car(vm,car,models[model],0,0));
            ah_val ret, arg = ah_objv(car);
            CHECK(nat_bind_game_car(vm,NULL,source,1,&arg,&ret)>=0);
            ah_obj *garage = get(vm, game_object(source), "garage").u.o, *cars = ah_array_new(vm);
            ah_array_push(vm, cars, arg);
            ah_set_member(vm, garage, "cars", ah_objv(cars));
            ah_set_member(vm, garage, "riding_index", ah_int(0));
            if (upgraded) {
                const int types[] = {11, 27, 2, 15};
                const int stages[] = {3, 1, 1, 3};
                for (int i = 0; i < 4; i++) {
                    ah_val args[] = {ah_int(types[i]), ah_int(stages[i])};
                    CHECK(ah_call_member(vm,car,"getParts",2,args,&ret)>=0);
                    CHECK(ah_call_member(vm,car,"changeParts",2,args,&ret)>=0);
                }
            }
            menu_close(m);
            m = menu_open(vol, "setting", msg, sizeof msg);
            CHECK(m);
            ah_set_log(m->vm, log_script, NULL);
            menu_set_origin(m, "gtmode");
            menu_save_capture(source);
            if (fixture_dir && model == 1 && upgraded) {
                CHECK(gt4_save_create(1,"Tuning fixture"));
                menu_save_capture(source);
                CHECK(gt4_save_store());
            }
            CHECK(menu_setting_begin(m));
            apply_pages(m);
            ticks(m);
            CHECK(!errors);
            ui_widget *setting = ui_root_widget(m->ui, menu_active_root(m));
            ui_widget *vbox = child(child(child(setting, "ScrollWindow"), "clip"), "space");
            ui_widget *nos = child(child(vbox, "VBox1"), "NOS");
            CHECK(nos);
            ui_widget *option = child(nos, "OptionMenu");
            CHECK(option && ui_option_count(option)==2);
            {
                int d, z;
                ui_draw_root(m->ui, menu_active_root(m), &d, &z);
            }
            field_x = option->pointer_rect[0];
            field_y = option->pointer_rect[1];
            field_width = 100000;
            field_required = option->w;
            field_bad = 0;
            field_probe = 1;
            {
                int d, z;
                ui_draw_root(m->ui, menu_active_root(m), &d, &z);
            }
            field_probe = 0;
            set_focus(m, option);
            field_probe = 1;
            {
                int d, z;
                ui_draw_root(m->ui, menu_active_root(m), &d, &z);
            }
            field_probe = 0;
            if (argc > 2 && !strcmp(argv[2], "--field-negative"))
                field_width = 155;
            CHECK(!field_bad && fabsf(field_width-option->w)<.01f);
            car = get(m->vm, m->project, "current_car").u.o;
            ah_val part[] = {ah_int(27), ah_int(1)}, exists, owned;
            CHECK(ah_call_member(m->vm,car,"isExistParts",2,part,&exists)>=0);
            CHECK(ah_call_member(m->vm,car,"isHavingParts",2,part,&owned)>=0);

            CHECK(ui_option_item(option,1)->insensitive==!(ah_truthy(exists)&&ah_truthy(owned)));
            /* Controller focus follows the live, scrolled widget rectangle. */
            const menu_input directions[] = {MENU_DOWN, MENU_RIGHT, MENU_UP, MENU_LEFT};
            for (unsigned step = 0; step < 32; step++) {
                menu_send_input(m, directions[(step / 8) % 4]);
                for (int frame = 0; frame < 120; frame++) {
                    int d, z;
                    ui_draw_root(m->ui, menu_active_root(m), &d, &z);
                    ui_animate(m->ui, menu_active_root(m), 1.f / 60);
                }
                ui_widget *focus = m->focus[menu_active_root(m)];
                CHECK(focus);
                int align = (int)mp_number(focus->src, "cursor_align", 8);
                float x = focus->pointer_rect[0], y = focus->pointer_rect[1], w = focus->w,
                      h = focus->h;
                const float px[] = {x + 2,     x + 2,     x + w / 2, x + w - 2, x + w - 2,
                                    x + w - 2, x + w / 2, x + 2,     x + w / 2};
                const float py[] = {y + h / 2, y + h - 2, y + h - 2, y + h - 2, y + h / 2,
                                    y + 2,     y + 2,     y + 2,     y + h / 2};
                CHECK(align>=0 && align<9);
                if (fabsf(cursor_x - px[align]) > .02f || fabsf(cursor_y - py[align]) > .02f)
                    fprintf(stderr, "focus %s/%s rect %g,%g,%g,%g cursor %g,%g expected %g,%g\n",
                            focus->parent->name, focus->name, x, y, w, h, cursor_x, cursor_y,
                            px[align], py[align]);
                CHECK(fabsf(cursor_x-px[align])<.02f && fabsf(cursor_y-py[align])<.02f);
            }
            if (upgraded) {
                set_focus(m, option);
                menu_send_input(m, MENU_ACCEPT);
                CHECK(option->option_open);
                for (int frame = 0; frame < 120; frame++) {
                    int d, z;
                    ui_draw_root(m->ui, menu_active_root(m), &d, &z);
                    ui_animate(m->ui, menu_active_root(m), 1.f / 60);
                }
                ui_widget *selected = ui_option_item(option, option->option_pending);
                CHECK(selected && !selected->insensitive && mp_number(option->src,"cursor_align",8)==3);
                CHECK(fabsf(cursor_x-(selected->pointer_rect[0]+option->w-2))<.02f);
                CHECK(fabsf(cursor_y-(selected->pointer_rect[1]+selected->h-2))<.02f);
                menu_send_input(m, MENU_ACCEPT);
                CHECK(!option->option_open && !errors);
            }
            car = get(m->vm, m->project, "current_car").u.o;
            const char *pages[] = {"GearRoot",       "NosRoot", "BrakeRoot",
                                   "SuspensionRoot", "LSDRoot", "DriveTrainRoot",
                                   "BallastRoot",    "AIDRoot", "AeroRoot"};
            for (unsigned p = 0; p < sizeof pages / sizeof *pages; p++) {
                /* Original row resolution is required before forcing a tuning panel. */
                uint8_t body[0x4b8], rows[0x142], valid[0x142];
                int32_t range[42];
                CHECK(gt4_game_car_body(m->vm,car,body));
                CHECK(gt4_career_blob_get(m->vm,car,"_setting_parts_ps2",rows,sizeof rows));
                CHECK(gt4_career_blob_get(m->vm,car,"_setting_parts_valid_ps2",valid,sizeof valid));
                const int groups[] = {
                    GT4_SETTING_GEAR,       GT4_SETTING_NOS,  GT4_SETTING_BRAKE,
                    GT4_SETTING_SUSPENSION, GT4_SETTING_LSD,  GT4_SETTING_DRIVE,
                    GT4_SETTING_BALLAST,    GT4_SETTING_ASCC, GT4_SETTING_DOWNFORCE};
                if (p && gt4_setting_range(body, rows, valid, groups[p], range) < 0)
                    continue;
                int root = page(m, pages[p]);
                CHECK(root>=0);
                ticks(m);
                CHECK(!errors);
                ui_widget *widgets[512];
                int count = 0;
                /* Inspect all controls, including disabled ones excluded from navigation. */
                setting_all(ui_root_widget(m->ui, root), widgets, &count);
                for (int i = 0; i < count; i++) {
                    ui_widget *w = widgets[i];
                    if (w->root != root ||
                        (strcmp(w->type, "SliderBar") && strcmp(w->type, "ScaleBar")))
                        continue;
                    ui_widget *label = child(w, "label");
                    if (!label)
                        continue;
                    CHECK(label->text && *label->text);
                    if (w->insensitive)
                        CHECK(!strcmp(label->text,"--"));
                    labels++;
                }
                if (p == 0 && upgraded) {
                    ui_widget *box =
                        child(child(child(ui_root_widget(m->ui, root), "Pane"), "HBox"), "VBox");
                    ui_widget *gear = child(box->kids[0], "slider");
                    CHECK(gear && !gear->insensitive);
                    if (argc > 2 && !strcmp(argv[2], "--missing-label")) {
                        free(child(gear, "label")->text);
                        child(gear, "label")->text = NULL;
                    }
                    CHECK(child(gear,"label")->text && strchr(child(gear,"label")->text,'.'));
                    set_focus(m, gear);
                    for (int frame = 0; frame < 120; frame++) {
                        int d, z;
                        ui_draw_root(m->ui, root, &d, &z);
                        ui_animate(m->ui, root, 1.f / 60);
                    }
                    /* Authored SliderBar cursor_align=3: bottom right, inset 2.
                   The pointer hit rectangle includes the decoration below it. */
                    CHECK(cursor_draws && mp_number(gear->src,"cursor_align",8)==3);
                    CHECK(fabsf(cursor_x-(gear->pointer_rect[0]+gear->w-2))<.02f);
                    CHECK(fabsf(cursor_y-(gear->pointer_rect[1]+gear->h-2))<.02f);
                    menu_send_input(m, MENU_ACCEPT);
                    CHECK(gear->option_open);
                    if (argc > 2 && !strcmp(argv[2], "--arrow-negative"))
                        gear->option_open = 0;
                    unsigned before_open = cursor_draws;
                    {
                        int d, z;
                        ui_draw_root(m->ui, root, &d, &z);
                    }
                    CHECK(cursor_draws==before_open);
                    int old = gear->focus_index;
                    menu_send_input(m, MENU_RIGHT);
                    CHECK(gear->focus_index==old+1 && !errors);
                    char expected[32];
                    snprintf(expected, sizeof expected, "%d.%03d", gear->focus_index / 1000,
                             gear->focus_index % 1000);
                    CHECK(!strcmp(child(gear,"label")->text,expected));
                    menu_send_input(m, MENU_ACCEPT);
                    CHECK(gear->focus_index==old+1 && !gear->option_open && !errors);
                    {
                        int d, z;
                        ui_draw_root(m->ui, root, &d, &z);
                    }
                    CHECK(cursor_draws>before_open);
                    menu_send_input(m, MENU_ACCEPT);
                    menu_send_input(m, MENU_RIGHT);
                    menu_send_input(m, MENU_BACK);
                    CHECK(!gear->option_open && gear->focus_index==old+1 && !errors);
                }
                panels++;
            }
            CHECK(menu_setting_end(m));
        }
    printf(
        "PASS stock/equipped %s and Ka: %u panels, %u slider labels, NOS ownership and gear editing\n",
        civic, panels, labels);
    free(ids);
    menu_close(m);
    menu_close(source);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    gt4_career_vehicle_close(factory);
    return 0;
}
