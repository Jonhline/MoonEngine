/* ---------------------------------------------------------------- widgets */

static const ah_host widget_host;
static int nat_album_widget(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static void album_tick(menu *m);
static void jukebox_store(menu *m);
static void album_music_bind(menu *m);
static void album_music_store(menu *m);
static int album_draw(void *, const ui_widget *, float, float, float, float, float);

static ah_obj *widget_module(menu *m, ui_widget *w);
static void set_focus(menu *m, ui_widget *w);
static int available(ui_widget *w);
static void set_tooltip(menu *m, int root, const ui_widget *w);
static void apply_pages(menu *m);
static int logger_can(void);
static void logger_tick(menu *m);
static int logger_key(menu *m, ui_widget *w, int keysym, int release);
static int logger_draw(void *user, const ui_widget *w, float x, float y, float wd, float h,
                       float opacity);
static void finalize_page(menu *m, int root);
static const char *car_name(menu *m, const char *code);
static int nat_car_name(ah_vm *, ah_obj *, void *, int, const ah_val *, ah_val *);
static int nat_car_paints(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret);
static int nat_chip_set_data(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret);
static int nat_set_color_index(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret);
static void update_progress(menu *m, ui_widget *w);
static void bind_garage_values(ah_vm *vm, ah_obj *o);
static const char *car_label_of(ah_vm *vm, ah_val v);

/* These commands still require native B-spec or record implementations
   (Replay itself plays back natively: race_game_replay).
   Visibility itself belongs to the original quick menu script. */
static int quick_pending_icon(menu *m, const ui_widget *w)
{
    const char *name = mp_string(ui_project_tree(m->ui), "name");
    return name &&
           (!strcmp(name, "QuickArcadeProject") || !strcmp(name, "QuickTimeTrialProject")) && w &&
           w->name && w->parent && w->parent->name && !strcmp(w->parent->name, "Buttons") &&
           !strcmp(w->name, "Spectator");
}

static ah_val maybe_module(menu *m, ui_widget *w)
{
    return w ? ah_objv(widget_module(m, w)) : ah_nil();
}
static int nat_prelight(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                        ah_val *ret);

/* P: the LAN and network modes are not ported (no LAN play on the PC
   yet): the Arcade's Multi-LAN Race (GameRoot LANBattle) and the
   Options' LAN Settings and Network pages (option.ad OptionPage titles
   "LAN", "Network") answer with the menu's refusal sound instead. */
static int lan_blocked(menu *m, ui_widget *w)
{
    const char *project = mp_string(ui_project_tree(m->ui), "name");
    if (!w || !project)
        return 0;
    if (!strcmp(project, "ArcadeProject"))
        return w->name && !strcmp(w->name, "LANBattle");
    if (!strcmp(project, "OptionProject")) {
        ah_val page = ah_nil(), title = ah_nil();
        if (!ah_get_member(m->vm, widget_module(m, w), "page", &page) || page.t != AV_OBJ)
            return 0;
        if (!ah_get_member(m->vm, page.u.o, "title", &title) || !ah_cstr(title))
            return 0;
        return !strcmp(ah_cstr(title), "LAN") || !strcmp(ah_cstr(title), "Network");
    }
    return 0;
}

static int sibling_index(const ui_widget *w)
{
    for (int i = 0; w->parent && i < w->parent->nkids; i++)
        if (w->parent->kids[i] == w)
            return i;
    return -1;
}

/* item = widget.doCopy(): a deep copy with the original's script members. */
static void copy_script(menu *m, ui_widget *src, ui_widget *dst)
{
    ah_obj *so = src->script, *dobj = widget_module(m, dst);
    if (so) {
        /* child widget modules of the original must not be shared */
        ah_obj *tmp = ah_module_new(m->vm, "", NULL, NULL, NULL);
        ah_copy_members(m->vm, tmp, so);
        for (int i = 0; i < src->nkids; i++) {
            ah_obj *child = src->kids[i]->script;
            if (child && src->kids[i]->name) {
                ah_val v;
                if (ah_get_member(m->vm, tmp, src->kids[i]->name, &v) && v.t == AV_OBJ &&
                    v.u.o == child)
                    ah_set_member(m->vm, tmp, src->kids[i]->name,
                                  ah_objv(widget_module(m, dst->kids[i])));
            }
        }
        /* a static still unset stays the module's, shared by its copies: set
           later (labomode.ad's Template::setListActivate, after the page
           copied Template::Normal::Scroll), the copy's onActivate sees it */
        for (int i = ah_object_count(tmp) - 1; i >= 0; i--)
            if (ah_object_value(tmp, i).t == AV_NIL)
                ah_remove_member(m->vm, tmp, ah_object_member_name(tmp, i));
        ah_copy_members(m->vm, dobj, tmp);
    }
    for (int i = 0; i < src->nkids && i < dst->nkids; i++)
        copy_script(m, src->kids[i], dst->kids[i]);
}

static int nat_do_copy(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_widget *w = ah_host_data(self);
    ui_widget *c = ui_widget_copy(m->ui, w);
    if (!c)
        return ah_error(vm, "doCopy failed");
    copy_script(m, w, c);
    *ret = ah_objv(widget_module(m, c));
    return 0;
}

/* parent.appendChild(context, item), or parent.appendChild(item). */
static int nat_append_child(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)d;
    ah_obj *item = argc ? ah_as_obj(argv[argc - 1], AO_MODULE) : NULL;
    if (item && ah_host_of(item) == &widget_host) {
        ui_widget *box = ah_host_data(self), *child = ah_host_data(item);
        /* a VBox/HBox stacks what is appended after what it holds
           (labomode.ad's folders, copied from templates at other places) */
        int vbox = !strcmp(box->type, "VBox"), hbox = !strcmp(box->type, "HBox");
        if ((vbox || hbox) && !mp_number(box->src, "packing", 0)) {
            float at = 0;
            for (int i = 0; i < box->nkids; i++) {
                /* a face without its own rectangle is the box's background
                   (the dropdown vbox's "bg"), not a row */
                const ui_widget *k = box->kids[i];
                size_t n = strlen(k->type);
                if (n >= 4 && !strcmp(k->type + n - 4, "Face") && !mp_prop(k->src, "geometry"))
                    continue;
                at = fmaxf(at, vbox ? k->y + k->h : k->x + k->w);
            }
            if (vbox) {
                child->x = 0;
                child->y = at;
            } else {
                child->x = at;
                child->y = 0;
            }
        }
        /* 002A2608: an ImageFace with no rectangle of its own draws in its
           parent's client area, the new one once appended. TuningApplyRoot
           places hidden::pict copies at pict.w * index * 2 and scrolls the
           270x127 frame by PartsPict.w * i * 2 (gtmode.ad L7344, L7452):
           the 320x150 part images fill the frame. */
        if (!strcmp(child->type, "ImageFace") && !child->has_geometry &&
            !mp_prop(child->src, "geometry")) {
            child->w = box->w;
            child->h = box->h;
            child->has_geometry = 1;
        }
        ui_widget_append(box, child);
    }
    *ret = ah_nil();
    return 0;
}

/* HistoryRoot inserts era cards before its Surface decoration. */
static int nat_insert_child(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)d;
    ui_widget *parent = ah_host_data(self);
    if (argc != 2)
        return ah_error(vm, "insertChild requires a child and an anchor");
    ah_obj *item = ah_as_obj(argv[0], AO_MODULE), *anchor = ah_as_obj(argv[1], AO_MODULE);
    if (!item || !anchor || ah_host_of(item) != &widget_host || ah_host_of(anchor) != &widget_host)
        return ah_error(vm, "insertChild requires widgets");
    ui_widget *child = ah_host_data(item), *before = ah_host_data(anchor);
    if (before->parent != parent)
        return ah_error(vm, "insertChild anchor belongs to another parent");
    if (child != before) {
        ui_widget_append(parent, child);
        if (child->parent != parent)
            return ah_error(vm, "insertChild allocation failed");
        for (int i = 0; i < parent->nkids - 1; i++)
            if (parent->kids[i] == before) {
                memmove(parent->kids + i + 1, parent->kids + i,
                        (size_t)(parent->nkids - i - 1) * sizeof *parent->kids);
                parent->kids[i] = child;
                break;
            }
    }
    *ret = ah_nil();
    return 0;
}

static int nat_remove_child(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)d;
    ah_obj *item = argc ? ah_as_obj(argv[argc - 1], AO_MODULE) : NULL;
    ui_widget *w = item && ah_host_of(item) == &widget_host ? ah_host_data(item) : NULL;
    if (w && w->parent == ah_host_data(self))
        ui_widget_detach(w);
    *ret = ah_nil();
    return 0;
}

/* MComposite countChild (002058E0 -> 00206880), and indexed getChild.
   Return the live authored children, including dynamically appended cards. */
static int nat_widget_children(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                               ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    menu *m = ah_user(vm);
    if (!data) {
        *ret = ah_int(w->nkids);
        return 0;
    }
    if (argc != 1 || !ah_is_num(argv[0]))
        return ah_error(vm, "getChild requires one index");
    double value = ah_num(argv[0]);
    if (!isfinite(value) || value < 0 || value >= w->nkids) {
        *ret = ah_nil();
        return 0;
    }
    int64_t index = (int64_t)value;
    *ret = maybe_module(m, index >= 0 && index < w->nkids ? w->kids[index] : NULL);
    return 0;
}
static int nat_widget_image(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)data;
    const char *path = argc ? ah_cstr(argv[0]) : NULL;
    if (argc != 1 || !path)
        return ah_error(vm, "setImagePath requires one path");
    ui_widget_set_image(ah_host_data(self), path);
    *ret = ah_nil();
    return 0;
}

/* Distinguish an absent optional label from a named TextFace. Scripts
   query defined("label") before indexing dynamically selected tickets. */
static int nat_widget_defined(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)data;
    ui_widget *w = ah_host_data(self);
    const char *name = argc ? ah_cstr(argv[0]) : NULL;
    ah_val value;
    if (name && !strcmp(name, "label") && !ui_widget_child(w, name) && !mp_prop(w->src, name)) {
        *ret = ah_bool(0);
        return 0;
    }
    *ret = ah_bool(name && ah_get_member(vm, self, name, &value));
    return 0;
}
/* func_001B1050 -> func_0042E6B8. US disc divisor is 1 (0042E440).
   Nonpositive amounts are displayed as "--", not as zero or negative money. */
