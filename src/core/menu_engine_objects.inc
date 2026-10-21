/* ---------------------------------------------------------------- engine objects */

/* Native actor state is owned by the VM object; the menu list is weak. */
struct menu_actor {
    menu_actor *next;
    menu *owner;
    ah_obj *target;
    int kind, out, repeat, running;
    float period, ratio, phase, over, dwell, lo[3], hi[3],
        value[3];   /* [2]: MMoveActor magnification */
    ah_obj *anchor; /* MAnchorActor: widget whose movement is followed */
    float base[2], anchor_base[2];
    int follow[2];
    double chase_remainder, actor_ticks;
    float chase_step, chase_range[2];
    int chase_angle;
};

static void actor_apply(menu_actor *a)
{
    ui_widget *w = ah_host_data(a->target);
    if (a->kind == 0 || a->kind == 2)
        w->opacity = a->value[0];
    else if (a->kind == 1) {
        w->x = a->value[0];
        w->y = a->value[1];
        w->magnify = a->value[2];
    } else if (a->kind == 4)
        w->rotation = a->value[0];
    else if (a->kind == 3) {
        w->scale_x = a->value[0];
        w->scale_y = a->value[1];
    }
}

static int actor_method(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    menu_actor *a = ah_host_data(self);
    const char *method = data;
    if (a->kind == 6 && (!strcmp(method, "doStart") || !strcmp(method, "doStop"))) {
        a->out = !strcmp(method, "doStop");
        a->chase_angle = 0;
        a->chase_remainder = 0;
        /* CORE 0x002841B0 resets phase; stopped updates restore the baseline. */
        if (a->out) {
            ui_widget *w = ah_host_data(a->target);
            w->x = a->base[0];
            w->y = a->base[1];
        }
        a->running = 1;
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(method, "warp")) {
        for (int i = 0; i < 3; i++)
            a->value[i] = a->out ? a->lo[i] : a->hi[i];
        a->dwell = a->over;
        actor_apply(a);
    } else if (!strcmp(method, "stop"))
        a->running = 0;
    else {
        a->running = 1;
        a->phase = 0;
        if (strcmp(method, "start")) {
            for (int i = 0; i < 3; i++)
                a->value[i] = a->out ? a->hi[i] : a->lo[i];
            actor_apply(a);
        }
    }
    *ret = ah_nil();
    return 0;
}
static int actor_get(ah_vm *vm, ah_obj *self, const char *key, ah_val *out)
{
    menu_actor *a = ah_host_data(self);
    if (a->kind == 6) {
        if (!strcmp(key, "doStart") || !strcmp(key, "doStop")) {
            *out = ah_bind(vm, self, ah_native(vm, key, actor_method, (void *)ah_sym(vm, key)));
            return 1;
        }
        if (!strcmp(key, "floating_range_x")) {
            *out = ah_float(a->chase_range[0]);
            return 1;
        }
        if (!strcmp(key, "floating_range_y")) {
            *out = ah_float(a->chase_range[1]);
            return 1;
        }
        if (!strcmp(key, "period")) {
            *out = ah_float(360.f / (60 * a->chase_step));
            return 1;
        }
    }
    if (a->kind == 1 && (!strcmp(key, "destinationX") || !strcmp(key, "destinationY"))) {
        *out = ah_float(a->lo[!strcmp(key, "destinationY")]);
        return 1;
    }
    if (!strcmp(key, "out"))
        *out = ah_bool(a->out);
    else if (!strcmp(key, "repeat"))
        *out = ah_bool(a->repeat);
    else if (!strcmp(key, "period"))
        *out = ah_float(a->period);
    else if (!strcmp(key, "ratio"))
        *out = ah_float(a->ratio);
    else if (!strcmp(key, "warp") || !strcmp(key, "reset") || !strcmp(key, "start") ||
             !strcmp(key, "stop") || !strcmp(key, "restart"))
        *out = ah_bind(vm, self, ah_native(vm, key, actor_method, (void *)ah_sym(vm, key)));
    else
        return 0;
    return 1;
}
static int actor_set(ah_vm *vm, ah_obj *self, const char *key, ah_val v)
{
    (void)vm;
    menu_actor *a = ah_host_data(self);
    if (a->kind == 6) {
        double n = ah_num(v);
        if (!strcmp(key, "floating_range_x") || !strcmp(key, "floating_range_y")) {
            if (isfinite(n))
                a->chase_range[!strcmp(key, "floating_range_y")] = (float)n;
            return 1;
        }
        if (!strcmp(key, "period")) {
            float step = (float)(360 / (60 * n));
            if (isfinite(n) && n > 0 && isfinite(step) && step > 0)
                a->chase_step = step;
            return 1;
        }
    }
    if (a->kind == 1 && (!strcmp(key, "destinationX") || !strcmp(key, "destinationY"))) {
        double value = ah_num(v);
        if (isfinite(value))
            a->lo[!strcmp(key, "destinationY")] = (float)value;
        return 1;
    }
    if (!strcmp(key, "out")) {
        a->out = ah_truthy(v);
        a->running = 1;
        if (a->kind == 1 && !a->out)
            ui_cursor_hold(a->owner->ui, 0);
        /* Closing a modal moves its panel, not the context cursor. Keep the
           selected screen position until closePage restores the underlying page. */
        menu *m = a->owner;
        if (a->kind == 1 && !m->initializing && a->out && m->depth > 1 &&
            (a->lo[1] >= 480 || a->lo[1] < 0)) {
            ui_widget *panel = ah_host_data(a->target);
            for (ui_widget *focus = m->focus[menu_active_root(m)]; focus; focus = focus->parent)
                if (focus == panel) {
                    ui_cursor_hold(m->ui, 1);
                    break;
                }
        }
    } else if (!strcmp(key, "destination") && a->kind == 4) {
        float goal = (float)ah_num(v);
        if (!isfinite(goal))
            return 1;
        float delta = fmodf(goal - a->value[0], 360);
        if (a->repeat && delta < 0)
            delta += 360;
        if (!a->repeat && delta > 0)
            delta -= 360;
        a->hi[0] = a->value[0] + delta;
        a->out = 0;
        a->running = 1;
    } else if (!strcmp(key, "over")) {
        double n = ah_num(v);
        if (isfinite(n) && n >= 0 && n <= 60)
            a->over = (float)n;
    } else if (!strcmp(key, "repeat"))
        a->repeat = ah_truthy(v);
    else if (!strcmp(key, "period")) {
        double n = ah_num(v);
        if (isfinite(n) && n > 0)
            a->period = (float)n;
    } else if (!strcmp(key, "ratio")) {
        double n = ah_num(v);
        if (isfinite(n) && n > 0 && n <= 1)
            a->ratio = (float)n;
    } else
        return 0;
    return 1;
}
static void actor_mark(ah_vm *vm, ah_obj *self)
{
    menu_actor *a = ah_host_data(self);
    ah_mark(vm, ah_objv(a->target));
    if (a->anchor)
        ah_mark(vm, ah_objv(a->anchor));
}
static void actor_free(ah_obj *self)
{
    menu_actor *a = ah_host_data(self);
    menu_actor **link = &a->owner->actors;
    while (*link && *link != a)
        link = &(*link)->next;
    if (*link)
        *link = a->next;
    free(a);
}
static const ah_host actor_host = {"actor", actor_get, actor_set, actor_mark, actor_free};
void menu_native_method(menu *m, ui_widget *w, const char *method, ah_native_fn fn, void *user)
{
    if (m && w) {
        ah_obj *receiver = widget_module(m, w);
        /* P: native PC callbacks need the same receiver as authored methods.
           AO_NATIVE is intentionally not automatically bound by the VM. */
        ah_set_member(m->vm, receiver, method,
                      ah_bind(m->vm, receiver, ah_native(m->vm, method, fn, user)));
    }
}
static int nat_page(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret);
void menu_native_page(menu *m, int root)
{
    if (m && root >= 0 && root < ui_root_count(m->ui)) {
        if (!strcmp(ah_module_name(m->project), "OptionProject")) {
            ah_val page = maybe_module(m, ui_root_widget(m->ui, root)), result;
            nat_page(m->vm, NULL, (void *)2, 1, &page, &result);
        } else {
            m->pending_kind = 2;
            m->pending_page = root;
            m->pending_mores = 0;
        }
    }
}
void menu_native_focus(menu *m, ui_widget *w)
{
    if (m)
        set_focus(m, w);
}
static int nat_actor(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    ah_obj *target = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    if (!target || ah_host_of(target) != &widget_host)
        return ah_error(vm, "actor requires a widget");
    const char *name = d;
    int kind = !strcmp(name, "MFadeActor")     ? 0
               : !strcmp(name, "MMoveActor")   ? 1
               : !strcmp(name, "MBlinkActor")  ? 2
               : !strcmp(name, "MScaleActor")  ? 3
               : !strcmp(name, "MRotateActor") ? 4
               : !strcmp(name, "MAnchorActor") ? 5
               : !strcmp(name, "MChaseActor")  ? 6
                                               : -1;
    if (kind < 0) { /* Unsupported actor kinds remain explicit in the log. */
        ah_logf(vm, "unsupported actor: %s", name);
        ah_obj *stub = ah_module_new(vm, name, NULL, NULL, NULL);
        const char *methods[] = {"warp", "reset", "start", "stop", "restart"};
        for (int i = 0; i < 5; i++)
            ah_set_member(vm, stub, methods[i], ah_native(vm, methods[i], nat_noop, NULL));
        *ret = ah_objv(stub);
        return 0;
    }
    menu_actor *a = calloc(1, sizeof *a);
    if (!a)
        return ah_error(vm, "out of memory");
    menu *m = ah_user(vm);
    ui_widget *w = ah_host_data(target);
    if (kind == 1 && !strcmp(ah_module_name(m->project), "LicenseProject") &&
        ui_widget_child(w, "Info")) {
        /* quick-share/icon.ad L46 replaces space.actor on each entry.
           The old native object loses its reference; our GC-backed actor
           list can retain it until collection. Stop that superseded move
           so it cannot overwrite the new scroll position every frame. */
        for (menu_actor *old = m->actors; old; old = old->next)
            if (old->kind == 1 && old->target == target)
                old->running = 0;
    }
    a->owner = m;
    a->target = target;
    a->kind = kind;
    a->running = 1;
    a->period = 0.25f;
    a->ratio = 0.3f;
    a->hi[0] = kind == 1 ? w->x : kind == 3 ? w->scale_x : argc > 1 ? (float)ah_num(argv[1]) : 1;
    a->hi[1] = kind == 1 ? w->y : kind == 3 ? w->scale_y : 0;
    a->lo[0] = kind == 1 || kind == 3 ? (argc > 1 ? (float)ah_num(argv[1]) : 0)
                                      : (argc > 2 ? (float)ah_num(argv[2]) : 0);
    a->lo[1] = argc > 2 ? (float)ah_num(argv[2]) : a->lo[0];
    if (kind == 4) {
        a->lo[0] = a->hi[0];
        a->repeat = argc > 2 ? ah_truthy(argv[2]) : 1;
    }
    /* MMoveActor's optional 4th argument is the magnification at the "out"
       position (MakerRoot.setMagnifyActor: 0.8/0.5 for the focused logo). */
    a->hi[2] = 1;
    a->lo[2] = kind == 1 && argc > 3 && ah_num(argv[3]) > 0 ? (float)ah_num(argv[3]) : 1;
    a->value[0] = a->hi[0];
    a->value[1] = a->hi[1];
    a->value[2] = a->hi[2];
    if (kind == 6) {
        /* CORE 0x00283618: stopped, x range 0, y range 16, 8.5 deg/tick. */
        a->base[0] = w->x;
        a->base[1] = w->y;
        a->out = 1;
        a->chase_range[1] = 16;
        a->chase_step = 8.5f;
    }
    if (kind == 5) {
        ah_obj *anchor = argc > 1 ? ah_as_obj(argv[1], AO_MODULE) : NULL;
        if (!anchor || ah_host_of(anchor) != &widget_host) {
            free(a);
            return ah_error(vm, "MAnchorActor requires two widgets");
        }
        ui_widget *t = ah_host_data(anchor);
        a->anchor = anchor;
        a->base[0] = w->x;
        a->base[1] = w->y;
        a->anchor_base[0] = t->x;
        a->anchor_base[1] = t->y;
        a->follow[0] = argc > 2 && ah_truthy(argv[2]);
        a->follow[1] = argc > 3 && ah_truthy(argv[3]);
    }
    a->next = m->actors;
    m->actors = a;
    *ret = ah_objv(ah_module_new(vm, name, NULL, &actor_host, a));
    return 0;
}

