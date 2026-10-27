/* Custom Modes: a native GT Mode tile and page. The page is built on the
   authored Template root; the tile is a copy of the authored Home tile with
   the riding car's CarFace. Opt-in: nothing runs in a normal race. */
static int custom_modes_top_root = -1, custom_modes_page_root = -1;
static ui_widget *custom_modes_tile_car;
static ui_widget *custom_modes_tile_model;
static ui_widget *custom_modes_page_bg;
static char custom_modes_car[64];
static char custom_modes_hall_bgm[32];
static int custom_modes_color;
static int custom_modes_attached;
static int custom_modes_pending;

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
    menu_request_music(m, custom_modes_hall_bgm);
    fprintf(stderr, "custom modes: music requested %s -> %s\n", custom_modes_hall_bgm,
            menu_music_name(m) ? menu_music_name(m) : "");
    custom_modes_pending = 1;
    ui_fade(menu_ui(m), 0, 1, .25f);
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
    custom_modes_pending = 2;
    ui_fade(menu_ui(m), 0, 1, .25f);
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
    if (m)
        ui_fade(menu_ui(m), 0, 1, .25f);
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

static const char *custom_modes_map_car(void)
{
    const char *c = custom_modes_car;
    static const char *const jpn[] = {"lancer", "skyline", "silvia", "supra", "rx", "impreza",
                                      "civic", "integra", "nsx", "evo", "fairlady", "mr2"};
    static const char *const usa[] = {"ford", "chev", "dodge", "chrysler", "cadillac", "pontiac",
                                      "saleen", "shelby", "corvette", "mustang", "camaro"};
    static const char *const ger[] = {"bmw", "mercedes", "audi", "volkswagen", "vw", "porsche",
                                      "opel", "amg"};
    static const char *const fra[] = {"peugeot", "citroen", "renault", "bugatti"};
    static const char *const ita[] = {"ferrari", "lamborghini", "alfa", "fiat", "lancia",
                                      "pagani", "maserati"};
    static const char *const gbr[] = {"aston", "jaguar", "lotus", "mini", "bentley", "tvr", "mg"};
    for (unsigned i = 0; i < sizeof jpn / sizeof jpn[0]; i++)
        if (strstr(c, jpn[i]))
            return "car_90a.mdl";
    for (unsigned i = 0; i < sizeof usa / sizeof usa[0]; i++)
        if (strstr(c, usa[i]))
            return "car_USA2.mdl";
    for (unsigned i = 0; i < sizeof ger / sizeof ger[0]; i++)
        if (strstr(c, ger[i]))
            return "car_GER.mdl";
    for (unsigned i = 0; i < sizeof fra / sizeof fra[0]; i++)
        if (strstr(c, fra[i]))
            return "car_FRA.mdl";
    for (unsigned i = 0; i < sizeof ita / sizeof ita[0]; i++)
        if (strstr(c, ita[i]))
            return "car_ITA.mdl";
    for (unsigned i = 0; i < sizeof gbr / sizeof gbr[0]; i++)
        if (strstr(c, gbr[i]))
            return "car_GBR.mdl";
    return "car_90a.mdl";
}

static void custom_modes_tick(menu *m)
{
    if (custom_modes_page_bg)
        custom_modes_page_bg->flash_time = 5.2f;
    if (!custom_modes_pending || !m)
        return;
    ui_project *ui = menu_ui(m);
    if (ui_fade_remaining(ui) > 0 || ui_fade_opacity(ui) < .999f)
        return;
    if (custom_modes_pending == 1)
        menu_native_page(m, custom_modes_page_root);
    else
        menu_native_page(m, custom_modes_top_root);
    ui_fade(ui, 1, 0, .25f);
    custom_modes_pending = 0;
}