static int nat_money_string(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    (void)data;
    if (argc != 1 || !ah_is_num(argv[0]) || !isfinite(ah_num(argv[0])))
        return ah_error(vm, "GetMoneyString requires one amount");
    char out[48];
    if (!gt4_game_money_text(ah_num(argv[0]), out, sizeof out))
        return ah_error(vm, "money display overflow");
    *ret = ah_str(vm, out);
    return 0;
}

/* UpdateContext enter/leaveEventLoop (0024EB40 -> 00251CE8,
   00251E80): each nested call owns a live frame; leave stops the innermost
   frame, and only then does its suspended script caller resume. The PC
   presenter replaces the original platform update/render callbacks. */
void menu_set_event_presenter(menu *m, int (*fn)(menu *, void *), void *user)
{
    if (m) {
        m->present_event = fn;
        m->event_user = user;
    }
}
static int nat_update_context(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    *ret = ah_objv(m->context);
    return 0;
}
/* context.getUpdateContext().getPadButtonState(port): the buttons held,
   as the platform gives them (menu_set_pad_held). labomode.ad's help
   names the bits: onListActivate's 256 and 2048 are L1 and R1 ("R1 + X
   multiple select", "L1 + R1 + X complete select") */
static int nat_pad_buttons(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    *ret = ah_int(argc && ah_num(argv[0]) != 0 ? 0 : m->pad_held);
    return 0;
}
void menu_set_pad_held(menu *m, unsigned buttons)
{
    if (m)
        m->pad_held = buttons;
}
static int nat_pad_analog(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    /* P: photo keyboard movement is read by photo_travel at the platform
       boundary; untouched analog channels are centered. */
    *ret = ah_float(0);
    return 0;
}
static int nat_event_loop(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    *ret = ah_nil();
    if (data) {
        if (m->event_depth)
            m->event_loops[m->event_depth - 1] = 0;
        return 0;
    }
    if (!m->present_event)
        return ah_error(vm, "modal event loop requires a platform presenter");
    if (m->event_depth >= 32)
        return ah_error(vm, "modal event loop nesting limit");
    int level = m->event_depth++;
    m->event_loops[level] = 1;
    apply_pages(m);
    int interrupted = 0;
    while (m->event_loops[level]) {
        unsigned budget = ah_exchange_instruction_budget(vm, 1000000);
        int alive = m->present_event(m, m->event_user);
        ah_exchange_instruction_budget(vm, budget);
        if (!alive) {
            interrupted = 1;
            break;
        }
    }
    m->event_loops[level] = 0;
    m->event_depth--;
    return interrupted ? ah_error(vm, "modal event loop interrupted by application shutdown") : 0;
}

/* labomode's EntryRoot keyboard on an InputTextFace: putString(s) at the
   caret up to max_input_length characters, backspace, left, right */
static int nat_input_edit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    const char *op = d;
    char buf[128], next[256];
    const char *t = w->text ? w->text : "";
    int len = (int)strlen(t), max = (int)mp_number(w->src, "max_input_length", 16);
    *ret = ah_nil();
    if (w->caret < 0 || w->caret > len)
        w->caret = len;
    if (!strcmp(op, "left")) {
        if (w->caret > 0)
            w->caret--;
        return 0;
    }
    if (!strcmp(op, "right")) {
        if (w->caret < len)
            w->caret++;
        return 0;
    }
    if (!strcmp(op, "backspace")) {
        if (w->caret <= 0)
            return 0;
        snprintf(next, sizeof next, "%.*s%s", w->caret - 1, t, t + w->caret);
        w->caret--;
    } else {
        const char *add = argc ? ah_to_text(argv[0], buf, sizeof buf) : "";
        int room = max - len, n = (int)strlen(add);
        if (n > room)
            n = room;
        if (n <= 0)
            return 0;
        snprintf(next, sizeof next, "%.*s%.*s%s", w->caret, t, n, add, t + w->caret);
        w->caret += n;
    }
    (void)vm;
    free(w->text);
    w->text = strdup(next);
    return 0;
}

static int nat_noop(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_nil();
    return 0;
}

static int nat_widget_is_closed(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                                ah_val *ret)
{
    (void)d;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    int closed = !m || !w || menu_active_root(m) != w->root || !m->entered[w->root];
    *ret = ah_bool(closed);
    return 0;
}

/* quick-event.ad:893 supplies the trophy model and three loading flags:
   the ModelFace draws that model (A: the flags 0, 1, 0 not read) */
static int nat_car_option_motion(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    (void)d;
    if (argc < 1 || !ah_cstr(argv[0]) || !w)
        return ah_error(vm, "loadModelOptionMotion requires widget/path");
    w->car_camera = ui_load_camera(m->ui, w->resource_root, ah_cstr(argv[0]));
    w->car_camera_time = 0;
    w->car_camera_loop = 1;
    w->car_camera_playing = 1;
    if (!w->car_camera)
        return ah_error(vm, "original option camera unavailable: %s", ah_cstr(argv[0]));
    *ret = ah_nil();
    return 0;
}
static int nat_car_crew_load(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    (void)d;
    if (argc != 1 || !w || !ah_cstr(argv[0]))
        return ah_error(vm, "loadModelSet requires model path");
    size_t size = 0;
    const uint8_t *data = ui_load_model_bytes(m->ui, w->resource_root, ah_cstr(argv[0]), &size);
    if (!data)
        return ah_error(vm, "additional model unavailable: %s", ah_cstr(argv[0]));
    /* 00138F80 -> 0013A4A8 -> 00153FC8 appends, preserving model indices. */
    static uint64_t serial;
    gt4_service_model *models =
        realloc(w->car_models, (size_t)(w->car_model_count + 1) * sizeof *models);
    if (!models)
        return ah_error(vm, "additional model allocation failed");
    w->car_models = models;
    models[w->car_model_count++] = (gt4_service_model){data, size, ++serial};
    if (w->car_model_count == 1) {
        w->car_crew_serial = serial;
        w->car_crew_data = data;
        w->car_crew_size = size;
        w->car_additional_scale = 1;
    }
    w->car_crew_loop = 1;
    *ret = ah_nil();
    return 0;
}
static ah_obj *game_object(menu *m);
static int nat_car_crew_motion(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret)
{
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    (void)d;
    if (argc < 1 || argc > 5 || !w || !ah_cstr(argv[0]) || !w->car_model_count)
        return ah_error(vm, "loadModelMotion requires a loaded model and path");
    /* 00139098..F0 defaults alignment/hidden/yaw to zero. 00154124..30
       maps a model index beyond the loaded collection to zero. */
    int index = argc > 1 ? (int)ah_num(argv[1]) : 0;
    if (index < 0)
        return ah_error(vm, "negative additional model index");
    if (index >= w->car_model_count)
        index = 0;
    const struct gt4_mot1 *motion = ui_load_motion(m->ui, w->resource_root, ah_cstr(argv[0]));
    if (!motion)
        return ah_error(vm, "additional motion unavailable: %s", ah_cstr(argv[0]));
    gt4_service_motion *tracks =
        realloc(w->car_motions, (size_t)(w->car_motion_count + 1) * sizeof *tracks);
    if (!tracks)
        return ah_error(vm, "additional motion allocation failed");
    w->car_motions = tracks;
    tracks[w->car_motion_count++] = (gt4_service_motion){motion,
                                                         0,
                                                         argc > 4 ? (float)ah_num(argv[4]) : 0,
                                                         index,
                                                         argc > 2 ? (int)ah_num(argv[2]) : 0,
                                                         argc > 3 ? ah_num(argv[3]) != 0 : 0,
                                                         0,
                                                         1};
    ah_obj *garage;
    ah_val v;
    if (ah_get_member(vm, game_object(m), "garage", &v) && (garage = ah_as_obj(v, AO_MODULE)) &&
        ah_get_member(vm, garage, "riding_car", &v))
        w->car_service_geometry_valid =
            gt4_game_car_service_geometry(vm, ah_as_obj(v, AO_MODULE), w->car_service_geometry);
    if (w->car_motion_count == 1) {
        w->car_crew_motion = motion;
        w->car_crew_time = 0;
        w->car_crew_playing = 0;
    }
    *ret = ah_nil();
    return 0;
}
static int nat_car_crew_play(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    (void)vm;
    /* 001525F0 -> 00153E20 marks additional motion controllers playing;
       00152650 -> 00153F28 sets loop, defaulting to true without an argument. */
    if (d == (void *)2) {
        *ret = ah_bool(w->car_camera && w->car_camera_playing);
        return 0;
    } /* 00153FA8 tests only option motion+10. */
    if (d) {
        w->car_crew_loop = argc ? ah_num(argv[0]) != 0 : 1;
        w->car_camera_loop = w->car_crew_loop;
    } else {
        w->car_crew_playing = 1;
        w->car_camera_playing = 1;
    }
    for (int i = 0; i < w->car_motion_count; i++) {
        if (d)
            w->car_motions[i].loop = w->car_crew_loop;
        else
            w->car_motions[i].playing = 1;
    }
    *ret = ah_nil();
    return 0;
}
static int nat_model_set_path(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)d;
    if (argc != 4 || !ah_cstr(argv[0]))
        return ah_error(vm, "setModelSetPath requires path and three flags");
    ah_set_member(vm, self, "requested_model_path", argv[0]);
    ui_widget *w = ah_host_data(self);
    if (w && !strcmp(w->type, "ModelFace")) {
        free(w->model_path);
        w->model_path = strdup(ah_cstr(argv[0]));
    }
    ah_obj *flags = ah_array_new(vm);
    for (int i = 1; i < 4; i++)
        ah_array_push(vm, flags, argv[i]);
    ah_set_member(vm, self, "requested_model_flags", ah_objv(flags));
    *ret = ah_nil();
    return 0;
}
static int nat_set_focus(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)vm;
    menu *m = d;
    ui_widget *root = ah_host_data(self);
    ah_obj *o = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    ui_widget *w = o && ah_host_of(o) == &widget_host ? ah_host_data(o) : NULL;
    if (w && w->root != root->root)
        return ah_error(vm, "setFocus: widget belongs to another page");
    if (root->root == menu_active_root(m))
        set_focus(m, w);
    else
        m->focus[root->root] = w;
    *ret = ah_nil();
    return 0;
}