/* A face of a ScrollBox's inc_arrow/dec_arrow composite. */
static int arrow_face(const ui_widget *w, const char *arrow)
{
    return w && w->parent && w->parent->name && !strcmp(w->parent->name, arrow) &&
           w->parent->parent && !strcmp(w->parent->parent->type, "ScrollBox");
}
/* For an inc_arrow face's fade actor: the fade actor on the same face of
   the same ScrollBox's dec_arrow, which leads it. */
static menu_actor *arrow_partner(menu *m, const menu_actor *a)
{
    const ui_widget *w = ah_host_data(a->target);
    if (a->kind != 0 || !arrow_face(w, "inc_arrow") || !w->name)
        return NULL;
    for (menu_actor *b = m->actors; b; b = b->next) {
        const ui_widget *v = ah_host_data(b->target);
        if (b != a && b->kind == 0 && arrow_face(v, "dec_arrow") &&
            v->parent->parent == w->parent->parent && v->name && !strcmp(v->name, w->name))
            return b;
    }
    return NULL;
}
static int arrow_partner_of(menu *m, const menu_actor *lead)
{
    for (menu_actor *a = m->actors; a; a = a->next)
        if (arrow_partner(m, a) == lead)
            return 1;
    return 0;
}

float menu_cursor_speed(menu *m)
{
    ah_val value;
    return m && ah_get_member(m->vm, m->context, "cursor_speed_ratio", &value) && ah_is_num(value)
               ? (float)ah_num(value)
               : 1.0f;
}