static void custom_modes_attach(menu *m)
{
    if (!m || custom_modes_attached || getenv("MOON_CUSTOM_MODES_OFF"))
        return;
    ui_project *ui = menu_ui(m);
    if (!ui)
        return;
    int top = -1, page = -1, home = -1, event = -1;
    for (int i = 0; i < ui_root_count(ui); i++) {
        const char *name = ui_root_name(ui, i);
        if (!strcmp(name, "TopRoot"))
            top = i;
        else if (!strcmp(name, "Template"))
            page = i;
        else if (!strcmp(name, "HomeRoot"))
            home = i;
        else if (!strcmp(name, "EventHallRoot"))
            event = i;
    }
    if (top < 0 || page < 0 || home < 0 || event < 0)
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
        custom_modes_tile_car->w = 88;
        custom_modes_tile_car->h = 72;
        custom_modes_tile_car->has_geometry = 1;
        custom_modes_tile_car->visible = 0;
        custom_modes_tile_car->car_normal = 0;
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

    custom_modes_refresh_car(m);
    if (tile_model) {
        char path[96];
        snprintf(path, sizeof path, "image/gtmode/map/%s", custom_modes_map_car());
        free(tile_model->model_path);
        tile_model->model_path = strdup(path);
        tile_model->visible = 1;
        fprintf(stderr, "custom modes: map car model=%s\n", path);
    }
    ui_widget *page_root = ui_root_widget(ui, page);
    ui_widget *template_common = ui_widget_child(page_root, "Common");
    if (template_common)
        template_common->visible = 1;
    ui_widget *event_root = ui_root_widget(ui, event);
    snprintf(custom_modes_hall_bgm, sizeof custom_modes_hall_bgm, "%s",
             mp_string(event_root ? event_root->src : NULL, "bgm") ?: "HALL");
    ui_load_package(ui, "/projects/gtmode", "EventHallRoot");
    ui_load_package(ui, "/projects/gtmode", "BeginnerHall");
    ui_widget *page_copy = event_root ? ui_widget_copy(ui, event_root) : NULL;
    if (!page_copy)
        return;
    page_copy->name = "CustomModesPage";
    page_copy->x = 0;
    page_copy->y = 0;
    page_copy->w = 640;
    page_copy->h = 480;
    page_copy->has_geometry = 1;
    page_copy->visible = 1;
    ui_widget *page_bg = cm_find(page_copy, "bg_flash");
    custom_modes_page_bg = page_bg;
    if (page_bg) {
        free(page_bg->flash_path);
        page_bg->flash_path = strdup("image/gtmode/event/hall.swf");
        page_bg->visible = 1;
        page_bg->flash_time = 5.2f;
    }
    ui_widget *page_bg_image = cm_find(page_copy, "bg_image");
    if (page_bg_image)
        page_bg_image->visible = 0;
    ui_widget_append(page_root, page_copy);
    {
        int last = page_root->nkids - 1;
        ui_widget *pc = page_root->kids[last];
        for (int k = last; k > 0; k--)
            page_root->kids[k] = page_root->kids[k - 1];
        page_root->kids[0] = pc;
    }
    if (template_common) {
        ui_widget *h = ui_widget_child(template_common, "Header");
        ui_widget *t = h ? ui_widget_child(h, "Title") : NULL;
        ui_widget *l = t ? ui_widget_child(t, "label") : NULL;
        if (l)
            cm_text(l, "Custom Modes");
        ui_widget *tt = cm_find(template_common, "tip");
        if (tt)
            cm_text(tt, "Custom Modes: Elimination - every lap, the last car is out.");
    }
    ui_widget *common = ui_widget_child(page_copy, "Common");
    ui_widget *header = common ? ui_widget_child(common, "Header") : NULL;
    ui_widget *title = header ? ui_widget_child(header, "Title") : NULL;
    ui_widget *title_label = title ? ui_widget_child(title, "label") : NULL;
    if (title_label)
        cm_text(title_label, "Custom Modes");
    ui_widget *info = cm_find(page_copy, "info");
    if (info)
        cm_text(info, "Choose a custom race mode.");
    ui_widget *tip = cm_find(page_copy, "tip");
    if (tip)
        cm_text(tip, "Custom Modes: Elimination - every lap, the last car is out.");
    ui_widget *scroll = ui_widget_child(page_copy, "ScrollBox");
    if (scroll)
        scroll->visible = 0;
    ui_widget *hidden = cm_find(page_copy, "hidden");
    ui_widget *tpl_item = hidden ? ui_widget_child(hidden, "EventItem") : NULL;
    ui_widget *tpl_ticket = tpl_item ? ui_widget_child(tpl_item, "Ticket") : NULL;
    ui_widget *tpl_stub = hidden ? ui_widget_child(hidden, "TicketStubLong") : NULL;
    if (hidden)
        hidden->visible = 0;
    if (tpl_ticket && tpl_stub) {
        ui_widget *item = ui_widget_from_text(
            ui, page,
            "Composite {name string {\"Elimination\"} geometry rectangle {0 100 640 132}}");
        if (item) {
            ui_widget *ticket = ui_widget_copy(ui, tpl_ticket);
            ui_widget *stub = ui_widget_copy(ui, tpl_stub);
            if (ticket) {
                ui_widget *ticket_img = cm_find(ticket, "image");
                if (ticket_img)
                    ticket_img->visible = 0;
                ui_widget_append(item, ticket);
            }
            ui_widget *car = ui_widget_copy(ui, car_sample);
            if (car) {
                car->name = "CarFace";
                car->x = 43;
                car->y = 0;
                car->w = 296;
                car->h = 132;
                car->has_geometry = 1;
                car->visible = 1;
                car->car_normal = 0;
                car->car_pose_set = 1;
                car->car_motion = 0;
                car->car_color_index = custom_modes_color;
                car->car_code = NULL;
                car->car_code = strdup(custom_modes_car[0] ? custom_modes_car
                                                            : (car_sample->car_code ? car_sample->car_code : ""));
                ui_widget_append(item, car);
            }
            if (stub) {
                ui_widget *label = cm_find(stub, "label");
                if (label) {
                    cm_text(label, "Elimination");
                    label->has_runtime_color = 1;
                    label->runtime_color[0] = label->runtime_color[1] = label->runtime_color[2] = 255;
                    label->runtime_color[3] = 255;
                }
                ui_widget *type = cm_find(stub, "type");
                if (type)
                    cm_text(type, "Elimination");
                const char *hide[] = {"stages", "aspec", "bspec", "license", "records"};
                for (unsigned h = 0; h < sizeof hide / sizeof hide[0]; h++) {
                    ui_widget *w = cm_find(stub, hide[h]);
                    if (w)
                        w->visible = 0;
                }
                ui_widget *stub_image = cm_find(stub, "image");
                if (stub_image) {
                    free(stub_image->image_path);
                    stub_image->image_path = strdup("image/gtmode/event/ticket_short.png");
                    stub_image->visible = 1;
                }
                ui_widget *aspec = cm_find(stub, "aspec");
                if (aspec) {
                    free(aspec->image_path);
                    aspec->image_path = strdup("image/gtmode/event/short_A.png");
                    aspec->x = 4;
                    aspec->y = 44;
                    aspec->w = 38;
                    aspec->h = 24;
                    aspec->has_geometry = 1;
                    aspec->scale_x = aspec->scale_y = 1;
                    aspec->visible = 1;
                }
                ui_widget *bspec = cm_find(stub, "bspec");
                if (bspec) {
                    free(bspec->image_path);
                    bspec->image_path = strdup("image/gtmode/event/short_B.png");
                    bspec->x = 4;
                    bspec->y = 72;
                    bspec->w = 38;
                    bspec->h = 24;
                    bspec->has_geometry = 1;
                    bspec->scale_x = bspec->scale_y = 1;
                    bspec->visible = 1;
                }
                ui_widget_append(item, stub);
            }
            item->visible = 1;
            item->has_focus_override = 1;
            item->focus_override = 1;
            ui_widget_append(page_copy, item);
            menu_native_method(m, item, "onActivate", custom_modes_elimination, m);
        }
    }
    menu_native_method(m, page_root, "onCancel", custom_modes_back, m);
    custom_modes_top_root = top;
    custom_modes_page_root = page;
    custom_modes_attached = 1;
}