/* The scripts transfer focus between the course and direction SelectBoxes. */
static int nat_widget_focus(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    if (!d) {
        ui_widget *pick = w;
        if (!strcmp(w->type, "SelectBox") && w->nkids) {
            int index = w->focus_index;
            if (index < 0 || index >= w->nkids)
                index = 0;
            pick = w->kids[index];
        }
        set_focus(m, pick);
    } else {
        for (ui_widget *p = m->focus[w->root]; p; p = p->parent)
            if (p == w) {
                set_focus(m, NULL);
                break;
            }
    }
    *ret = ah_nil();
    return 0;
}
static int nat_get_focus(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_widget *w = ah_host_data(self);
    /* a SelectBar's focused item is its child at its index (memcard.ad's
       FileList), whatever holds the page's focus */
    if (!strcmp(w->type, "SelectBox") && w->name && !strcmp(w->name, "FileList")) {
        int i = w->focus_index;
        *ret = w->nkids ? maybe_module(m, w->kids[i < 0           ? 0
                                                  : i >= w->nkids ? w->nkids - 1
                                                                  : i])
                        : ah_nil();
        return 0;
    }
    *ret = maybe_module(m, m->focus[w->root]);
    return 0;
}

/* clearChildren takes every child; clearWindow (widget vtable +0x310,
   00205330) empties the window's items and keeps its own faces:
   option.ad append_optionmenu clears each dropdown's vbox, whose "bg"
   ColorFace is the grey behind the popup's rows (the PCSX2 print). A: the
   class's own lists not read; told apart by the children's type. */
static int clear_children(menu *m, ui_widget *w, int keep_faces)
{
    ui_widget *f = m->focus[w->root];
    for (ui_widget *p = f; p; p = p->parent)
        if (p == w) {
            m->focus[w->root] = NULL;
            break;
        }
    for (int i = w->nkids - 1; i >= 0; i--) {
        size_t n = strlen(w->kids[i]->type);
        if (keep_faces && n >= 4 && !strcmp(w->kids[i]->type + n - 4, "Face"))
            continue;
        ui_widget_detach(w->kids[i]);
    }
    return 0;
}
static int nat_clear_children(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    *ret = ah_nil();
    return clear_children(d, ah_host_data(self), 0);
}
static int nat_clear_window(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    *ret = ah_nil();
    return clear_children(d, ah_host_data(self), 1);
}

static int nat_adjust_scale(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_text_adjust_scale(m->ui, ah_host_data(self));
    *ret = ah_nil();
    return 0;
}

static int nat_abbreviate(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    *ret = ah_bool(ui_text_abbreviate(m->ui, ah_host_data(self)));
    return 0;
}

/* context.translate(root, key), or context.translate(root, page, key). */
static int nat_translate(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    menu *m = d;
    ah_obj *obj = argc >= 2 ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    const char *text = "";
    if (obj && ah_host_of(obj) == &widget_host && (argc == 2 || argc == 3)) {
        ui_widget *w = ah_host_data(obj);
        char page[128], key[128];
        const char *label = ah_to_text(argv[argc - 1], key, sizeof key);
        text = ui_translate(m->ui, w->root,
                            argc == 3 ? ah_to_text(argv[1], page, sizeof page) : NULL, label);
        /* Scripts also pass already-translated strings (OptionRoot.onCancel). */
        if (!text)
            text = label;
        *ret = ah_str(vm, text);
        return 0;
    }
    if (obj && obj == m->project &&
        argc == 3) { /* the project and a page: labomode's CourseNameCaption */
        char page[128], key[128];
        const char *label = ah_to_text(argv[2], key, sizeof key);
        text = ui_translate(m->ui, menu_active_root(m) < 0 ? 0 : menu_active_root(m),
                            ah_to_text(argv[1], page, sizeof page), label);
        *ret = ah_str(vm, text ? text : label);
        return 0;
    }
    *ret = ah_str(vm, text);
    return 0;
}

static void assign_index(ui_widget *w, int index)
{
    if (!strcmp(w->type, "OptionMenu")) {
        int count = ui_option_count(w);
        if (index >= count)
            index = count - 1;
        if (index < 0)
            index = 0;
    }
    if (!strcmp(w->type, "SliderBar") || !strcmp(w->type, "ScaleBar") && w->slider_range_set) {
        double range[2] = {0, 100};
        mp_numbers(w->src, "range", range, 2);
        if (w->slider_range_set) {
            range[0] = w->slider_min;
            range[1] = w->slider_max;
        }
        if (index < range[0])
            index = (int)range[0];
        if (index > range[1])
            index = (int)range[1];
    }
    w->focus_index = index;
}

/* 002C7A80 converts each mask integer to !=0; 002C9B68 applies at
   most min(mask length, logical item count), leaving the tail intact.
   Disabling the current choice calls 002C9898 -> 002C9740(start=0):
   first non-disabled logical child; no eligible child preserves index. */
static int nat_option_insensitive(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                                  ah_val *ret)
{
    (void)vm;
    (void)d;
    ui_widget *w = ah_host_data(self);
    *ret = ah_nil();
    if (argc < 2 || !w)
        return 0;
    ah_obj *mask = ah_as_obj(argv[1], AO_ARRAY);
    if (!mask)
        return ah_error(vm, "setInsensitive requires an array");
    int count = ui_option_count(w), n = ah_array_size(mask), reset = 0;
    if (n > count)
        n = count;
    for (int i = 0; i < n; i++) {
        ui_widget *item = ui_option_item(w, i);
        ah_val value = ah_array_get(mask, i);
        int disabled = (value.t == AV_INT || value.t == AV_LONG || value.t == AV_BOOL)
                           ? (uint32_t)value.u.i != 0
                           : (int32_t)ah_num(value) != 0;
        item->insensitive = disabled;
        if (i == w->focus_index && disabled)
            reset = 1;
    }
    if (reset)
        for (int i = 0; i < count; i++)
            if (!ui_option_item(w, i)->insensitive) {
                w->focus_index = i;
                break;
            }
    return 0;
}
static int nat_set_index(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)vm;
    (void)d;
    ui_widget *w = ah_host_data(self);
    if (argc >= 2) {
        int index = (int)ah_num(argv[1]);
        assign_index(w, index);
    }
    *ret = ah_nil();
    return 0;
}

static int nat_items(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    int op = (int)(intptr_t)d;
    *ret = ah_nil();
    if (op == 0) {
        ah_obj *o = argc > 1 ? ah_as_obj(argv[1], AO_MODULE) : NULL;
        if (!o || ah_host_of(o) != &widget_host)
            return ah_error(vm, "invalid item template");
        w->item_template = ah_host_data(o);
    } else if (op == 1) {
        int count = argc ? (int)ah_num(argv[0]) : 0;
        if (!w->item_template || count < 0 || count > 2048)
            return ah_error(vm, "invalid item count");
        /* the focus let go only if it was on an item going away (not the list) */
        for (ui_widget *f = m->focus[w->root]; f; f = f->parent)
            if (f->parent == w) {
                m->focus[w->root] = NULL;
                break;
            }
        /* Original: func_002B4A00 +0x2D0 -> func_002B3AF8 +0x38..0x84:
           clamp the index to the new count, including -1 for an empty list.
           Refilled lists then start at zero instead of retaining another maker's index. */
        if (w->focus_index < 0)
            w->focus_index = count ? 0 : -1;
        else if (w->focus_index >= count)
            w->focus_index = count - 1;
        while (w->nkids)
            ui_widget_detach(w->kids[w->nkids - 1]);
        for (int i = 0; i < count; i++) {
            ui_widget *c = ui_widget_copy(m->ui, w->item_template);
            if (!c)
                return ah_error(vm, "cannot copy item");
            copy_script(m, w->item_template, c);
            ui_widget_append(w, c);
        }
        /* its visible_callback runs on the next tick, for every item */
        int known = 0;
        for (int i = 0; i < m->nitem_lists; i++)
            known |= m->item_lists[i] == w;
        if (!known && m->nitem_lists < 8)
            m->item_lists[m->nitem_lists++] = w;
    } else if (op == 3) {
        int i = argc ? (int)ah_num(argv[0]) : -1;
        *ret = ah_bool(i >= 0 && i < w->nkids); /* A: every item counts as shown */
    } else if (op == 4) {
        int i = argc ? (int)ah_num(argv[0]) : -1, flags = argc > 1 ? (int)ah_num(argv[1]) : 0,
            k = 0;
        while (k < m->nitem_updates &&
               !(m->item_updates[k].list == w && m->item_updates[k].index == i))
            k++;
        if (flags && k == m->nitem_updates && k < 64) {
            m->item_updates[k].list = w;
            m->item_updates[k].index = i;
            m->nitem_updates++;
        } else if (!flags && k < m->nitem_updates)
            m->item_updates[k] = m->item_updates[--m->nitem_updates];
    } else if (op == 6)
        *ret = ah_int(w->nkids);
    else if (op == 5 || op == 7) {
    } /* setItemWidget (a widget let go), postRedraw */
    else {
        int i = argc ? (int)ah_num(argv[0]) : -1;
        if (i >= 0 && i < w->nkids)
            *ret = maybe_module(m, w->kids[i]);
    }
    return 0;
}
/* list.setItemActive(index, on): an item's active state (its faces' visible_compare) */
static int nat_item_active(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    int i = argc ? (int)ah_num(argv[0]) : -1;
    (void)vm;
    (void)d;
    if (i >= 0 && i < w->nkids)
        w->kids[i]->active = argc > 1 ? ah_truthy(argv[1]) : 1;
    *ret = ah_nil();
    return 0;
}
/* widget.interpolateY(target, ratio) (002579A0 -> 0025C0A0/0025BFE0):
   within one unit of the target it lands there and answers 1, otherwise
   it moves by the distance times the ratio and answers 0 */
static int nat_interpolate_y(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    (void)d;
    if (argc < 2)
        return ah_error(vm, "interpolateY requires target and ratio");
    float target = (float)ah_num(argv[0]), ratio = (float)ah_num(argv[1]), dist = target - w->y;
    if (-1.f < dist && dist < 1.f) {
        w->y = target;
        *ret = ah_int(1);
        return 0;
    }
    w->y = w->y + dist * ratio;
    *ret = ah_int(0);
    return 0;
}
/* parent.movetoLast(child): the child made the last one, drawn over its
   siblings (gtmode.ad TuningApplyRoot L7444: Spec.movetoLast(Spec.label),
   the label over the chip appended at L7442) */