static void menu_animate_frame(menu *m, float seconds)
{
    if (!m || !isfinite(seconds) || seconds <= 0)
        return;
    if (seconds > 0.1f)
        seconds = 0.1f;
    int cursor_root = menu_active_root(m);
    if (cursor_root >= 0)
        ui_cursor_select(m->ui, m->focus[cursor_root]);
    update_progress(m, ui_root_widget(m->ui, cursor_root));
    ah_val cursor_ratio;
    if (ah_get_member(m->vm, m->context, "cursor_speed_ratio", &cursor_ratio) &&
        ah_is_num(cursor_ratio))
        ui_cursor_speed(m->ui, (float)ah_num(cursor_ratio));
    for (menu_actor *a = m->actors; a; a = a->next) {
        ui_widget *w = ah_host_data(a->target);
        if (!a->running || w->root != menu_active_root(m))
            continue;
        if (a->kind == 6) {
            /* CORE 0x00284270 truncates phase to integer degrees each tick.
               Preserve that 60 Hz clock independently of presentation FPS. */
            if (a->out) {
                w->x = a->base[0];
                w->y = a->base[1];
                continue;
            }
            a->chase_remainder += seconds * 60.0;
            while (a->chase_remainder >= 1.0 - 1e-6) {
                a->chase_remainder -= 1;
                a->chase_angle = (int)fmodf(a->chase_angle + a->chase_step, 360.f);
            }
            float wave = sinf(a->chase_angle * (3.141592502593994f / 180.f));
            w->x = a->base[0] + wave * a->chase_range[0];
            w->y = a->base[1] - fabsf(wave) * a->chase_range[1];
            continue;
        }
        if (a->kind == 5) {
            /* MAnchorActor(widget, anchor, follow_x, follow_y): the widget moves
               by as much as the anchor has moved (MakerRoot's nation flags
               scroll with the manufacturer logos). */
            ui_widget *t = a->anchor ? ah_host_data(a->anchor) : NULL;
            if (t) {
                if (a->follow[0])
                    w->x = a->base[0] + t->x - a->anchor_base[0];
                if (a->follow[1])
                    w->y = a->base[1] + t->y - a->anchor_base[1];
            }
            continue;
        }
        /* A widget's fade/blink actor advances with the widget's own update,
           so it holds while the widget isn't shown (e.g. a ScrollBox arrow
           face hidden by its state). */
        unsigned tick = ui_draw_tick(m->ui);
        /* ScrollBox arrows (TopRoot's StartScroll: setFadeActor2 on both
           bga faces): in PCSX2 one is fully shown while the other is gone,
           so inc_arrow's fade is kept the complement of dec_arrow's. */
        menu_actor *lead = arrow_partner(m, a);
        if (lead) {
            a->out = !lead->out;
            for (int i = 0; i < 3; i++)
                a->value[i] = a->hi[i] + a->lo[i] - lead->value[i];
            actor_apply(a);
            continue;
        }
        int paired = a->kind == 0 && arrow_face(w, "dec_arrow") && arrow_partner_of(m, a);
        if (!paired && (a->kind == 0 || a->kind == 2) && tick && w->drawn_tick != tick)
            continue;
        if (a->kind == 0 && a->repeat && a->dwell > 0) {
            a->dwell = fmaxf(0, a->dwell - seconds);
            continue;
        }
        if (a->kind == 2) {
            a->phase = fmodf(a->phase + seconds, 2 * a->period);
            a->value[0] = a->phase < a->period ? a->hi[0] : a->lo[0];
        } else if (a->kind == 3 || a->kind == 4) {
            /* Scale/rotation actors have their own native implementations;
               retain their existing adapter until those are traced. */
            float blend = 1 - powf(1 - a->ratio, seconds * 60);
            for (int i = 0; i < 3; i++) {
                float goal = a->out ? a->lo[i] : a->hi[i];
                a->value[i] += (goal - a->value[i]) * blend;
                if (fabsf(goal - a->value[i]) < .001f)
                    a->value[i] = goal;
            }
            if (a->kind != 4 && a->repeat && a->value[0] == (a->out ? a->lo[0] : a->hi[0])) {
                a->out = !a->out;
                a->dwell = a->over;
            }
        } else {
            /* MMoveActor 002C1960 / MFadeActor 0020C478. Advance
               original frames, without exponential dt resampling or snapping. */
            a->actor_ticks += (double)seconds * 60.0;
            while (a->actor_ticks >= 1.0 - 1e-6) {
                a->actor_ticks -= 1;
                for (int i = 0; i < 3; i++) {
                    float goal = a->out ? a->lo[i] : a->hi[i];
                    if (a->kind == 1 || a->kind == 3 || a->kind == 4)
                        a->value[i] = gt4_move_value(a->value[i], goal, a->ratio);
                    else {
                        float step = fabsf(a->hi[i] - a->lo[i]) / (a->period * 60.0f);
                        if (goal < a->value[i])
                            step = -step;
                        a->value[i] = gt4_fade_value(a->value[i], goal, step);
                    }
                }
                if (a->kind != 4 && a->repeat && a->value[0] == (a->out ? a->lo[0] : a->hi[0])) {
                    a->out = !a->out;
                    a->dwell = a->over;
                    if (a->dwell > 0)
                        break;
                }
            }
        }
        actor_apply(a);
    }
    ui_animate(m->ui, menu_active_root(m), seconds);
}

static void context_cursor_visibility(menu *m)
{
    ah_val visible;
    /* A modal has its own visible cursor; the underlying context may hide
       its arrow while a dialog owns focus. TopRoot explicitly hides it. */
    int shown = m->depth > 1 || !ah_get_member(m->vm, m->context, "cursor_visible", &visible) ||
                ah_truthy(visible);
    ui_cursor_show(m->ui, shown);
}

void menu_animate(menu *m, float seconds)
{
    if (!m)
        return;
    context_cursor_visibility(m);
    if (!isfinite(seconds) || seconds <= 0)
        return;
    m->pointer_sound_delay=fmaxf(0,m->pointer_sound_delay-seconds);
    int cursor_root = menu_active_root(m);
    if (cursor_root >= 0)
        ui_cursor_select(m->ui, m->focus[cursor_root]);
    update_progress(m, ui_root_widget(m->ui, cursor_root));
    m->animation_ticks += (double)fminf(seconds, .1f) * 60.0;
    /* A panel and its cursor must advance together for each original frame,
       rather than advancing the panel N times before chasing its final target. */
    while (m->animation_ticks >= 1.0 - 1e-6) {
        m->animation_ticks -= 1;
        menu_animate_frame(m, 1.0f / 60.0f);
    }
}

void menu_set_presenter(menu *m, void (*fn)(menu *, float, void *), void *user)
{
    if (m) {
        m->present_wait = fn;
        m->present_user = user;
    }
}
static int nat_sync(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    menu *m = data;
    double seconds = argc ? ah_num(argv[0]) : 1.0 / 60;
    if (isfinite(seconds) && seconds >= 0 && m->present_wait) {
        /* A dialog queues pushPage, starts its incoming actors, then Syncs.
           Commit that push before presenting: waiting until enterEventLoop
           painted the old Purchase page throughout the popup entrance. */
        if (m->pending_kind == 1 && !m->initializing)
            apply_pages(m);
        m->present_wait(m, (float)seconds, m->present_user);
    }
    *ret = ah_nil();
    return 0;
}

/* GT Mode requests a single render context. Its pages share the current
   surface and event queue; returning that context also preserves transition
   assignments for later callbacks. Reject additional surfaces explicitly. */
static int nat_render_context(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    if (argc != 1 || !ah_is_num(argv[0]))
        return ah_error(vm, "render context requires a numeric count/index");
    double n = ah_num(argv[0]);
    if (data) {
        if (n != 1)
            return ah_error(vm, "only one UI render context is supported");
        m->render_context_count = 1;
        *ret = ah_nil();
    } else {
        if (n != 0 || !m->render_context_count)
            return ah_error(vm, "UI render context index out of range");
        *ret = ah_objv(m->context);
    }
    return 0;
}

