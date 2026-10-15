#include "../../src/core/menu.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "favorites menu line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static ah_obj *field(menu *m, ah_obj *object, const char *name)
{
    ah_val value;
    ah_get_member(m->vm, object, name, &value);
    return ah_as_obj(value, AO_MODULE);
}
static ah_val invoke(menu *m, ah_obj *object, const char *name, const char *label)
{
    ah_val result, argument = ah_str(m->vm, label ? label : "");
    ah_call_member(m->vm, object, name, label != NULL, &argument, &result);
    return result;
}
static ui_widget *root(menu *m, const char *name)
{
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), name))
            return ui_root_widget(m->ui, i);
    return NULL;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 3);
    char directory[1024];
    snprintf(directory, sizeof directory, "%s-%lu", argv[2], (unsigned long)GetCurrentProcessId());
    gt4_save_init(directory);
    CHECK(gt4_game_load_disc(argv[1]));
    const char *why;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol);
    menu *arcade = menu_open(vol, "arcade", error, sizeof error);
    CHECK(arcade);
    menu_save_capture(arcade);
    CHECK(gt4_save_create(1, "Favorites"));
    menu_save_refresh(arcade);
    ah_obj *cars = field(arcade, game_object(arcade), "favorite_car");
    ah_obj *courses = field(arcade, game_object(arcade), "favorite_course");
    CHECK(cars && courses);
    load_car_tables(arcade);
    const char *car = gt4_specdb_label_for_id(arcade->car_labels, arcade->car_labels_size, car_u32(arcade->car_labels + 20));
    CHECK(car);
    char car_label[96];
    snprintf(car_label, sizeof car_label, "%s", car);
    CHECK(ah_truthy(invoke(arcade, cars, "append", car_label)));
    CHECK(!ah_truthy(invoke(arcade, cars, "append", car_label)));
    CHECK(ah_truthy(invoke(arcade, courses, "append", "tsukuba")));
    CHECK(gt4_save_dirty());
    arcade->options_dirty = 0;
    menu_save_capture(arcade);
    CHECK(gt4_save_dirty() && gt4_save_store());
    CHECK(!gt4_save_dirty());
    CHECK(!ah_truthy(invoke(arcade, cars, "append", car_label)));
    CHECK(!gt4_save_dirty());
    menu *option = menu_open(vol, "option", error, sizeof error);
    CHECK(option);
    ah_obj *saved_cars = field(option, game_object(option), "favorite_car");
    ah_obj *saved_courses = field(option, game_object(option), "favorite_course");
    CHECK(ah_truthy(invoke(option, saved_cars, "has", car_label)));
    CHECK(ah_truthy(invoke(option, saved_courses, "has", "tsukuba")));
    ah_val labels = invoke(option, saved_cars, "buildLabelList", NULL);
    CHECK(ah_array_size(labels.u.o) == 1);
    CHECK(!strcmp(ah_cstr(ah_array_get(labels.u.o, 0)), car_label));
    ui_widget *template = ui_widget_child(ui_widget_child(root(option, "CourseRoot"), "hidden"), "Item");
    CHECK(template);
    ah_obj *item = ah_module_new(option->vm, "course item", NULL, NULL, NULL);
    ah_set_member(option->vm, item, "name", ah_str(option->vm, "tsukuba"));
    ah_obj *event = ah_module_new(option->vm, "event", NULL, NULL, NULL);
    ah_set_member(option->vm, event, "widget", ah_objv(item));
    ah_val args[] = {ah_objv(option->context), ah_objv(event)}, result;
    CHECK(ah_call_member(option->vm, widget_module(option, template), "onActivate", 2, args, &result) == 1);
    CHECK(!ah_truthy(invoke(option, saved_courses, "has", "tsukuba")));
    CHECK(ah_call_member(option->vm, widget_module(option, template), "onActivate", 2, args, &result) == 1);
    CHECK(ah_truthy(invoke(option, saved_courses, "has", "tsukuba")));
    if (argc > 3)
        invoke(option, saved_courses, "clear", NULL);
    CHECK(gt4_save_dirty());
    option->options_dirty = 0;
    menu_save_capture(option);
    CHECK(gt4_save_dirty() && gt4_save_store());
    menu_close(option);
    menu_close(arcade);
    CHECK(gt4_save_load_slot(1));
    arcade = menu_open(vol, "arcade", error, sizeof error);
    CHECK(arcade);
    courses = field(arcade, game_object(arcade), "favorite_course");
    CHECK(ah_truthy(invoke(arcade, courses, "has", "tsukuba")));
    ui_widget *course_root = root(arcade, "CourseRoot");
    CHECK(course_root);
    CHECK(ah_call_member(arcade->vm, widget_module(arcade, course_root), "buildFavoriteCourse", 0, NULL, &result) == 1);
    CHECK(ah_array_size(ah_as_obj(result, AO_ARRAY)) == 1);
    menu_close(arcade);
    gt4_vol_close(vol);
    CHECK(gt4_save_delete_slot(1));
    gt4_save_shutdown();
    puts("PASS favorites: disc labels, authored course toggle/list, profile reload, cached menu transfer");
    return 0;
}