static int nat_move_to_last(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)d;
    ah_obj *child = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    if (argc && (!child || ah_host_of(child) != &widget_host))
        return ah_error(vm, "movetoLast requires a child widget");
    ui_widget *w = child ? ah_host_data(child) : ah_host_data(self), *pa = child
                                                                               ? ah_host_data(self)
                                                                           : w ? w->parent
                                                                               : NULL;
    if (pa)
        for (int i = 0; i < pa->nkids; i++)
            if (pa->kids[i] == w) {
                memmove(&pa->kids[i], &pa->kids[i + 1],
                        (size_t)(pa->nkids - i - 1) * sizeof *pa->kids);
                pa->kids[pa->nkids - 1] = w;
                break;
            }
    *ret = ah_nil();
    return 0;
}
static int nat_adjust_size(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_adjust_size(m->ui, ah_host_data(self));
    *ret = ah_nil();
    return 0;
}
static int nat_load_gpb(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = d;
    if (argc != 2 || !ui_load_package(m->ui, ah_cstr(argv[0]), ah_cstr(argv[1])))
        return ah_error(vm, "missing menu image package");
    *ret = ah_nil();
    return 0;
}
static int nat_exist_gpb(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    menu *m = data;
    const char *path = argc ? ah_cstr(argv[0]) : NULL;
    if (argc != 1 || !path)
        return ah_error(vm, "existGpbBinary requires one path");
    *ret = ah_bool(ui_package_contains(m->ui, menu_active_root(m), path));
    return 0;
}

static void scroll_to(ui_widget *w, int snap);

/* scroll_window.warp(item): show item at once, without easing. */
static int nat_scroll_warp(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)d;
    ah_obj *item = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    if (item && ah_host_of(item) == &widget_host)
        scroll_to(ah_host_data(item), 1);
    *ret = ah_nil();
    return 0;
}

/* scroll_window.updateGeometry(): the scrolled "space" takes the size of its
   content (MakerRoot then adds a 32-unit border). Content is the union of
   the visible children's rectangles. */
static int nat_update_geometry(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_widget *sw = ah_host_data(self), *clip = ui_widget_child(sw, "clip"),
              *space = ui_widget_child(clip, "space");
    *ret = ah_nil();
    if (!space)
        return 0;
    if (!strcmp(ah_module_name(m->project), "LicenseProject")) {
        ui_widget *info = ui_widget_child(space, "Info"), *label = ui_widget_child(info, "label");
        double parent_rect[4], label_rect[4];
        if (info && label && mp_numbers(info->src, "geometry", parent_rect, 4) == 4 &&
            mp_numbers(label->src, "geometry", label_rect, 4) == 4 && parent_rect[3] > 0) {
            /* 0025B578 +0xc0..0xd0: relative height follows the parent.
               PCSX2 B-1 menu: TextFace 01e34740 has geometry mode 1,
               local h=1 and parent Info/space h=240. quick-license.ad
               L255..264 enlarges Info before reading label.h; using the
               preferred text height here made range and scrollbar disagree.
               A: paragraph wrapping/measurement still lacks a PS2 oracle. */
            label->h = (float)(label_rect[3] / parent_rect[3]) * info->h;
        }
    }
    float r = 0, b = 0;
    for (int i = 0; i < space->nkids; i++) {
        const ui_widget *c = space->kids[i];
        if (!c->visible)
            continue;
        r = fmaxf(r, c->x + c->w);
        b = fmaxf(b, c->y + c->h);
    }
    if (r > 0)
        space->w = r;
    if (b > 0)
        space->h = b;
    return 0;
}

/* ToolTipFace.remove(): empty help line. */
static int nat_tooltip_clear(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)vm;
    (void)d;
    (void)argc;
    (void)argv;
    ui_set_tooltips(ah_host_data(self), NULL);
    *ret = ah_nil();
    return 0;
}

/* 00233468 -> 00235CF8 refreshes the current focus; 00249B70 starts
   the tooltip controller again, even when its text has not changed. */
static int nat_tooltip_reset(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu *m = d;
    ui_widget *root = ah_host_data(self);
    if (root && root->root >= 0 && root->root < ui_root_count(m->ui)) {
        set_tooltip(m, root->root, m->focus[root->root]);
        ui_reset_tooltips(root);
    }
    *ret = ah_nil();
    return 0;
}

static int nat_text_size(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = d;
    float size[2];
    ui_widget *w = ah_host_data(self);
    /* quick-license.ad L253..256 measures the multiline information
       after assigning it. Its width and multiline flag are authored in
       quick-license.mproject. Reuse the existing word-wrap adapter (A:
       original text layout/equalization has no differential proof yet). */
    if (!strcmp(ah_module_name(m->project), "LicenseProject") && mp_number(w->src, "multiline", 0))
        ui_text_wrap(m->ui, w, w->w);
    ui_text_size(m->ui, w, size);
    /* 00245950 +0x38..0x4c publishes the preferred size (+0x44), not
       allocated geometry (+0x34). updateGeometry resolves label.h from Info. */
    ah_obj *a = ah_array_new(vm);
    ah_array_push(vm, a, ah_float(size[0]));
    ah_array_push(vm, a, ah_float(size[1]));
    *ret = ah_objv(a);
    return 0;
}

static int nat_flash(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    ui_widget *w = ah_host_data(self);
    if (d) {
        const char *path = argc ? ah_cstr(argv[0]) : NULL;
        if (!path)
            return ah_error(vm, "setFlashPath requires a path");
        free(w->flash_path);
        w->flash_path = strdup(path);
    }
    w->flash_time = 0;
    *ret = ah_nil();
    return 0;
}

static ah_val color_value(ah_vm *vm, const double rgba[4]);
static int widget_color(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret);
/* CarFace's original syncLoading wrapper is func_00138F50 -> func_0013A470,
   which waits through the model loader vtable. The PC loader owns its thread. */
void menu_set_car_loader(menu *m, int (*fn)(menu *, const ui_widget *, void *), void *user)
{
    if (m) {
        m->load_car = fn;
        m->car_loader_user = user;
    }
}
static int nat_car_sync(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)data;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    if (w && w->car_code && (!m->load_car || !m->load_car(m, w, m->car_loader_user)))
        return ah_error(vm, "CarFace model loading failed: %s", w->car_code);
    *ret = ah_nil();
    return 0;
}
static int nat_car_warp(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)vm;
    (void)data;
    (void)argc;
    (void)argv;
    ui_widget *w = ah_host_data(self);
    /* 00153B08 +0x00..0x54 copies targets into the displayed pose. */
    memcpy(w->car_position, w->car_target_position, sizeof w->car_position);
    memcpy(w->model_angles, w->car_target_angles, sizeof w->model_angles);
    w->car_pose_set = 1;
    w->car_spin = 0;
    w->car_spin_ticks = 0;
    *ret = ah_nil();
    return 0;
}
static int car_model_set(ah_vm *vm, ah_obj *self, const char *name, ah_val value)
{
    (void)vm;
    ui_widget *w = ah_host_data(self);
    const char *angle[] = {"rx", "ry", "rz"}, *position[] = {"tx", "ty", "tz"};
    if (!strcmp(name, "motion")) {
        w->car_motion = (int)ah_num(value);
        w->car_spin_ticks = 0;
        return 0;
    }
    if (!strcmp(name, "posInterpolateSpeed")) {
        float speed = (float)ah_num(value);
        if (isfinite(speed)) {
            w->car_position_ratio = speed;
            w->car_position_easing = 1;
        }
        return 0;
    }
    /* 00151A38 default; 00153994 property -> model+8C, 00156E68 scale. */
    if (!strcmp(name, "additionalModelScale")) {
        w->car_additional_scale = (float)ah_num(value);
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        if (!strcmp(name, angle[i])) {
            w->car_target_angles[i] = (float)ah_num(value);
            if (w->car_motion != 2)
                w->model_angles[i] = w->car_target_angles[i];
            w->car_pose_set = 1;
            return 0;
        }
        if (!strcmp(name, position[i])) {
            w->car_target_position[i] = (float)ah_num(value);
            if (!w->car_motion)
                w->car_position[i] = w->car_target_position[i];
            w->car_pose_set = 1;
            return 0;
        }
    }
    return 0;
}
static const ah_host car_model_host = {"car_model", NULL, car_model_set, NULL, NULL};
static ah_val car_model_value(ah_vm *vm, ah_obj *self, ui_widget *w)
{
    /* HomeRoot L3950 configures CarModel even with an empty garage. */
    ah_val value;
    if (ah_get_member(vm, self, "_pc_car_model", &value))
        return value;
    ah_obj *model = ah_module_new(vm, "CarModel", NULL, &car_model_host, w);
    const char *fields[] = {"rx", "ry", "rz", "tx", "ty", "tz", "motion"};
    for (unsigned i = 0; i < sizeof fields / sizeof fields[0]; i++)
        ah_set_member(vm, model, fields[i], ah_float(0));
    w->car_pose_set = 0;
    ah_set_member(vm, model, "warp", ah_bind(vm, model, ah_native(vm, "warp", nat_car_warp, NULL)));
    ah_set_member(vm, model, "playModelMotion",
                  ah_bind(vm, model, ah_native(vm, "playModelMotion", nat_car_crew_play, NULL)));
    ah_set_member(
        vm, model, "setModelMotionLoop",
        ah_bind(vm, model, ah_native(vm, "setModelMotionLoop", nat_car_crew_play, (void *)1)));
    ah_set_member(
        vm, model, "isPlayingModelMotion",
        ah_bind(vm, model, ah_native(vm, "isPlayingModelMotion", nat_car_crew_play, (void *)2)));
    value = ah_objv(model);
    ah_set_member(vm, self, "_pc_car_model", value);
    return value;
}
static int nat_photo(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    const char *op = data;
    *ret = ah_nil();
    if (!strcmp(op, "getAperture")) {
        if (!m->photo_apertures_ready)
            return ah_error(vm, "Photo Travel: original aperture table unavailable");
        int i = argc ? to_int(argv[argc - 1]) : 1;
        i = i < 1 ? 1 : i > 16 ? 16 : i;
        *ret = ah_float(m->photo_apertures[i]);
    } else if (!strcmp(op, "setBaseLen")) {
        /* 001960F8 -> 00194C90 / 00370240: native map length. */
        if (argc && ah_is_num(argv[argc - 1]) && isfinite(ah_num(argv[argc - 1])) &&
            ah_num(argv[argc - 1]) > 0)
            m->photo.location.map_length = (float)ah_num(argv[argc - 1]);
    } else if (!strcmp(op, "loadShutterSound")) {
        const char *kind = argc ? ah_cstr(argv[argc - 1]) : NULL;
        if (m->sound && !gt4_sound_shutter(m->sound, kind))
            return ah_error(vm, "Photo Travel: shutter bank unavailable");
    } else if (!strcmp(op, "isRendering"))
        *ret = ah_bool(!m->photo.image_width);
    else if (!strcmp(op, "doFocus")) {
        /* 001952D0: centre-depth focus, consumed by the scene renderer. */
        m->photo.focus_requested = 1;
        *ret = ah_int(0);
    }
    return 0;
}