static int nat_transition_method(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                                 ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ah_val value;
    ah_get_member(vm, self, "period", &value);
    float period = (float)ah_num(value);
    if (!isfinite(period) || period < 0 || period > 2)
        period = .25f;
    const char *method = data;
    const char *name = ah_module_name(self);
    if (getenv("MOON_TRANSITION_TRACE"))
        fprintf(stderr, "transition %s/%s %s.%s period %.6f pending %.6f\n", m->project_label,
                ui_root_name(m->ui, menu_active_root(m)), name, method, period, m->pending_fade_in);
    int kind = name && !strcmp(name, "MCrossTransition")   ? 1
               : name && !strcmp(name, "MBlockTransition") ? 2
                                                           : 0;
    if (kind) {
        gt4_transition_motion *motion = ui_transition_state(m->ui);
        if (motion->kind != kind)
            gt4_transition_init(motion, kind);
        if (kind == 2 && ah_get_member(vm, self, "_block_seed", &value))
            motion->random = (uint32_t)ah_num(value);
        static const char *const fields[] = {"range",        "slope",   "magnify_ratio",
                                             "magnify_diff", "opacity", "type",
                                             "with_black",   "columns", "rows"};
        for (int k = 0; k < 9; k++)
            if (ah_get_member(vm, self, fields[k], &value)) {
                float n = (float)ah_num(value);
                switch (k) {
                case 0:
                    motion->range = n;
                    break;
                case 1:
                    motion->slope = n;
                    break;
                case 2:
                    motion->magnify_ratio = n;
                    break;
                case 3:
                    motion->magnify_diff = n;
                    break;
                case 4:
                    motion->black_opacity = n;
                    break;
                case 5:
                    motion->type = (int)n;
                    break;
                case 6:
                    motion->with_black = ah_truthy(value);
                    break;
                case 7:
                    motion->columns = (int)fminf(32, fmaxf(1, n));
                    break;
                case 8:
                    motion->rows = (int)fminf(24, fmaxf(1, n));
                    break;
                }
            }
        if (!strcmp(method, "panIn")) {
            ui_transition_start(m->ui, kind, 1, period);
            if (m->initializing)
                m->pending_fade_in = -1;
            else
                ui_transition_reveal(m->ui);
        } else if (!strcmp(method, "panOut"))
            ui_transition_start(m->ui, kind, 0, period);
        else if (!strcmp(method, "panOutIn")) {
            ui_transition_start(m->ui, kind, 2, period);
            /* 0027B2A8/0028A6B0: outgoing stage signals ready in one frame;
               it does not fade to black before changing pages. */
            if (m->present_wait)
                m->present_wait(m, 1.0f / 60, m->present_user);
            else
                ui_animate(m->ui, menu_active_root(m), 1.0f / 60);
            m->pending_fade_in = -1;
        } else {
            float remaining = ui_transition_remaining(m->ui);
            if (remaining > 0 && m->present_wait)
                m->present_wait(m, remaining, m->present_user);
        }
        if (kind == 2)
            ah_set_member(vm, self, "_block_seed", ah_int(motion->random));
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(method, "panIn")) {
        if (m->initializing && period > 0) {
            ui_fade(m->ui, 1, 1, 0);
            m->pending_fade_in = period;
        } else
            ui_fade(m->ui, 1, 0, period);
    } else if (!strcmp(method, "panOut"))
        ui_fade(m->ui, 0, 1, period);
    else if (!strcmp(method, "panOutIn")) {
        if (period == 0) {
            ui_fade(m->ui, 0, 0, 0);
            m->pending_fade_in = 0;
            *ret = ah_nil();
            return 0;
        }
        /* MColorTransition 002874F0: period applies to each leg. */
        /* arcade.ad L4301 and page_util.ad L40 may request consecutive
           panOutIn calls. Preserve the current opacity, including black. */
        ui_fade(m->ui, ui_fade_opacity(m->ui), 1, period);
        while (m->present_wait && ui_fade_remaining(m->ui) > 0)
            m->present_wait(m, ui_fade_remaining(m->ui), m->present_user);
        m->pending_fade_in = period;
    } else {
        float remaining = ui_fade_remaining(m->ui);
        while (remaining > 0 && m->present_wait) {
            m->present_wait(m, remaining, m->present_user);
            remaining = ui_fade_remaining(m->ui);
        }
    }
    *ret = ah_nil();
    return 0;
}
static int nat_transition(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    ah_obj *o = ah_module_new(vm, data, NULL, NULL, NULL);
    /* Constructors 00286EA0 (colour), 00289888 (cross). */
    int block = !strcmp(data, "MBlockTransition");
    ah_set_member(vm, o, "period",
                  ah_float(block                               ? .125
                           : !strcmp(data, "MColorTransition") ? .25
                                                               : .5));
    if (block) {
        ah_set_member(vm, o, "_block_seed", ah_int(0x12345678));
        ah_set_member(vm, o, "range", ah_float(.125));
        ah_set_member(vm, o, "slope", ah_float(2));
        ah_set_member(vm, o, "columns", ah_int(32));
        ah_set_member(vm, o, "rows", ah_int(24));
    } else if (!strcmp(data, "MCrossTransition")) {
        ah_set_member(vm, o, "type", ah_int(1));
        ah_set_member(vm, o, "with_black", ah_int(1));
        ah_set_member(vm, o, "opacity", ah_float(0.09999999403953552f));
        ah_set_member(vm, o, "magnify_ratio", ah_float(.5));
        ah_set_member(vm, o, "magnify_diff", ah_float(0));
    }
    static const char *const methods[] = {"panIn", "panOut", "panOutIn", "syncIn", "syncOut"};
    for (int i = 0; i < 5; i++)
        ah_set_member(
            vm, o, methods[i],
            ah_bind(vm, o, ah_native(vm, methods[i], nat_transition_method, (void *)methods[i])));
    *ret = ah_objv(o);
    return 0;
}
static int nat_progress(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    ah_obj *o = ah_module_new(vm, "MProgress", NULL, NULL, NULL);
    ah_set_member(vm, o, "value", ah_float(0));
    *ret = ah_objv(o);
    return 0;
}

static void update_progress(menu *m, ui_widget *w)
{
    if (!w)
        return;
    if (!strcmp(w->type, "ProgressFace") && w->script) {
        ah_val progress, value;
        if (ah_get_member(m->vm, w->script, "progress", &progress) &&
            ah_get_member(m->vm, ah_as_obj(progress, AO_MODULE), "value", &value)) {
            double n = ah_num(value);
            if (isfinite(n))
                w->progress = (float)fmin(1, fmax(0, n));
        }
    }
    for (int i = 0; i < w->nkids; i++)
        update_progress(m, w->kids[i]);
}

/* Authored RootWindow.bgm and main.sound.keepMusic own the requested group. */
static void request_music(menu *m, const char *name)
{
    if (!name || !strcmp(name, "keep"))
        return;
    size_t n = strlen(name);
    if (n >= sizeof m->music_name)
        return;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_'))
            return;
    }
    for (size_t i = 0; i <= n; i++)
        m->music_name[i] = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + 32 : name[i];
    m->music_set = 1;
    if (n && m->audio_preparer)
        m->audio_preparer(m, m->audio_preparer_user);
}
static int nat_music(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    if (data && m->music_fader)
        m->music_fader(argc ? (float)ah_num(argv[0]) : 0);
    request_music(m, data ? "" : argc ? ah_cstr(argv[0]) : NULL);
    *ret = ah_nil();
    return 0;
}
const char *menu_music_name(menu *m)
{
    return m && m->music_set ? m->music_name : NULL;
}

