/* Custom Modes: a native GT Mode tile and page. The page is built on the
   authored Template root; the tile is a copy of the authored Home tile with
   the riding car's CarFace. Opt-in: nothing runs in a normal race. */
static int custom_modes_top_root = -1, custom_modes_page_root = -1;
static ui_widget *custom_modes_tile_car;
static ui_widget *custom_modes_tile_model;
static char custom_modes_car[64];
static int custom_modes_color;
static int custom_modes_attached;

typedef struct {
    char course[32];
    char car[32];
    int color;
} custom_modes_race_request;

static custom_modes_race_request custom_modes_request;
static int custom_modes_request_pending;

static int custom_modes_take_race_request(custom_modes_race_request *out)
{
    if (!custom_modes_request_pending)
        return 0;
    custom_modes_request_pending = 0;
    if (out)
        *out = custom_modes_request;
    return 1;
}

static ui_widget *cm_find(ui_widget *w, const char *name)
{
    if (!w)
        return NULL;
    if (w->name && !strcmp(w->name, name))
        return w;
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *found = cm_find(w->kids[i], name);
        if (found)
            return found;
    }
    return NULL;
}

static void cm_text(ui_widget *w, const char *s)
{
    if (!w)
        return;
    free(w->text);
    w->text = strdup(s ? s : "");
}

static void custom_modes_refresh_car(menu *m)
{
    if (!m)
        return;
    const char *car = menu_split_car(m, 0);
    if (!car || !*car)
        car = menu_riding_car(m);
    if (car && *car) {
        snprintf(custom_modes_car, sizeof custom_modes_car, "%s", car);
        custom_modes_color = menu_riding_color(m);
        if (!custom_modes_color)
            custom_modes_color = (int)menu_game_number(m, "car_color", 0);
    }
    if (!custom_modes_tile_car || !custom_modes_car[0])
        return;
    free(custom_modes_tile_car->car_code);
    custom_modes_tile_car->car_code = strdup(custom_modes_car);
    custom_modes_tile_car->car_color_index = custom_modes_color;
}

static int custom_modes_open(ah_vm *vm, ah_obj *self, void *data, int argc,
                             const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    custom_modes_refresh_car(m);
    menu_native_page(m, custom_modes_page_root);
    menu_play_effect(m, "ok");
    *ret = ah_int(2);
    return 0;
}

static int custom_modes_back(ah_vm *vm, ah_obj *self, void *data, int argc,
                             const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    menu_native_page(m, custom_modes_top_root);
    menu_play_effect(m, "cancel");
    *ret = ah_int(2);
    return 0;
}

static int custom_modes_elimination(ah_vm *vm, ah_obj *self, void *data, int argc,
                                    const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    const char *car = m ? menu_split_car(m, 0) : NULL;
    if (!car || !*car)
        car = m ? menu_riding_car(m) : NULL;
    if ((!car || !*car) && custom_modes_car[0])
        car = custom_modes_car;
    snprintf(custom_modes_request.car, sizeof custom_modes_request.car, "%s",
             car && *car ? car : custom_modes_car);
    custom_modes_request.color = m ? menu_riding_color(m) : custom_modes_color;
    if (!custom_modes_request.color)
        custom_modes_request.color = custom_modes_color;
    snprintf(custom_modes_request.course, sizeof custom_modes_request.course, "%s",
             getenv("MOON_CUSTOM_COURSE") && *getenv("MOON_CUSTOM_COURSE")
                 ? getenv("MOON_CUSTOM_COURSE")
                 : "tsukuba");
    custom_modes_request_pending = 1;
    fprintf(stderr, "custom modes: elimination selected car=%s color=%d course=%s\n",
            custom_modes_request.car, custom_modes_request.color,
            custom_modes_request.course);
    *ret = ah_int(2);
    return 0;
}

static int custom_modes_focus(ah_vm *vm, ah_obj *self, void *data, int argc,
                              const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    custom_modes_refresh_car(data);
    if (custom_modes_tile_model) {
        custom_modes_tile_model->model_velocity[1] = 2.0f;
        custom_modes_tile_model->model_target_mode = 0;
    }
    if (custom_modes_tile_car) {
        custom_modes_tile_car->car_pose_set = 1;
        custom_modes_tile_car->car_motion = 1;
    }
    if (custom_modes_tile_car)
        fprintf(stderr, "custom modes: focus car yaw=%.1f motion=%d\n",
                custom_modes_tile_car->model_angles[1], custom_modes_tile_car->car_motion);
    *ret = ah_int(0);
    return 0;
}

static int custom_modes_blur(ah_vm *vm, ah_obj *self, void *data, int argc,
                             const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    if (custom_modes_tile_model) {
        custom_modes_tile_model->model_velocity[1] = 0;
        custom_modes_tile_model->model_target_mode = 1;
    }
    if (custom_modes_tile_car) {
        custom_modes_tile_car->car_motion = 0;
        fprintf(stderr, "custom modes: blur car yaw=%.1f motion=%d\n",
                custom_modes_tile_car->model_angles[1], custom_modes_tile_car->car_motion);
    }
    *ret = ah_int(0);
    return 0;
}