static int widget_get(ah_vm *vm, ah_obj *self, const char *s, ah_val *out)
{
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(self);
    if (!w)
        return 0;

    if (!strcmp(w->type, "ScaleBar")) {
        float *field = !strcmp(s, "value")           ? &w->photo_value
                       : !strcmp(s, "min")           ? &w->photo_min
                       : !strcmp(s, "max")           ? &w->photo_max
                       : !strcmp(s, "step")          ? &w->photo_step
                       : !strcmp(s, "current_value") ? &w->photo_current
                       : !strcmp(s, "repeat_start")  ? &w->photo_repeat
                                                     : NULL;
        if (field) {
            *out = ah_float(*field);
            return 1;
        }
        if (!strcmp(s, "popup")) { /* MScaleBar.popup: open (+0x100) */
            *out = ah_int(w->option_open);
            return 1;
        }
    }
    if (!strcmp(w->type, "SliderBar") && !strcmp(s, "is_popuped")) { /* MSliderBar +0x10C */
        *out = ah_int(w->option_open);
        return 1;
    }
    /* Photo filters use SliderBar.repeat_start. The script assigns it;
       its module value must remain numeric, rather than a TextFace string. */
    if (!strcmp(w->type, "SliderBar") && !strcmp(s, "value")) {
        *out = ah_int(w->focus_index);
        return 1;
    }
    if (!strcmp(w->type, "PhotoMapWindow")) {
        gt4_photo_mode *p = &m->photo;
        float *field = !strcmp(s, "wheel_angle")  ? &p->wheel_angle
                       : !strcmp(s, "lightness")  ? &p->lightness
                       : !strcmp(s, "brightness") ? &p->brightness
                       : !strcmp(s, "contrast")   ? &p->contrast
                       : !strcmp(s, "view_angle") ? &p->view_angle
                       : !strcmp(s, "aperture")   ? &p->aperture
                                                  : NULL;
        if (field) {
            *out = ah_float(*field);
            return 1;
        }
        if (!strcmp(s, "operate_mode")) {
            *out = ah_str(vm, p->operate_mode);
            return 1;
        }
        if (!strcmp(s, "color_filter")) {
            *out = ah_str(vm, p->color_filter);
            return 1;
        }
        if (!strcmp(s, "whiteBalance")) {
            *out = ah_int(p->white_balance);
            return 1;
        }
        if (!strcmp(s, "stand")) {
            *out = ah_int(p->stand);
            return 1;
        }
        if (!strcmp(s, "can_shoot")) {
            *out = ah_int(p->ready);
            return 1;
        }
        if (!strcmp(s, "getAperture") || !strcmp(s, "setBaseLen") || !strcmp(s, "doFocus")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_photo, (void *)ah_sym(vm, s)));
            return 1;
        }
    }
    if (!strcmp(w->type, "SlideShowFace") ||
        ((!strcmp(m->project_label, "labomode") || !strcmp(m->project_label, "photo_save")) &&
         !strcmp(w->type, "PhotoRenderFace"))) {
        if (!strcmp(s, "doPlay") || !strcmp(s, "doStop") || !strcmp(s, "setPhotoFile") ||
            !strcmp(s, "allocateImageBuffer") || !strcmp(s, "outUSBStorage")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_album_widget, (void *)ah_sym(vm, s)));
            return 1;
        }
        if (!strcmp(s, "shuffle")) {
            *out = ah_int(m->album_shuffle);
            return 1;
        }
    }
    if (!strcmp(w->type, "PhotoRenderFace")) {
        gt4_photo_mode *p = &m->photo;
        if (!strcmp(s, "imageWidth")) {
            *out = ah_int(p->image_width);
            return 1;
        }
        if (!strcmp(s, "imageHeight")) {
            *out = ah_int(p->image_height);
            return 1;
        }
        if (!strcmp(s, "imageScale")) {
            *out = ah_float(p->image_scale);
            return 1;
        }
        if (!strcmp(s, "ofs_x")) {
            *out = ah_float(p->image_x);
            return 1;
        }
        if (!strcmp(s, "ofs_y")) {
            *out = ah_float(p->image_y);
            return 1;
        }
        if (!strcmp(s, "portrait")) {
            *out = ah_int(p->stand != 0);
            return 1;
        }
        if (!strcmp(s, "isRendering")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_photo, (void *)ah_sym(vm, s)));
            return 1;
        }
    }
    if (!strcmp(s, "color") || !strcmp(s, "shadow_color")) {
        int shadow = !strcmp(s, "shadow_color");
        double rgba[4] = {255, 255, 255, 255};
        mp_numbers(w->src,
                   shadow                         ? "shadow_color"
                   : !strcmp(w->type, "TextFace") ? "text_color"
                                                  : "color",
                   rgba, 4);
        const uint8_t *runtime = shadow ? w->runtime_shadow : w->runtime_color;
        for (int i = 0; i < 4; i++)
            rgba[i] =
                ((shadow ? w->has_runtime_shadow : w->has_runtime_color) ? runtime[i] : rgba[i]) /
                255.;
        *out = color_value(vm, rgba);
        return 1;
    }
    if (!strcmp(s, "defined")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_widget_defined, NULL));
        return 1;
    }
    if (!strcmp(s, "setColor")) {
        *out = ah_bind(vm, self, ah_native(vm, s, widget_color, NULL));
        return 1;
    }
    /* GT Mode has a TextFace named "name". Authored children take priority
       over generic widget properties, as they do in qualified script paths. */
    ui_widget *named_child = ui_widget_child(w, s);
    if (named_child) {
        *out = ah_objv(widget_module(m, named_child));
        return 1;
    }
    if ((!strcmp(w->type, "SliderBar") || !strcmp(w->type, "ScaleBar")) && !strcmp(s, "range")) {
        /* arcade.ad SettingPopup.setupSlider reads the authored pair;
           assignments use 002DFA10's same endpoints as min/max below. */
        double range[2] = {0, 100};
        mp_numbers(w->src, "range", range, 2);
        if (w->slider_range_set) {
            range[0] = w->slider_min;
            range[1] = w->slider_max;
        }
        ah_obj *pair = ah_array_new(vm);
        for (int i = 0; i < 2; i++)
            ah_array_push(vm, pair, ah_int((int)range[i]));
        *out = ah_objv(pair);
        return 1;
    }
    if ((!strcmp(w->type, "SliderBar") || !strcmp(w->type, "ScaleBar")) &&
        (!strcmp(s, "min") || !strcmp(s, "max") || !strcmp(s, "display_min") ||
         !strcmp(s, "display_max"))) {
        double range[2] = {0, 100};
        mp_numbers(w->src, "range", range, 2);
        if (w->slider_range_set) {
            range[0] = w->slider_min;
            range[1] = w->slider_max;
        }
        int maximum = strstr(s, "max") != NULL;
        /* 002DE148/248 read E0/E4; 002DFA10 +1C..20 copies
           assigned range into those display endpoints. */
        if (strstr(s, "display_") && (w->slider_display_set & (1 << maximum)))
            *out = ah_float(maximum ? w->slider_display_max : w->slider_display_min);
        else
            *out = ah_int((int)range[maximum]);
        return 1;
    }
    if (!strcmp(s, "name")) {
        *out = ah_str(vm, w->name ? w->name : "");
        return 1;
    }
    if (!strcmp(s, "label")) {
        const char *label = mp_string(w->src, "label");
        *out = label ? ah_str(vm, label) : ah_nil();
        return 1;
    }
    if (!strcmp(w->type, "InputTextFace")) {
        if (!strcmp(s, "value")) {
            *out = ah_str(vm, w->text ? w->text : "");
            return 1;
        }
        if (!strcmp(s, "putString") || !strcmp(s, "backspace") || !strcmp(s, "left") ||
            !strcmp(s, "right")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_input_edit, (void *)ah_sym(vm, s)));
            return 1;
        }
    }
    if (!strcmp(w->type, "ToolTipFace")) {
        if (!strcmp(s, "lock")) {
            /* PhotoShoot PreviewRoot L2875..2877: native scalar holder.
               A: tooltip locking elsewhere still needs a separate audit. */
            if (!ah_get_member(vm, self, "_pc_photo_tip_lock", out)) {
                ah_obj *lock = ah_module_new(vm, "tip_lock", NULL, NULL, NULL);
                ah_set_member(vm, lock, "value", ah_int(0));
                *out = ah_objv(lock);
                ah_set_member(vm, self, "_pc_photo_tip_lock", *out);
            }
            return 1;
        }
        if (!strcmp(s, "attach")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_noop, m));
            return 1;
        }
        if (!strcmp(s, "value")) {
            *out = ah_str(vm, w->text ? w->text : "");
            return 1;
        }
        if (!strcmp(s, "remove")) {
            *out = ah_bind(vm, self, ah_native(vm, s, nat_tooltip_clear, m));
            return 1;
        }
    }
    if (!strcmp(s, "resetToolTip")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_tooltip_reset, m));
        return 1;
    }
    if (!strcmp(s, "warp") && !strcmp(w->type, "ScrollWindow")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_scroll_warp, m));
        return 1;
    }
    if (!strcmp(s, "updateGeometry") && !strcmp(w->type, "ScrollWindow")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_update_geometry, m));
        return 1;
    }
    if (!strcmp(s, "text") ||
        !strcmp(s, "value")) { /* value: memcard.ad StoreHouse messages, the translated text */
        *out = ah_str(vm, ui_widget_source_text(m->ui, w));
        return 1;
    }
    if (!strcmp(s, "align")) {
        *out = ah_int(w->has_runtime_align ? w->runtime_align : (int)mp_number(w->src, "align", 1));
        return 1;
    }
    if (!strcmp(s, "isClosed")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_widget_is_closed, m));
        return 1;
    }
    if (!strcmp(s, "getTextSize")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_text_size, m));
        return 1;
    }
    if (!strcmp(s, "key")) {
        *out = ah_str(vm, w->text_key ? w->text_key : "");
        return 1;
    }
    if (!strcmp(s, "visible")) {
        *out = ah_int(w->visible);
        return 1;
    }
    if (!strcmp(s, "x")) {
        *out = ah_float(w->x);
        return 1;
    }
    if (!strcmp(s, "y")) {
        *out = ah_float(w->y);
        return 1;
    }
    if (!strcmp(s, "w")) {
        *out = ah_float(w->w);
        return 1;
    }
    if (!strcmp(s, "h")) {
        *out = ah_float(w->h);
        return 1;
    }
    if (!strcmp(s, "opacity")) {
        *out = ah_float(w->opacity);
        return 1;
    }
    if (!strcmp(w->type, "ModelFace")) {
        if (!strcmp(s, "target_mode")) {
            *out = ah_int(w->model_target_mode);
            return 1;
        }
        if (!strcmp(s, "model_rotate") || !strcmp(s, "rotate_v") || !strcmp(s, "model_rotate_v")) {
            const float *v = !strcmp(s, "model_rotate") ? w->model_angles : w->model_velocity;
            ah_obj *a = ah_array_new(vm);
            for (int i = 0; i < 3; i++)
                ah_array_push(vm, a, ah_float(v[i]));
            *out = ah_objv(a);
            return 1;
        }
    }
    if (!strcmp(s, "scale_x")) {
        *out = ah_float(w->scale_x);
        return 1;
    }
    if (!strcmp(s, "scale_y")) {
        *out = ah_float(w->scale_y);
        return 1;
    }
    if (!strcmp(s, "letter_spacing")) {
        *out = ah_float(w->letter_spacing);
        return 1;
    }
    if (!strcmp(s, "movie_name")) {
        *out = ah_str(vm, w->movie_name ? w->movie_name : "");
        return 1;
    }
    if (!strcmp(s, "flash_path")) {
        *out = ah_str(vm, w->flash_path ? w->flash_path : "");
        return 1;
    }
    if (!strcmp(w->type, "FlashFace") &&
        (!strcmp(s, "setFlashPath") || !strcmp(s, "reset") || !strcmp(s, "doInitialize"))) {
        *out = ah_bind(vm, self,
                       ah_native(vm, s, nat_flash, !strcmp(s, "setFlashPath") ? (void *)1 : NULL));
        return 1;
    }
    if (!strcmp(s, "audio_track")) {
        *out = ah_int(w->movie_audio_track);
        return 1;
    }
    if (!strcmp(s, "image_path")) {
        *out = ah_str(vm, w->image_path ? w->image_path : "");
        return 1;
    }
    if (!strcmp(s, "setImagePath") &&
        (!strcmp(w->type, "ImageFace") || !strcmp(w->type, "FrameImageFace"))) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_widget_image, NULL));
        return 1;
    }
    if (!strcmp(s, "countChild") || !strcmp(s, "getChild")) {
        *out = ah_bind(
            vm, self,
            ah_native(vm, s, nat_widget_children, !strcmp(s, "getChild") ? (void *)1 : NULL));
        return 1;
    }
    if (!strcmp(s, "selected_item") && !strcmp(w->type, "OptionMenu")) {
        *out = maybe_module(m, ui_option_item(w, w->focus_index));
        return 1;
    }
    if (!strcmp(s, "focus_index") || !strcmp(s, "index")) {
        *out = ah_int(w->focus_index);
        return 1;
    }
    if (!strcmp(s, "insensitive")) {
        *out = ah_bool(w->insensitive);
        return 1;
    }
    /* the page's focused widget (labomode.ad resetFocus), and whether one can take it */
    if (!strcmp(s, "focus_widget")) {
        *out = maybe_module(m, m->focus[w->root]);
        return 1;
    }
    /* insensitive or hidden, it cannot: labomode.ad setFocusTopCommand skips
       the commands the selection disabled, down to Option and Exit */
    if (!strcmp(s, "can_focus")) {
        *out = ah_bool(available(w) && (mp_number(w->src, "can_focus", 0) ||
                                        !strcmp(w->type, "Button") || !strcmp(w->type, "ListBox")));
        return 1;
    }
    if (!strcmp(s, "setItemActive")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_item_active, m));
        return 1;
    }
    if (!strcmp(s, "active")) {
        *out = ah_bool(w->active);
        return 1;
    }
    if (!strcmp(s, "setInsensitive") && !strcmp(w->type, "OptionMenu")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_option_insensitive, m));
        return 1;
    }
    if (!strcmp(s, "applyAppend") && !strcmp(w->type, "OptionMenu")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_noop, m));
        return 1;
    }
    if (!strcmp(s, "setIndex")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_set_index, m));
        return 1;
    }
    if (!strcmp(s, "callFocus") || !strcmp(s, "outFocus")) {
        *out =
            ah_bind(vm, self,
                    ah_native(vm, s, nat_widget_focus, !strcmp(s, "outFocus") ? (void *)1 : NULL));
        return 1;
    }
    if (!strcmp(s, "getFocusedWidget")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_get_focus, m));
        return 1;
    }
    if (!strcmp(s, "setFocus")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_set_focus, m));
        return 1;
    }
    if (!strcmp(s, "setData") && !strcmp(w->type, "ColorChipFace")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_chip_set_data, m));
        return 1;
    }
    if (!strcmp(s, "setColorIndex") && !strcmp(w->type, "CarFace")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_set_color_index, m));
        return 1;
    }
    if (!strcmp(s, "car_model") && !strcmp(w->type, "CarFace")) {
        *out = car_model_value(vm, self, w);
        return 1;
    }
    if (!strcmp(s, "syncLoading") && !strcmp(w->type, "CarFace")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_car_sync, NULL));
        return 1;
    }
    if (!strcmp(s, "clearChildren") || !strcmp(s, "clearWindow")) {
        *out = ah_bind(vm, self,
                       ah_native(vm, s, s[5] == 'W' ? nat_clear_window : nat_clear_children, m));
        return 1;
    }
    if (!strcmp(s, "parent")) {
        *out = maybe_module(m, w->parent);
        return 1;
    }
    if (!strcmp(s, "first")) {
        *out = maybe_module(m, w->nkids ? w->kids[0] : NULL);
        return 1;
    }
    if (!strcmp(s, "last")) {
        *out = maybe_module(m, w->nkids ? w->kids[w->nkids - 1] : NULL);
        return 1;
    }
    if (!strcmp(s, "next") || !strcmp(s, "prev")) {
        int i = sibling_index(w) + (s[0] == 'n' ? 1 : -1);
        *out = maybe_module(m, w->parent && i >= 0 && i < w->parent->nkids ? w->parent->kids[i]
                                                                           : NULL);
        return 1;
    }
    if (!strcmp(s, "doCopy") || !strcmp(s, "getDeepCopy")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_do_copy, m));
        return 1;
    } /* memcard.ad FileSelector copies its templates */
    if (!strcmp(s, "insertChild")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_insert_child, m));
        return 1;
    }
    if (!strcmp(s, "appendChild")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_append_child, m));
        return 1;
    }
    if (!strcmp(s, "interpolateY")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_interpolate_y, m));
        return 1;
    }
    if (!strcmp(s, "movetoLast")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_move_to_last, m));
        return 1;
    }
    if (!strcmp(s, "removeChild")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_remove_child, m));
        return 1;
    }
    if (!strcmp(s, "adjustScale")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_adjust_scale, m));
        return 1;
    }
    if (!strcmp(s, "loadModelOptionMotion")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_car_option_motion, m));
        return 1;
    }
    if (!strcmp(s, "loadModelSet")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_car_crew_load, m));
        return 1;
    }
    if (!strcmp(s, "loadModelMotion")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_car_crew_motion, m));
        return 1;
    }
    if (!strcmp(s, "setModelSetPath")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_model_set_path, m));
        return 1;
    }
    if (!strcmp(s, "abbreviate")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_abbreviate, m));
        return 1;
    }
    if (!strcmp(s, "setItemTemplate") || !strcmp(s, "setItemCount") ||
        !strcmp(s, "getItemWidget") || !strcmp(s, "getItemVisible") ||
        !strcmp(s, "setItemUpdate") || !strcmp(s, "setItemWidget") || !strcmp(s, "getItemCount") ||
        !strcmp(s, "postRedraw")) {
        intptr_t op = !strcmp(s, "setItemTemplate")  ? 0
                      : !strcmp(s, "setItemCount")   ? 1
                      : !strcmp(s, "getItemWidget")  ? 2
                      : !strcmp(s, "getItemVisible") ? 3
                      : !strcmp(s, "setItemUpdate")  ? 4
                      : !strcmp(s, "setItemWidget")  ? 5
                      : !strcmp(s, "getItemCount")   ? 6
                                                     : 7;
        *out = ah_bind(vm, self, ah_native(vm, s, nat_items, (void *)op));
        return 1;
    }
    if (!strcmp(s, "adjustSize")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_adjust_size, m));
        return 1;
    }
    /* Texture decode is lazy; initialization needs no extra work. */
    if (!strcmp(s, "doInitialize")) {
        *out = ah_bind(vm, self, ah_native(vm, s, nat_noop, m));
        return 1;
    }
    ui_widget *child = ui_widget_child(w, s); /* children by name (copies) */
    if (child) {
        *out = ah_objv(widget_module(m, child));
        return 1;
    }
    const mp_node *prop = mp_prop(w->src, s); /* other numbers from the screen description */
    if (prop && prop->kind == MP_PROPERTY && prop->count == 1 &&
        prop->items[0]->kind == MP_NUMBER) {
        *out = ah_float(prop->items[0]->number);
        return 1;
    }
    return 0;
}