/* startStreamEx registration 0023ED48 +0x6F4, implementation 0023E740.
   Keep PCM requests separate from /music MIDI names. quick-arcade.ad L159
   requests pcm/default_quick, track -1, repeat 1 and normalized menu volume. */
static int nat_quick_stream(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    if (!data) {
        const char *device = argc ? ah_cstr(argv[0]) : NULL,
                   *group = argc > 1 ? ah_cstr(argv[1]) : NULL;
        /* gtmode.ad:14542 uses pcm/get_car with this same registration.
           Validate group syntax here; the existing ADS loader resolves it. */
        if (!device || strcmp(device, "pcm") || !group || !*group ||
            strlen(group) >= sizeof m->pcm_group)
            return ah_error(vm, "unsupported PCM stream");
        for (const unsigned char *p = (const unsigned char *)group; *p; p++)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_'))
                return ah_error(vm, "invalid PCM group");
        double volume = argc > 4 ? ah_num(argv[4]) : 120. / 128.;
        if (!isfinite(volume))
            return ah_error(vm, "invalid PCM volume");
        snprintf(m->pcm_group, sizeof m->pcm_group, "%s", group);
        m->pcm_sequence = argc > 2 ? to_int(argv[2]) : -1;
        m->pcm_repeat = argc > 3 ? to_int(argv[3]) : 1;
        m->pcm_serial++;
        m->pcm_volume = (int)lround(fmin(1, fmax(0, volume)) * 128);
    } else
        m->pcm_group[0] = 0;
    m->pcm_paused = 0;
    m->pcm_requested = 1;
    if (!data && m->audio_preparer)
        m->audio_preparer(m, m->audio_preparer_user);
    *ret = ah_nil();
    return 0;
}
/* sound.pauseStream(flag) (slide.ad musiccontrol.h music_player_do_pause /
   do_resume): the stream held where it is, or going on */
static int nat_pcm_pause(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    (void)d;
    menu *m = ah_user(vm);
    m->pcm_paused = argc && ah_truthy(argv[0]);
    *ret = ah_nil();
    return 0;
}
int menu_pcm_paused(menu *m)
{
    return m && m->pcm_paused;
}
void menu_pcm_state(menu *m, int playing)
{
    if (m)
        m->pcm_playing = playing;
}
void menu_pcm_parameters(menu *m, int *sequence, int *repeat, unsigned *serial)
{
    if (sequence)
        *sequence = m ? m->pcm_sequence : -1;
    if (repeat)
        *repeat = m ? m->pcm_repeat : 1;
    if (serial)
        *serial = m ? m->pcm_serial : 0;
}
static int nat_pcm_playing(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_int(((menu *)ah_user(vm))->pcm_playing);
    return 0;
}
const char *menu_pcm_request(menu *m, int *volume)
{
    if (volume)
        *volume = m ? m->pcm_volume : 0;
    return m && m->pcm_requested ? m->pcm_group : NULL;
}

static int nat_sound_load(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    if (!m->sound)
        m->sound = gt4_sound_open(m->vol);
    *ret = ah_nil();
    return 0;
}
static int nat_sound_unload(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = data;
    gt4_sound_close(m->sound);
    m->sound = NULL;
    *ret = ah_nil();
    return 0;
}
static int nat_sound_play(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    menu *m = data;
    /* Hover runs the original enter/leave/preview callbacks for visuals.
       Its one navigation sound is emitted by pointer_set_hover instead. */
    if (m->hover_dispatch) { *ret=ah_nil(); return 0; }
    if (argc && ah_cstr(argv[0])) {
        if (!m->sound)
            m->sound = gt4_sound_open(m->vol);
        gt4_sound_gain(m->sound, (float)(menu_option_number(m, "menu_se_volume_raw", 120) / 120.0));
        if (m->sound && !gt4_sound_play(m->sound, ah_cstr(argv[0])))
            ah_logf(vm, "unsupported sound: %s", ah_cstr(argv[0]));
    }
    *ret = ah_nil();
    return 0;
}

void menu_play_effect(menu *m, const char *name)
{
    if (m) {
        ah_val sound = ah_str(m->vm, name), result;
        nat_sound_play(m->vm, NULL, m, 1, &sound, &result);
    }
}

/* MScriptWatcher(context, func): calls func(context) every `interval` frames
   once appended. */
static int nat_watcher_append(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = d;
    *ret = ah_nil();
    for (int i = 0; i < m->nwatchers; i++)
        if (m->watchers[i].obj == self)
            return 0;
    watcher *grown = realloc(m->watchers, (size_t)(m->nwatchers + 1) * sizeof *grown);
    if (!grown)
        return ah_error(vm, "out of memory");
    m->watchers = grown;
    m->watchers[m->nwatchers].obj = self;
    m->watchers[m->nwatchers++].count = 0;
    ah_pin(vm, ah_objv(self));
    return 0;
}

static int nat_watcher_remove(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                              ah_val *ret)
{
    (void)argc;
    (void)argv;
    menu *m = d;
    *ret = ah_nil();
    for (int i = 0; i < m->nwatchers; i++)
        if (m->watchers[i].obj == self) {
            ah_unpin(vm, ah_objv(self));
            m->watchers[i] = m->watchers[--m->nwatchers];
            break;
        }
    return 0;
}

static int nat_watcher(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    ah_obj *w = ah_module_new(vm, "MScriptWatcher", NULL, NULL, NULL);
    ah_set_member(vm, w, "context", argc > 0 ? argv[0] : ah_nil());
    ah_set_member(vm, w, "func", argc > 1 ? argv[1] : ah_nil());
    ah_set_member(vm, w, "interval", ah_int(1));
    ah_set_member(vm, w, "append", ah_bind(vm, w, ah_native(vm, "append", nat_watcher_append, d)));
    ah_set_member(vm, w, "remove", ah_bind(vm, w, ah_native(vm, "remove", nat_watcher_remove, d)));
    *ret = ah_objv(w);
    return 0;
}

/* Region of the disc: the supported disc is NTSC-U (config/supported_discs.json). */
static int nat_region(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_str(vm, "us");
    return 0;
}
/* 001AC598 returns D_00692E20, "NTSC", on the supported US disc. */
static int nat_video_system(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    *ret = ah_str(vm, "NTSC");
    return 0;
}

static ah_obj *submodule(ah_vm *vm, ah_obj *parent, const char *name)
{
    ah_obj *o = ah_module_new(vm, name, parent, NULL, NULL);
    ah_set_member(vm, parent, name, ah_objv(o));
    return o;
}

/* Presentation-only virtual loading, varying across 30..60 rendered ticks.
   No blocking 500-ms sleep: the presenter keeps drawing and pumping events. */
float menu_loading_duration(menu *m)
{
    if (!m->loading_seed)
        m->loading_seed = ((uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)m) | 1u;
    uint32_t n = m->loading_seed;
    n ^= n << 13;
    n ^= n >> 17;
    n ^= n << 5;
    m->loading_seed = n;
    return (30u + n % 31u) / 60.0f;
}