static void custom_modes_attach(menu *m)
{
    if (!m || custom_modes_attached || getenv("MOON_CUSTOM_MODES_OFF"))
        return;
    ui_project *ui = menu_ui(m);
    if (!ui)
        return;
    int top = -1, page = -1, home = -1;
    for (int i = 0; i < ui_root_count(ui); i++) {
        const char *name = ui_root_name(ui, i);
        if (!strcmp(name, "TopRoot"))
            top = i;
        else if (!strcmp(name, "Template"))
            page = i;
        else if (!strcmp(name, "HomeRoot"))
            home = i;
    }
    if (top < 0 || page < 0 || home < 0)
        return;
    ui_widget *top_root = ui_root_widget(ui, top);
    ui_widget *space = top_root ? cm_find(top_root, "space") : NULL;
    ui_widget *sample = space ? ui_widget_child(space, "Home") : NULL;
    ui_widget *home_root = ui_root_widget(ui, home);
    ui_widget *car_sample = home_root ? cm_find(home_root, "CarFace") : NULL;
    if (car_sample && car_sample->car_code && *car_sample->car_code) {
        snprintf(custom_modes_car, sizeof custom_modes_car, "%s", car_sample->car_code);
        custom_modes_color = car_sample->car_color_index;
    }
    if (!space || !sample || !car_sample)
        return;
    ui_widget *tile = ui_widget_copy(ui, sample);
    if (!tile)
        return;
    {
        ui_widget *nav = ui_widget_from_text(
            ui, top,
            "Composite {name string {\"nav\"} navigate_source rectangle {110 0 174 72} "
            "navigate_target rectangle {110 0 174 72}}");
        if (nav) {
            tile->src = nav->src;
        }
    }
    tile->name = "CustomModes";
    tile->x = 600;
    tile->y = 410;
    tile->visible = 1;
    tile->has_focus_override = 1;
    tile->focus_override = 1;
    ui_widget *home_model = ui_widget_child(sample, "bg");
    ui_widget *tile_model = ui_widget_child(tile, "bg");
    custom_modes_tile_model = tile_model;
    if (home_model && tile_model) {
        for (int i = 0; i < 3; i++)
            tile_model->model_angles[i] = home_model->model_angles[i];
        tile_model->model_velocity[1] = 0;
        tile_model->model_target_mode = 1;
        tile_model->visible = 0;
    }
    ui_widget *label = ui_widget_child(tile, "label");
    if (label) {
        cm_text(label, "Custom Modes");
        label->x = -40;
        label->y = 69;
        label->w = 168;
        label->h = 50;
        label->has_geometry = 1;
    }
    ui_widget *label2 = ui_widget_child(tile, "label2");
    if (label2)
        label2->visible = 0;
    custom_modes_tile_car = ui_widget_copy(ui, car_sample);
    if (custom_modes_tile_car) {
        custom_modes_tile_car->name = "CarFace";
        custom_modes_tile_car->x = 0;
        custom_modes_tile_car->y = 0;
        custom_modes_tile_car->w = 90;
        custom_modes_tile_car->h = 58;
        custom_modes_tile_car->has_geometry = 1;
        custom_modes_tile_car->visible = 1;
        custom_modes_tile_car->car_normal = 1;
        custom_modes_tile_car->car_pose_set = 0;
        custom_modes_tile_car->car_color_index = custom_modes_color;
        custom_modes_tile_car->car_pose_set = 1;
        custom_modes_tile_car->car_motion = 0;
        custom_modes_tile_car->car_code = NULL;
        ui_widget_append(tile, custom_modes_tile_car);
    }
    ui_widget_append(space, tile);
    fprintf(stderr, "custom modes: attached tile at %.0f,%.0f space kids=%d\n", tile->x, tile->y,
            space->nkids);
    menu_native_method(m, tile, "onActivate", custom_modes_open, m);
    menu_native_method(m, tile, "onFocusEnter", custom_modes_focus, m);
    menu_native_method(m, tile, "onFocusLeave", custom_modes_blur, m);

    ui_widget *page_root = ui_root_widget(ui, page);
    ui_widget *title = ui_widget_child(page_root, "Title");
    if (title) {
        ui_widget *label = ui_widget_child(title, "label");
        if (label)
            cm_text(label, "Custom Modes");
    }
    ui_widget *panel = ui_widget_from_text(
        ui, page,
        "ColorFace {name string {\"panel\"} geometry rectangle {40 64 560 340} color [1] "
        "{RGBA {8 9 12 220}}}");
    if (panel)
        ui_widget_append(page_root, panel);
    ui_widget *heading = ui_widget_from_text(
        ui, page,
        "TextFace {name string {\"heading\"} geometry rectangle {40 72 560 40} font string "
        "{\"hb_24\"} align digit {3} text string {\"Custom Modes\"} text_color RGBA {225 225 "
        "230 255} shadow digit {1.5} shadow_color RGBA {0 0 0 180}}");
    if (heading)
        ui_widget_append(page_root, heading);
    ui_widget *entry = ui_widget_from_text(
        ui, page,
        "Composite {name string {\"Elimination\"} can_focus digit {1} geometry rectangle {100 "
        "150 440 48} children [2] {ColorFace {name string {\"bg\"} geometry rectangle {0 0 440 "
        "48} color [1] {RGBA {36 38 44 230}}} TextFace {name string {\"label\"} geometry "
        "rectangle {14 8 412 32} font string {\"hm_16\"} text string {\"Elimination\"} "
        "text_color RGBA {230 230 230 255}}}}");
    if (entry) {
        ui_widget_append(page_root, entry);
        menu_native_method(m, entry, "onActivate", custom_modes_elimination, m);
    }
    menu_native_method(m, page_root, "onCancel", custom_modes_back, m);
    custom_modes_top_root = top;
    custom_modes_page_root = page;
    custom_modes_attached = 1;
    custom_modes_refresh_car(m);
}