static int nat_car_value(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)argc;
    (void)argv;
    if (!d) {
        if (!ah_get_member(vm, self, "code", ret))
            *ret = ah_nil();
    } else
        *ret = ah_str(vm, "--"); /* specifications not decoded yet */
    return 0;
}

static int widget_set(ah_vm *vm, ah_obj *self, const char *s, ah_val v)
{
    ui_widget *w = ah_host_data(self);
    if (!w)
        return 0;
    if (!strcmp(w->type, "GraphFace")) {
        if (!strcmp(s, "points")) {
            ah_obj *points = ah_as_obj(v, AO_ARRAY);
            int n = ah_array_size(points);
            if (n < 0 || n > 256)
                return 1;
            float(*pairs)[2] = n ? malloc((size_t)n * sizeof *pairs) : NULL;
            if (n && !pairs)
                return 1;
            for (int i = 0; i < n; i++) {
                ah_obj *xy = ah_as_obj(ah_array_get(points, i), AO_ARRAY);
                if (ah_array_size(xy) != 2) {
                    free(pairs);
                    return 1;
                }
                for (int j = 0; j < 2; j++) {
                    double f = ah_num(ah_array_get(xy, j));
                    if (!isfinite(f)) {
                        free(pairs);
                        return 1;
                    }
                    pairs[i][j] = (float)f;
                }
            }
            free(w->graph_points);
            w->graph_points = pairs;
            /* 0029EE30 / 0029F798: count C0 and float pairs at C4. */
            w->graph_count = n;
            return 1;
        }
        if (!strcmp(s, "line_width")) {
            w->graph_line_width = (float)ah_num(v);
            return 1;
        }
        if (!strcmp(s, "type")) {
            const char *kind = ah_cstr(v);
            w->graph_segments = kind && !strcmp(kind, "lines");
            w->graph_type_set = 1;
            return 1;
        }
    }
    if (!strcmp(w->type, "ScaleBar")) {
        float *field = !strcmp(s, "value")           ? &w->photo_value
                       : !strcmp(s, "min")           ? &w->photo_min
                       : !strcmp(s, "max")           ? &w->photo_max
                       : !strcmp(s, "step")          ? &w->photo_step
                       : !strcmp(s, "current_value") ? &w->photo_current
                       : !strcmp(s, "repeat_start")  ? &w->photo_repeat
                                                     : NULL;
        if (field) {
            if (ah_is_num(v) && isfinite(ah_num(v))) {
                float n = (float)ah_num(v);
                /* 00239020 +0..24, clamp C8/CC before storing C4. */
                *field = field == &w->photo_value ? fminf(w->photo_max, fmaxf(w->photo_min, n)) : n;
            }
            return 1;
        }
        /* The project binds the child Composite named popup here.
           It is a widget reference, not a request to open the control. */
        if (!strcmp(s, "popup") && !ah_as_obj(v, AO_MODULE)) {
            w->option_open = ah_truthy(v);
            return 1;
        }
    }
    if (!strcmp(w->type, "SliderBar") && !strcmp(s, "value")) {
        assign_index(w, to_int(v));
        return 1;
    }
    if (!strcmp(w->type, "SliderBar") || !strcmp(w->type, "ScaleBar")) {
        /* setting.ad initSlider L142 / initSliderDisplay L166..167. */
        if (!strcmp(s, "range")) {
            ah_obj *a = ah_as_obj(v, AO_ARRAY);
            if (ah_array_size(a) == 2) {
                w->slider_min = to_int(ah_array_get(a, 0));
                w->slider_max = to_int(ah_array_get(a, 1));
                w->slider_range_set = 1;
                w->slider_display_set = 0;
            }
            return 1;
        }
        if (!strcmp(s, "display_min") || !strcmp(s, "display_max")) {
            int maximum = strstr(s, "max") != NULL;
            w->slider_display_set |= 1 << maximum;
            if (maximum)
                w->slider_display_max = ah_num(v);
            else
                w->slider_display_min = ah_num(v);
            return 1;
        }
    }
    if (!strcmp(w->type, "PhotoMapWindow")) {
        gt4_photo_mode *p = &((menu *)ah_user(vm))->photo;
        /* MPhotoMapWindow registrations 001965A0; original setters retain
           these values at +F4/+114..130 without host-side tuning/clamps. */
        float *field = !strcmp(s, "wheel_angle")  ? &p->wheel_angle
                       : !strcmp(s, "lightness")  ? &p->lightness
                       : !strcmp(s, "brightness") ? &p->brightness
                       : !strcmp(s, "contrast")   ? &p->contrast
                       : !strcmp(s, "view_angle") ? &p->view_angle
                       : !strcmp(s, "aperture")   ? &p->aperture
                                                  : NULL;
        if (field) {
            if (isfinite(ah_num(v)))
                *field = (float)ah_num(v);
            return 1;
        }
        if (!strcmp(s, "operate_mode") || !strcmp(s, "color_filter")) {
            const char *value = ah_cstr(v);
            if (value)
                snprintf(!strcmp(s, "operate_mode") ? p->operate_mode : p->color_filter, 24, "%s",
                         value);
            return 1;
        }
        if (!strcmp(s, "whiteBalance")) {
            p->white_balance = (int)ah_num(v);
            return 1;
        }
        if (!strcmp(s, "stand")) {
            p->stand = ah_truthy(v);
            return 1;
        }
    }
    if (!strcmp(w->type, "SlideShowFace") && !strcmp(s, "shuffle")) {
        ((menu *)ah_user(vm))->album_shuffle = ah_truthy(v);
        return 1;
    }
    if (!strcmp(w->type, "PhotoRenderFace")) {
        gt4_photo_mode *p = &((menu *)ah_user(vm))->photo;
        float *field = !strcmp(s, "imageScale") ? &p->image_scale
                       : !strcmp(s, "ofs_x")    ? &p->image_x
                       : !strcmp(s, "ofs_y")    ? &p->image_y
                                                : NULL;
        if (field) {
            if (isfinite(ah_num(v)))
                *field = (float)ah_num(v);
            return 1;
        }
    }
    if (!strcmp(s, "color") || !strcmp(s, "shadow_color")) {
        const char *keys[] = {"r", "g", "b", "a"};
        ah_obj *color = ah_as_obj(v, AO_MODULE);
        uint8_t rgba[4];
        if (!color)
            return 1;
        for (int i = 0; i < 4; i++) {
            ah_val component;
            if (!ah_get_member(vm, color, keys[i], &component) || !ah_is_num(component) ||
                !isfinite(ah_num(component)))
                return 1;
            rgba[i] = (uint8_t)lround(fmin(1, fmax(0, ah_num(component))) * 255);
        }
        int shadow = !strcmp(s, "shadow_color");
        memcpy(shadow ? w->runtime_shadow : w->runtime_color, rgba, 4);
        if (shadow)
            w->has_runtime_shadow = 1;
        else
            w->has_runtime_color = 1;
        return 1;
    }
    if (!strcmp(w->type, "ModelFace")) {
        if (!strcmp(s, "target_mode")) {
            w->model_target_mode = ah_num(v) != 0;
            return 1;
        }
        if (!strcmp(s, "model_rotate") || !strcmp(s, "rotate_v") || !strcmp(s, "model_rotate_v")) {
            ah_obj *a = ah_as_obj(v, AO_ARRAY);
            float values[3];
            if (ah_array_size(a) != 3)
                return 1;
            for (int i = 0; i < 3; i++) {
                double n = ah_num(ah_array_get(a, i));
                if (!isfinite(n))
                    return 1;
                values[i] = (float)n;
            }
            memcpy(!strcmp(s, "model_rotate") ? w->model_angles : w->model_velocity, values,
                   sizeof values);
            return 1;
        }
    }
    if (!strcmp(s, "action_type") && !strcmp(w->type, "CarFace")) {
        const char *t = ah_cstr(v);
        w->car_normal = t && !strcmp(t, "normal");
        return 0; /* the module keeps the value for the scripts */
    }
    if (!strcmp(s, "wing") && !strcmp(w->type, "CarFace")) {
        const char *label = ah_cstr(v);
        free(
            w->car_wing); /* WingShopRoot.tryon L12436 passes default to restore the stock model. */
        w->car_wing = label && *label && strcmp(label, "default") ? strdup(label) : NULL;
        return 0;
    }
    if (!strcmp(s, "wheel") && !strcmp(w->type, "CarFace")) {
        const char *label = ah_cstr(v);
        free(w->car_wheel);
        w->car_wheel = label && *label ? strdup(label) : NULL;
        return 0;
    }
    if (!strcmp(s, "car_garage") && !strcmp(w->type, "CarFace")) {
        const char *code = car_label_of(vm, v);
        widget_set(vm, self, "car_code", code ? ah_str(vm, code) : ah_nil());
        ah_obj *car = ah_as_obj(v, AO_MODULE);
        ah_val color;
        /* 00440938 reads live +0x470 (serialized +0x468), normal tyres.
           Re-entering a page must show the owned wheel, not only try-on. */
        uint8_t body[0x4b8];
        if (car && gt4_game_car_body(vm, car, body)) {
            uint64_t wheel;
            memcpy(&wheel, body + 0x468, 8);
            if (wheel != UINT64_MAX) {
                menu *m = ah_user(vm);
                size_t bytes = 0;
                uint8_t *labels = gt4_vol_read(m->vol, "/specdb/GT4_US2560/WHEEL.idi", &bytes);
                const char *label = gt4_specdb_label_for_id(labels, bytes, (uint32_t)wheel);
                if (label)
                    w->car_wheel = strdup(label);
                free(labels);
            }
            uint32_t slot;
            memcpy(&slot, body + 0x488, 4);
            if (slot < 3) {
                uint64_t wing;
                memcpy(&wing, body + slot * 0x178 + 0x100, 8);
                if (wing != UINT64_MAX) {
                    menu *m = ah_user(vm);
                    size_t bytes = 0;
                    uint8_t *labels = gt4_vol_read(m->vol, "/specdb/GT4_US2560/WING.idi", &bytes);
                    const char *label = gt4_specdb_label_for_id(labels, bytes, (uint32_t)wing);
                    if (label)
                        w->car_wing = strdup(label);
                    free(labels);
                }
            }
        }
        if (car && ah_get_member(vm, car, "color_index", &color) && ah_is_num(color))
            ((menu *)ah_user(vm))->car_color = w->car_color_index = (int)ah_num(color);
        return 0; /* scripts retain the garage object, not just its label */
    }
    if (!strcmp(s, "car_code") && !strcmp(w->type, "CarFace")) {
        menu *m = ah_user(vm);
        const char *code = ah_cstr(v), *name = car_name(m, code);
        free(w->car_code);
        w->car_code = code && *code ? strdup(code) : NULL;
        w->car_crew_data = NULL;
        w->car_crew_size = 0;
        w->car_crew_motion = NULL;
        w->car_crew_time = 0;
        w->car_crew_playing = 0;
        free(w->car_models);
        w->car_models = NULL;
        w->car_model_count = 0;
        free(w->car_motions);
        w->car_motions = NULL;
        w->car_motion_count = 0;
        w->car_service_geometry_valid = 0;
        free(w->car_wheel);
        w->car_wheel = NULL;
        free(w->car_wing);
        w->car_wing = NULL;
        m->car_color = w->car_color_index = -1; /* a new car shows its default paint (PCSX2) */
        ah_obj *info = ah_module_new(vm, "car_data", NULL, NULL, NULL);
        ah_set_member(vm, info, "name", ah_str(vm, name ? name : ""));
        ah_set_member(vm, info, "code", v);
        ah_set_member(vm, info, "getCarLabel",
                      ah_bind(vm, info, ah_native(vm, "getCarLabel", nat_car_value, NULL)));
        static const char *const specs[] = {"getDTrainType", "getCatPs", "getCatTq"};
        for (int i = 0; i < 3; i++)
            ah_set_member(vm, info, specs[i], ah_native(vm, specs[i], nat_car_value, (void *)1));
        bind_garage_values(vm, info);
        ah_set_member(vm, info, "getColors",
                      ah_bind(vm, info, ah_native(vm, "getColors", nat_car_paints, NULL)));
        ah_set_member(vm, info, "getColorName",
                      ah_bind(vm, info, ah_native(vm, "getColorName", nat_car_paints, (void *)1)));
        ah_set_member(vm, self, "car_data", ah_objv(info));
        return 0; /* retain the assigned code in the module */
    }
    if (!strcmp(s, "name")) {
        /* Binding a child module named "name" is not a widget rename. */
        if (ah_as_obj(v, AO_MODULE))
            return 0;
        const char *name = ah_cstr(v);
        w->name = ah_sym(vm, name ? name : "");
        return 1;
    }
    if (!strcmp(w->type, "InputTextFace") &&
        !strcmp(s, "value")) { /* the entry's text, the caret after it */
        char buf[128];
        free(w->text);
        w->text = strdup(v.t == AV_NIL ? "" : ah_to_text(v, buf, sizeof buf));
        w->caret = w->text ? (int)strlen(w->text) : 0;
        return 1;
    }
    if (!strcmp(w->type, "ToolTipFace") && !strcmp(s, "value")) {
        char buf[64];
        ui_set_tooltips(w, v.t == AV_NIL ? NULL : ah_to_text(v, buf, sizeof buf));
        return 1;
    }
    if (!strcmp(s, "text") || !strcmp(s, "key")) {
        char buf[64];
        char **dst = !strcmp(s, "text") ? &w->text : &w->text_key;
        char *copy = strdup(v.t == AV_NIL ? "" : ah_to_text(v, buf, sizeof buf));
        /* The header's car name: "--" when none. Its balance comes already
           formatted from main::game.money (gtmode_game.c game_read). */
        if (!strcmp(s, "text") && w->name && !strcmp(w->name, "label") && w->parent &&
            w->parent->parent && w->parent->parent->name &&
            !strcmp(w->parent->parent->name, "Header") && w->parent->name &&
            !strcmp(w->parent->name, "Car") && !*copy) {
            free(copy);
            copy = strdup("--");
        }
        free(*dst);
        *dst = copy;
        if (!strcmp(s, "key")) {
            free(w->text);
            w->text = NULL;
        }
        return 1;
    }
    if (!strcmp(s, "visible")) {
        w->visible = ah_truthy(v);
        return 1;
    }
    if (!strcmp(s, "align")) {
        int align = (int)ah_num(v);
        if (align >= 0 && align <= 8) {
            w->runtime_align = align;
            w->has_runtime_align = 1;
        }
        return 1;
    }
    if (!strcmp(s, "x")) {
        w->x = (float)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "y")) {
        w->y = (float)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "w")) {
        float next = (float)ah_num(v);
        if (w->nkids == 1 && !strcmp(w->kids[0]->type, "ImageFace") && w->w > 0)
            w->kids[0]->w *= next / w->w;
        w->w = next;
        w->has_geometry = 1;
        return 1;
    }
    if (!strcmp(s, "h")) {
        float next = (float)ah_num(v);
        if (w->nkids == 1 && !strcmp(w->kids[0]->type, "ImageFace") && w->h > 0)
            w->kids[0]->h *= next / w->h;
        w->h = next;
        w->has_geometry = 1;
        return 1;
    }
    if (!strcmp(s, "opacity")) {
        w->opacity = (float)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "scale_x")) {
        w->scale_x = (float)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "scale_y")) {
        w->scale_y = (float)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "letter_spacing")) {
        double n = ah_num(v);
        if (isfinite(n) && n >= 0 && n <= 32)
            w->letter_spacing = (float)n;
        return 1;
    }
    if (!strcmp(s, "focus_index") || !strcmp(s, "index")) {
        assign_index(w, (int)ah_num(v));
        return 1;
    }
    if (!strcmp(s, "insensitive")) {
        w->insensitive = ah_truthy(v);
        return 1;
    }
    if (!strcmp(s, "focus_widget")) {
        menu *m = ah_user(vm);
        ah_obj *o = ah_as_obj(v, AO_MODULE);
        if (o && ah_host_of(o) == &widget_host)
            set_focus(m, ah_host_data(o));
        return 1;
    }
    if (!strcmp(s, "active")) {
        w->active = ah_truthy(v);
        return 1;
    }
    if (!strcmp(s, "movie_name")) {
        const char *name = ah_cstr(v);
        free(w->movie_name);
        w->movie_name = name ? strdup(name) : NULL;
        return 1;
    }
    if (!strcmp(s, "audio_track")) {
        w->movie_audio_track = (int)ah_num(v);
        return 1;
    }
    if (!strcmp(s, "flash_path")) {
        const char *path = ah_cstr(v);
        free(w->flash_path);
        w->flash_path = path ? strdup(path) : NULL;
        w->flash_time = 0;
        return 1;
    }
    if (!strcmp(s,
                "image_chunk")) { /* labomode.ad update_widget: a file's picture (getIconTexture) */
        char buf[128];
        ui_widget_set_image(w, ah_to_text(v, buf, sizeof buf));
        return 1;
    }
    if (!strcmp(s, "image_path")) {
        char buf[64];
        ui_widget_set_image(w, ah_to_text(v, buf, sizeof buf));
        (void)vm;
        return 1;
    }
    return 0;
}