/* Deferred page operations avoid entering another script inside a callback. */
static int nat_page(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    ah_obj *o = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    ui_widget *w = o && ah_host_of(o) == &widget_host ? ah_host_data(o) : NULL;
    if (!w || strcmp(w->type, "RootWindow"))
        return ah_error(vm, "page operation needs a RootWindow");
    int option_switch = (int)(intptr_t)d == 2 && menu_active_root(m) >= 0 &&
                        w->root != menu_active_root(m) &&
                        !strcmp(ah_module_name(m->project), "OptionProject");
    /* Native settings callbacks also use the authored colour transition.
       Scripted start_page already queued its outgoing/incoming legs. */
    if (option_switch && m->pending_fade_in == 0) {
        ah_val transition, context = ah_objv(m->context), result;
        if (ah_get_member(vm, m->context, "transition", &transition))
            ah_call_member(vm, ah_as_obj(transition, AO_MODULE), "panOutIn", 1, &context, &result);
    }
    if (option_switch)
        ui_cursor_icon(m->ui, "load");
    /* startPage -> 0022F6A0 pauses context updates around two Sync(2)
       barriers (0022F6E0, 0022F79C). Keep the old cursor/loading sprite
       presented before the deferred teardown; otherwise a cached page
       resets "load" before any frame ever displays it. */
    if ((int)(intptr_t)d == 2 && menu_active_root(m) >= 0 && w->root != menu_active_root(m) &&
        m->present_wait) {
        int paused = ui_cursor_pause(m->ui, 1);
        float loading = menu_loading_duration(m);
        m->present_wait(m, 2.0f / 60.0f, m->present_user);
        /* 00233D18 sets RootWindow+E8 bit 3 (pending close), not visible.
           The old root is finalized only after Sync at 0022F79C. Hiding it
           here cleared colour/direct transitions to one black frame before
           the destination/reveal. Keep presenting its last live surface. */
        m->present_wait(m, loading - 2.0f / 60.0f, m->present_user);
        ui_cursor_pause(m->ui, paused);
    }
    if (option_switch)
        ui_cursor_icon(m->ui, NULL);
    /* photo_shoot.ad PreviewRoot.onCancel L2890..2892 closes the render
       context with panOut/syncOut, then starts ShootRoot. The native adapter
       shares that context's surface: carry its incoming leg across startPage
       instead of retaining the completed opaque outgoing overlay forever.
       Keep the authored transition period; page initialization may replace it. */
    if ((int)(intptr_t)d == 2 && !strcmp(m->project_label, "photo_shoot") &&
        m->pending_fade_in == 0 && ui_fade_opacity(m->ui) >= 1 &&
        ui_fade_remaining(m->ui) == 0) {
        ah_val transition, period;
        if (ah_get_member(vm, m->context, "transition", &transition) &&
            ah_get_member(vm, ah_as_obj(transition, AO_MODULE), "period", &period) &&
            ah_is_num(period) && ah_num(period) > 0 && ah_num(period) <= 2)
            m->pending_fade_in = (float)ah_num(period);
    }
    m->pending_kind = (int)(intptr_t)d;
    m->pending_page = w->root;
    /* startPage (0022BC20) pushes each of its pages in order (0022F890):
       arcade CategoryRoot.onCancel's startPage(TopRoot, GameRoot) leaves
       GameRoot on top of TopRoot */
    m->pending_mores = 0;
    for (int i = 1; m->pending_kind == 2 && i < argc && m->pending_mores < 8; i++) {
        ah_obj *more = ah_as_obj(argv[i], AO_MODULE);
        ui_widget *page = more && ah_host_of(more) == &widget_host ? ah_host_data(more) : NULL;
        if (page && !strcmp(page->type, "RootWindow"))
            m->pending_more[m->pending_mores++] = page->root;
    }
    *ret = ah_nil();
    return 0;
}

static int nat_event_new(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    int script = d != NULL;
    if (argc < (script ? 3 : 1))
        return ah_error(vm, "event constructor: missing arguments");
    ah_obj *e = ah_module_new(vm, script ? "MScriptEvent" : "MFunctionEvent", NULL, NULL, NULL);
    ah_obj *args = ah_array_new(vm);
    if (script) {
        ah_set_member(vm, e, "target", argv[1]);
        ah_set_member(vm, e, "method", argv[2]);
        ah_array_push(vm, args, argv[0]);
        ah_obj *extra = argc > 3 ? ah_as_obj(argv[3], AO_ARRAY) : NULL;
        for (int i = 0; i < ah_array_size(extra); i++)
            ah_array_push(vm, args, ah_array_get(extra, i));
    } else {
        ah_set_member(vm, e, "function", argv[0]);
        for (int i = 1; i < argc; i++)
            ah_array_push(vm, args, argv[i]);
    }
    ah_set_member(vm, e, "arguments", ah_objv(args));
    *ret = ah_objv(e);
    return 0;
}

/* A key a script pushed at a native bar (MKeyPressEvent): its own key
   handler (bar_input), as the queued event reaches the widget's +0x244 */
static int nat_widget_key(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    menu *m = d;
    *ret = ah_nil();
    ah_obj *o = argc > 1 ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    ui_widget *w = o ? ah_host_data(o) : NULL;
    int k = argc > 1 ? (int)ah_num(argv[1]) : 0;
    menu_input input = k == 0xff0d ? MENU_ACCEPT : k == 0xff1b ? MENU_BACK : k == 0xff51 ? MENU_LEFT
                     : k == 0xff52 ? MENU_UP : k == 0xff53 ? MENU_RIGHT : k == 0xff54 ? MENU_DOWN : -1;
    if (w && (int)input >= 0)
        bar_input(m, w, input);
    return 0;
}
/* photo_shoot.ad L219/1279: preserve event payload and defer callbacks
   through the existing script-event queue. A: general event parity pending. */
static int nat_photo_event(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    int kind = (int)(intptr_t)d;
    if (argc < (kind == 2 ? 2 : 3))
        return ah_error(vm, "photo event: missing arguments");
    ah_obj *event = ah_module_new(vm, "PhotoEvent", NULL, NULL, NULL), *args = ah_array_new(vm);
    ah_set_member(vm, event, "widget", argv[1]);
    ah_set_member(vm, event, "keysym", kind == 2 ? ah_int(0) : argv[2]);
    ah_array_push(vm, args, ah_objv(event));
    if (kind == 2)
        ah_array_push(vm, args, argv[1]);
    {   /* photo_shoot.ad L322/L408: the FocalLen ScaleBar opened and closed
           by a pushed Return, which only its native handler (00238D10) takes */
        ah_obj *o = ah_as_obj(argv[1], AO_MODULE);
        ui_widget *w = o ? ah_host_data(o) : NULL;
        if (!kind && w && w->type && (!strcmp(w->type, "ScaleBar") || !strcmp(w->type, "SliderBar"))) {
            ah_val call[] = {ah_native(vm, "widgetKey", nat_widget_key, ah_user(vm)), argv[1], argv[2]};
            return nat_event_new(vm, NULL, NULL, 3, call, ret);
        }
    }
    ah_val call[] = {argv[0], argv[1],
                     ah_str(vm, kind == 2   ? "onActivate"
                                : kind == 1 ? "onKeyRelease"
                                            : "onKeyPress"),
                     ah_objv(args)};
    return nat_event_new(vm, NULL, (void *)1, 4, call, ret);
}

static int nat_push_event(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    menu *m = d;
    ah_obj *e = argc ? ah_as_obj(argv[0], AO_MODULE) : NULL;
    if (!e ||
        (strcmp(ah_module_name(e), "MScriptEvent") && strcmp(ah_module_name(e), "MFunctionEvent")))
        return ah_error(vm, "unsupported event type");
    if (m->nevents == 128)
        return ah_error(vm, "menu event queue is full");
    ah_pin(vm, ah_objv(e));
    m->events[m->nevents++] = e;
    *ret = ah_nil();
    return 0;
}

static ah_obj *game_object(menu *m)
{
    ah_val main_, game;
    if (!ah_get_member(m->vm, ah_root(m->vm), "main", &main_) ||
        !ah_get_member(m->vm, ah_as_obj(main_, AO_MODULE), "game", &game))
        return NULL;
    return ah_as_obj(game, AO_MODULE);
}

/* App commands that replace the presented sequence rather than open a
   dialog over this one. The destination/loader owns the incoming reveal. */
static int quick_loading_handoff(const char *command)
{
    static const char *const names[]={"COMMAND_START","COMMAND_SPECTATOR",
        "COMMAND_REPLAY","COMMAND_DEMONSTRATION","COMMAND_NEXT_RACE",
        "COMMAND_SETTINGS","COMMAND_LOGGER","COMMAND_OPTION"};
    for(unsigned i=0;i<sizeof names/sizeof *names;i++)
        if(!strcmp(command,names[i]))return 1;
    return 0;
}
static int nat_finish(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    menu *m = d;
    ah_val sequence = ah_nil(), project = ah_nil(), command = ah_nil();
    /* A quick menu's command (start, replay, ...) is the race sequence's:
       the caller acts on it; leaving (COMMAND_EXIT) goes on to the next
       menu project as below */
    if (m->quick_work && ah_get_member(vm, m->quick_work, "selectedCommand", &command) &&
        ah_cstr(command) && *ah_cstr(command) && strcmp(ah_cstr(command), "COMMAND_EXIT")) {
        snprintf(m->quick_command, sizeof m->quick_command, "%s", ah_cstr(command));
        ah_set_member(vm, m->quick_work, "selectedCommand", ah_str(vm, ""));
        if(!quick_loading_handoff(m->quick_command))
            ui_fade(m->ui, 0, 0, 0);
        *ret = ah_nil();
        return 0;
    }
    album_music_store(m);
    if (!strcmp(ah_module_name(m->project), "SlideProject"))
        jukebox_store(m); /* the Jukebox's race playlist kept */
    ah_get_member(vm, game_object(m), "next_sequence", &sequence);
    ah_get_member(vm, game_object(m), "next_menu_project", &project);
    const char *s = ah_cstr(sequence), *p = ah_cstr(project);
    if (!strcmp(ah_module_name(m->project), "SettingProject")) {
        /* HomeRoot L4127..4133 and SettingRoot L843 retain major project/page. */
        ah_val major = ah_nil(), page = ah_nil();
        ah_get_member(vm, game_object(m), "major_menu_project", &major);
        ah_get_member(vm, game_object(m), "major_menu_page", &page);
        const char *origin = ah_cstr(major), *root = ah_cstr(page);
        if (!origin || !*origin)
            return ah_error(vm, "missing setting return project");
        snprintf(m->next_project, sizeof m->next_project, "%s%s%s", origin,
                 root && *root ? ":" : "", root ? root : "");
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(ah_module_name(m->project), "PhotoShootProject")) {
        /* photo_shoot.ad L55, gtmode.ad L13591..13594: return to the
           selecting page, not a new race or the GT Mode root. */
        snprintf(m->next_project, sizeof m->next_project, "gtmode:PhotoTravelRoot");
        *ret = ah_nil();
        return 0;
    }
    if (!strcmp(ah_module_name(m->project), "PhotoSaveProject")) {
        /* photo_save.ad PreviewRoot::onCancel L63 finishes its caller's
           photo sequence. Native cached projects return through origin. */
        ah_val origin = ah_nil();
        ah_get_member(vm, game_object(m), "last_menu_project", &origin);
        const char *caller = ah_cstr(origin);
        if (caller && *caller) {
            snprintf(m->next_project, sizeof m->next_project, "%s", caller);
            *ret = ah_nil();
            return 0;
        }
    }
    if (s && !strcmp(s, "setting")) {
        /* gtmode.ad: GarageRoot calls start_sequence(context,"setting"). */
        snprintf(m->next_project, sizeof m->next_project, "setting");
        *ret = ah_nil();
        return 0;
    }
    if (s && !strcmp(s, "photo_shoot")) {
        /* gtmode.ad L13585..13596: selected course, garage car, capture
           loading mode, then start_sequence(context,"photo_shoot"). */
        snprintf(m->next_project, sizeof m->next_project, "photo_shoot");
        *ret = ah_nil();
        return 0;
    }
    if (s &&
        (!strcmp(s, "single_race") || !strcmp(s, "time_trial") || !strcmp(s, "freerun") ||
         !strcmp(s, "practice") || !strcmp(s, "photo_drive") || !strcmp(s, "machinetest") ||
         !strcmp(s, "solo_event") || !strcmp(s, "championship") || !strcmp(s, "license") ||
         !strcmp(s, "mission") ||
         !strcmp(s,
                 "split_battle"))) { /* gtmode.ad MissionRoot: exit_menu("mission") -> 0011BB60 */
        /* The race itself is the platform's (race view); the menu comes
           back from black when it returns. */
        m->race_requested = 1;
        ui_fade(m->ui, 0, 0, 0);
        *ret = ah_nil();
        return 0;
    }
    if (mp_string(ui_project_tree(m->ui), "name") &&
        !strcmp(mp_string(ui_project_tree(m->ui), "name"), "LaboModeProject") &&
        (!p || !*p || !strcmp(p, "labomode") || replay_action)) {
        /* labomode's page done (start_page(context, nil)): a replay it
           loaded plays (MMemoryCardManager::setAction(1), the caller's
           menu_take_loaded_replay) and comes back here; the save screen
           goes back where it was opened from */
        ah_set_member(vm, game_object(m), "next_sequence", ah_str(vm, "menu"));
        /* a best lap loaded (MODE_LOAD_BESTLAP_REPLAY) is the Time Trial's
           ghost: back where the screen was opened from, the caller taking
           it (menu_take_loaded_ghost) */
        if (replay_action && strncmp(replay_loaded, "bestlap_", 8))
            snprintf(m->next_project, sizeof m->next_project, "labomode");
        else
            snprintf(m->next_project, sizeof m->next_project, "%s",
                     memcard_origin[0] ? memcard_origin : "arcade");
        *ret = ah_nil();
        return 0;
    }
    if (s && !strcmp(s, "photo_labo")) {
        /* gtmode.ad HomeRoot Labo L4202..4206: start_sequence("photo_labo")
           loads labomode with next_menu_arg "" (labomode.ad L37: PhotoRoot);
           its exit goes back to major_menu_project. */
        ah_val major;
        const char *origin = ah_get_member(vm, game_object(m), "major_menu_project", &major) &&
                                     ah_cstr(major) && *ah_cstr(major)
                                 ? ah_cstr(major)
                                 : "gtmode";
        snprintf(memcard_origin, sizeof memcard_origin, "%s", origin);
        ah_set_member(vm, game_object(m), "next_sequence", ah_str(vm, "menu"));
        snprintf(m->labo_arg, sizeof m->labo_arg, "PhotoRoot");
        snprintf(m->next_project, sizeof m->next_project, "labomode");
        *ret = ah_nil();
        return 0;
    }
    if (s && !strcmp(s, "load_replay")) {
        /* the Replay Theater: labomode's ReplayTheaterRoot (menu_open_labo);
           its exit goes back to major_menu_project */
        /* Native projects own separate game objects. Their initial major
           project can still say arcade even when this action is in GT Mode.
           The actual caller is authoritative at this sequence boundary. */
        const char *origin = m->project_label;
        ah_set_member(vm,game_object(m),"major_menu_project",ah_str(vm,origin));
        snprintf(memcard_origin, sizeof memcard_origin, "%s", origin);
        ah_set_member(vm, game_object(m), "next_sequence", ah_str(vm, "menu"));
        ah_set_member(vm, game_object(m), "last_sequence", ah_str(vm, "load_replay"));
        snprintf(m->labo_arg, sizeof m->labo_arg, "ReplayTheaterRoot");
        snprintf(m->next_project, sizeof m->next_project, "labomode");
        *ret = ah_nil();
        return 0;
    }
    if (attract_left && !strcmp(ah_module_name(m->project), "BootProject") &&
        !strcmp(ui_root_name(m->ui, menu_active_root(m)), "MovieRoot")) {
        menu_attract_finish(m, attract_cancelled);
        *ret = ah_nil();
        return 0;
    }
    if (s && !strcmp(s, "autodemo")) {
        /* 004345D8 +0x2C/+0x48, 0015DB40 +0x34: four presentations;
           the first cycle starts interval-1 before its opening movie. */
        if (!attract_origin[0])
            demo_movie_count = (int)menu_option_number(m, "demo_movie_interval", 3) - 1;
        snprintf(attract_origin, sizeof attract_origin, "%s:%s", m->project_label,
                 ui_root_name(m->ui, menu_active_root(m)));
        attract_left = 4;
        attract_cancelled = 0;
        attract_next(m);
        *ret = ah_nil();
        return 0;
    }
    if (s && *s && strcmp(s, "menu"))
        return ah_error(vm, "sequence '%s' is not implemented", s);
    if (!p || !*p || strlen(p) >= sizeof m->next_project)
        return ah_error(vm, "missing next menu project");
    if (!strcmp(ah_module_name(m->project), "OptionProject")) {
        ah_val page = ah_nil();
        if (ah_get_member(vm, game_object(m), "major_menu_page", &page) && ah_cstr(page) &&
            *ah_cstr(page))
            fprintf(stderr, "options: return to %s:%s\n", p, ah_cstr(page));
        else
            fprintf(stderr, "options: return to %s\n", p);
    }
    snprintf(m->next_project, sizeof m->next_project, "%s", p);
    if (!strcmp(
            p,
            "slide")) { /* slide.ad onLoad L25..28: next_menu_arg is its first page (MusicRoot from the GT Mode's MusicTheater) */
        ah_val arg = ah_nil();
        ah_get_member(vm, game_object(m), "next_menu_arg", &arg);
        if (ah_cstr(arg) && *ah_cstr(arg) &&
            strlen(p) + 1 + strlen(ah_cstr(arg)) < sizeof m->next_project)
            snprintf(m->next_project, sizeof m->next_project, "%s:%s", p, ah_cstr(arg));
    }
    *ret = ah_nil();
    return 0;
}