static void widget_mark(ah_vm *vm, ah_obj *self)
{
    ui_widget *w = ah_host_data(self);
    if (!w)
        return;
    if (w->item_template && w->item_template->script)
        ah_mark(vm, ah_objv(w->item_template->script));
    for (int i = 0; i < w->nkids; i++)
        if (w->kids[i]->script)
            ah_mark(vm, ah_objv(w->kids[i]->script));
}

static void widget_finalize(ah_obj *self)
{
    ui_widget *w = ah_host_data(self);
    if (w && w->script == self)
        w->script = NULL;
}

static const ah_host widget_host = {"widget", widget_get, widget_set, widget_mark, widget_finalize};

static ah_obj *widget_module(menu *m, ui_widget *w)
{
    if (w->script)
        return w->script;
    ah_obj *parent = w->parent ? widget_module(m, w->parent) : m->project;
    ah_obj *o = ah_module_new(m->vm, w->name ? w->name : w->type, parent, &widget_host, w);
    w->script = o;
    return o;
}

/* The screens' widgets are modules nested by name, as the scripts expect
   (ArcadeProject::TopRoot::StartScroll::SelectBox). */
static void bind_tree(menu *m, ui_widget *w)
{
    ah_obj *o = widget_module(m, w);
    ah_obj *parent = w->parent ? widget_module(m, w->parent) : m->project;
    if (w->name)
        ah_set_member(m->vm, parent, w->name, ah_objv(o));
    for (int i = 0; i < w->nkids; i++)
        bind_tree(m, w->kids[i]);
}