static int nat_unavailable(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)argc;
    (void)argv;
    (void)ret;
    return ah_error(vm, "%s is not implemented", (const char *)d);
}

void menu_set_dialog_presenter(menu *m, int (*fn)(menu *, const char *, const char *, int,
                                                  const char *, const char *))
{
    if (m)
        m->present_dialog = fn;
}
void menu_set_progress_presenter(menu *m, void (*fn)(menu *, int, const char *, int, float))
{
    if (m)
        m->present_progress = fn;
}

static int nat_dialog_open(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)d;
    /* ConfirmDialog.open(context, type, message, title, ..., yes, no). */
    if (argc < 3)
        return ah_error(vm, "ConfirmDialog.open: missing message");
    const char *message = ah_cstr(argv[2]), *title = argc > 3 ? ah_cstr(argv[3]) : NULL;
    menu *m = d;
    title = ui_storage_text(title);
    message = ui_storage_text(message);
    const char *yes = argc > 5 ? ah_cstr(argv[5]) : NULL, *no = argc > 6 ? ah_cstr(argv[6]) : NULL;
    int result = m && m->present_dialog
                     ? m->present_dialog(m, title,
                                         message ? message : "", (int)ah_num(argv[1]), yes, no)
                     : plat_dialog(title && *title ? title : "Info", message ? message : "",
                                   ah_num(argv[1]) != 0);
    if (result < 0)
        return ah_error(vm, "could not display confirmation dialog");
    *ret = ah_int(result);
    return 0;
}

static int nat_cursor_icon(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)data;
    menu *m = ah_user(vm);
    ui_cursor_icon(m->ui, argc > 1 ? ah_cstr(argv[1]) : NULL);
    *ret = ah_nil();
    return 0;
}

/* cursor.ad's ProgressDialog (open(context, title, callback): the message,
   the OK button when there is a callback, pushPage; setProgress(p);
   close(context): closePage), shown by the platform over the page */
static int nat_progress_dialog(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                               ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    const char *op = d;
    *ret = ah_nil();
    if (!m || !m->present_progress)
        return 0;
    if (!strcmp(op, "setRawMode"))
        return 0; /* logger.ad: A, the dialog's raw mode not read */
    if (!strcmp(op, "open"))
        m->present_progress(m, 1, argc > 1 && ah_cstr(argv[1]) ? ah_cstr(argv[1]) : "",
                            argc > 2 && argv[2].t != AV_NIL, -1);
    else if (!strcmp(op, "close"))
        m->present_progress(m, 0, NULL, 0, -1);
    else { /* setProgress: a number, or the manager's progress (its value) */
        ah_val v = argc ? argv[argc - 1] : ah_nil(), inner;
        if (v.t == AV_OBJ && ah_get_member(vm, ah_as_obj(v, AO_MODULE), "value", &inner))
            v = inner;
        m->present_progress(m, 2, NULL, 0, ah_is_num(v) ? (float)ah_num(v) : -1);
    }
    return 0;
}

static int nat_common_page(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)d;
    const char *name = argc ? ah_cstr(argv[0]) : NULL;
    if (name && !strcmp(name, "CursorRoot")) {
        ah_obj *cursor = ah_module_new(vm, "CursorRoot", NULL, NULL, NULL);
        ah_set_member(vm, cursor, "setIcon", ah_native(vm, "setIcon", nat_cursor_icon, NULL));
        *ret = ah_objv(cursor);
        return 0;
    }
    if (name && !strcmp(name, "ConfirmDialog")) {
        ah_obj *dialog = ah_module_new(vm, "ConfirmDialog", NULL, NULL, NULL);
        ah_set_member(vm, dialog, "open", ah_native(vm, "open", nat_dialog_open, d));
        *ret = ah_objv(dialog);
        return 0;
    }
    if (name && !strcmp(name, "ProgressDialog")) {
        ah_obj *dialog = ah_module_new(vm, "ProgressDialog", NULL, NULL, NULL);
        static const char *const ops[] = {"open", "close", "setProgress", "setRawMode"};
        for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++)
            ah_set_member(vm, dialog, ops[i],
                          ah_native(vm, ops[i], nat_progress_dialog, (void *)ops[i]));
        *ret = ah_objv(dialog);
        return 0;
    }
    return ah_error(vm, "common page '%s' is not implemented", name ? name : "?");
}