static int widget_color(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)data;
    ui_widget *w = ah_host_data(self);
    ah_obj *list = argc == 1 ? ah_as_obj(argv[0], AO_ARRAY) : NULL;
    if (argc != 1)
        return ah_error(vm, "setColor requires one color");
    if (list && w && w->type && !strcmp(w->type, "ColorChipFace")) {
        /* the quick menus' chips: MQuickWork.getColorChipInfo's colours,
           the used ones (the paint's flag bit 0) in bands, the first's
           metallic bit (1) for the sheen */
        w->chip_band_count = 0;
        w->chip_kind = 0;
        for (int i = 0; i < ah_array_size(list) && i < 4; i++) {
            ah_val c = ah_array_get(list, i), r, g, b, a;
            ah_obj *o = ah_as_obj(c, AO_MODULE);
            if (!o || !ah_get_member(vm, o, "r", &r) || !ah_get_member(vm, o, "g", &g) ||
                !ah_get_member(vm, o, "b", &b) || !ah_get_member(vm, o, "a", &a))
                continue;
            int flag = (int)(ah_num(a) * 255 + .5);
            if (!(flag & 1))
                continue;
            if (!w->chip_band_count)
                w->chip_kind = flag & 2 ? 1 : 2;
            w->chip_bands[w->chip_band_count][0] = (uint8_t)(ah_num(r) * 255 + .5);
            w->chip_bands[w->chip_band_count][1] = (uint8_t)(ah_num(g) * 255 + .5);
            w->chip_bands[w->chip_band_count][2] = (uint8_t)(ah_num(b) * 255 + .5);
            if (w->chip_band_count == 0)
                memcpy(w->chip_rgb, w->chip_bands[0], 3);
            w->chip_band_count++;
        }
        *ret = ah_nil();
        return 0;
    }
    widget_set(vm, self, "color", argv[0]);
    *ret = ah_nil();
    return 0;
}
