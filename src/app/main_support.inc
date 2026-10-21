#include "dialog_style.h"
static const char *projects[] = {
    "arcade",       "gtmode",        "boot",          "option",
    "setting",      "event",         "quick",         "save",
    "quick-arcade", "quick-tt",      "quick-event",   "quick-championship",
    "labomode",     "quick-license", "photo_shoot",   "quick-mission",
    "logger",       "slide",         "quick-freerun", "quick-practice",
    "quick-mt",     "photo_save"};
static photo_travel *presentation_photo;
static int photo_project(int i)
{
    return i >= 0 && i < (int)(sizeof projects / sizeof projects[0]) &&
           !strcmp(projects[i], "photo_shoot");
}
/* Arcade and Time Trial are distinct authored projects (arcade.ad sequences,
   quick-arcade.mproject ArcadeRoot / quick-tt.mproject TimeTrialRoot). */
static int race_quick_project(int project)
{
    return project == 8 || project == 9 || project == 10 || project == 11 || project == 13 ||
           project == 15 || project == 18 || project == 19 || project == 20;
}
static int
    free_mode; /* the race's GT Mode Free Run (1), Practice (2), Photo Drive (3) or Power & Speed (4): their quick menus */
static const char *race_quick_route(int time_trial)
{
    return (free_mode == 1 || free_mode == 3) ? "quick-freerun:FreeRunRoot"
           : free_mode == 2                   ? "quick-practice:PracticeRoot"
           : free_mode == 4                   ? "quick-mt:QuickRoot"
           : time_trial                       ? "quick-tt:TimeTrialRoot"
                                              : "quick-arcade:ArcadeRoot";
}

#define NPROJECTS (int)(sizeof projects / sizeof projects[0])
static struct {
    int active, rng_ready, result_applied, has_distances, championship, retired;
    int mission; /* a driving mission (0011BB60): the game's car, quick-mission */
    unsigned round, rounds;
    int32_t points[6];
    uint32_t grand_prize;
    char series[32];
    gt4_race_desc championship_grid;
    uint8_t bodies[6][0x4b8];
    uint32_t rng, award, a_distance, b_distance;
    uint8_t best, row[151];
    char race[32];
} career_race;
static struct {
    int active, result_applied;
    char code[32];
    gt4_career_license_attempts visit;
    uint32_t best_time;
    uint32_t last_time; /* the last attempt's time (race+0x8C before the menu clears it) */
} career_license;

static uint64_t presentation_clock;
static video_settings *app_video_settings;
static save_ui save_screen;
static int present_profile_picker(menu *m);
static gt4_replay_file replay_to_save; /* the race the memory card's save screen writes */
static ui_project *dialog_ui;
static const char *dialog_shot;
static int dialog_shot_scripted;
static const char *loading_shot;
static void screenshot(const char *path);
static int present_dialog(menu *, const char *, const char *, int, const char *, const char *);
static int present_event(menu *, void *);
static char *scripted_keys;
static float scripted_interval;
static showroom *presentation_cars;
static showroom *split_selection_cars[2];
static const char *presentation_disc;
static race_game *presentation_race;
static int presentation_pregrid;
static uint64_t pregrid_clock;
static int draw_pregrid_face(void *user, float x, float y, float w, float h, float opacity)
{
    (void)user;
    if (!presentation_pregrid || x != 0 || y != 0 || w != 640 || h != 480)
        return 0;
    uint64_t now = plat_time_ns();
    race_game_pregrid_update(presentation_race,
                             pregrid_clock ? (now - pregrid_clock) / 1e9 : 1.0 / 60);
    pregrid_clock = now;
    video_begin_scene();
    int ready = race_game_draw_pregrid(presentation_race);
    video_end_scene();
    if (ready && opacity < 1) {
        uint8_t black[4] = {0, 0, 0, (uint8_t)(255 * (1 - opacity))};
        plat_draw_quad(NULL, 0, 0, 640, 480, 0, 0, 1, 1, black);
    }
    return ready;
}
/* Draws the persistent movie layer under pages without their own movie. */
static void (*presentation_movie_layer)(menu *m);

/* ModelFace assets belong to UI packages, independent of race/showroom state. */
typedef struct {
    r3d_mesh *gpu;
    float ground_pivot[3];
} ui_mesh;
static void *ui_mesh_create(const uint8_t *bytes, size_t size)
{
    gt4_model *model = gt4_model_decode_part(bytes, size, 0, 0);
    if (!model)
        return NULL;
    /* Static ModelFace conversion does not execute the original VU anchor
       adjustment. Keep an otherwise detached ground-shadow quad under its
       model. Preserve the authored quad size and small directional offsets. */
    float base_lo[3] = {1e9f, 1e9f, 1e9f}, base_hi[3] = {-1e9f, -1e9f, -1e9f};
    float body_lo[3] = {1e9f, 1e9f, 1e9f}, body_hi[3] = {-1e9f, -1e9f, -1e9f};
    int base_count = 0, body_count = 0;
    for (int i = 0; i < model->count; i++) {
        gt4_triangle *t = &model->triangles[i];
        int ground = fabsf(t->v[0].p[1]) < 1e-5f && fabsf(t->v[1].p[1]) < 1e-5f &&
                     fabsf(t->v[2].p[1]) < 1e-5f;
        float *lo = ground ? base_lo : body_lo, *hi = ground ? base_hi : body_hi;
        if (ground)
            base_count++;
        else
            body_count++;
        for (int v = 0; v < 3; v++)
            for (int k = 0; k < 3; k++) {
                lo[k] = fminf(lo[k], t->v[v].p[k]);
                hi[k] = fmaxf(hi[k], t->v[v].p[k]);
            }
    }
    if (base_count == 2 && body_count > 0) {
        float shift[3] = {0};
        for (int k = 0; k < 3; k += 2) {
            float delta = (base_lo[k] + base_hi[k] - body_lo[k] - body_hi[k]) * .5f;
            if (fabsf(delta) > (body_hi[k] - body_lo[k]) * .25f)
                shift[k] = delta;
        }
        for (int i = 0; i < model->count; i++) {
            gt4_triangle *t = &model->triangles[i];
            if (fabsf(t->v[0].p[1]) < 1e-5f && fabsf(t->v[1].p[1]) < 1e-5f &&
                fabsf(t->v[2].p[1]) < 1e-5f)
                for (int v = 0; v < 3; v++)
                    for (int k = 0; k < 3; k += 2)
                        t->v[v].p[k] -= shift[k];
        }
    }
    ui_mesh *mesh = calloc(1, sizeof *mesh);
    if (mesh) {
        mesh->gpu = r3d_mesh_create(model);
        if (body_count > 0)
            for (int k = 0; k < 3; k += 2)
                mesh->ground_pivot[k] = (body_lo[k] + body_hi[k]) * .5f;
        if (!mesh->gpu) {
            free(mesh);
            mesh = NULL;
        }
    }
    gt4_model_free(model);
    return mesh;
}
static void ui_mesh_destroy(void *opaque)
{
    ui_mesh *mesh = opaque;
    if (mesh) {
        r3d_mesh_destroy(mesh->gpu);
        free(mesh);
    }
}
static void ui_mesh_draw(void *mesh, const mp_node *node, const float angles[3], float x, float y,
                         float w, float h, const tex1_image *environment)
{
    if (w <= 0 || h <= 0)
        return;
    ui_mesh *icon = mesh;
    double camera[3] = {0, 7.2, 14}, tilt[3] = {-25, 0, 0};
    mp_numbers(node, "camera_trans", camera, 3);
    mp_numbers(node, "camera_rotate", tilt, 3);
    const float rad = 0.01745329252f;
    float a = (float)angles[0] * rad, b = (float)angles[1] * rad, c = (float)angles[2] * rad;
    float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b), cc = cosf(c), sc = sinf(c);
    r3d_view v = {0};
    /* ModelFace's authored composition: Rz * Rx * Ry. The tilt stays
       fixed while heading turns in model space. */
    float rotation[9] = {cc * cb - sc * sa * sb,
                         -sc * ca,
                         cc * sb + sc * sa * cb,
                         sc * cb + cc * sa * sb,
                         cc * ca,
                         sc * sb - cc * sa * cb,
                         -ca * sb,
                         sa,
                         ca * cb};
    memcpy(v.rot, rotation, sizeof rotation);
    for (int i = 0; i < 3; i++)
        v.cam[i] = (float)camera[i];
    float pitch = (float)tilt[0] * rad, yaw = (float)tilt[1] * rad;
    v.r[0] = cosf(yaw);
    v.r[2] = -sinf(yaw);
    v.u[0] = sinf(yaw) * sinf(pitch);
    v.u[1] = cosf(pitch);
    v.u[2] = cosf(yaw) * sinf(pitch);
    v.f[0] = -sinf(yaw) * cosf(pitch);
    v.f[1] = sinf(pitch);
    v.f[2] = -cosf(yaw) * cosf(pitch);
    float fov = (float)mp_number(node, "view_angle", 30);
    if (fov < 1 || fov > 170)
        fov = 30;
    /* 00217058 +0x2B0..0x3A8: the widget's rectangle in the framebuffer
       (00215FE0: x * 640/640, y * 448/480, D_006184D0 mode 0 640x448),
       the projection centred on it with one focal length for both axes,
       half its height over tan(view_angle / 2) (004A59B8). Back in the
       640x480 units: x * 0.5 * h * 448/480 / tan, y * 0.5 * h / tan.
       The viewport is the rectangle grown k times about its centre, so
       the model may leave it as the PS2's full-screen frustum lets it. */
    const float k = 2;
    float t = tanf(fov * rad * .5f);
    v.focal = 320 * .5f * h * (448.f / 480.f) / (k * w * t);
    v.focal_y = 224 * .5f / (k * t);
    /* the Dutch flag car is the size of the others (2.38 x 0.71 x 1.19
       against 2.69 x 0.71 x 1.14): no framing of its own */
    const char *model_path = mp_string(node, "model_set_path");
    if (model_path && !strcmp(model_path, "image/gtmode/map/car_old_timer.mdl")) {
        /* This mesh's body is offset from the set origin (~0.498 on Z).
           Turn about its own ground center, retaining its authored rest
           placement. Other map assets keep their original origin. */
        double rest[3] = {0};
        mp_numbers(node, "model_rotate", rest, 3);
        float ra = (float)rest[0] * rad, rb = (float)rest[1] * rad, rc = (float)rest[2] * rad;
        float xa = cosf(ra), xs = sinf(ra), ya = cosf(rb), ys = sinf(rb), za = cosf(rc),
              zs = sinf(rc);
        float rest_matrix[9] = {za * ya - zs * xs * ys,
                                -zs * xa,
                                za * ys + zs * xs * ya,
                                zs * ya + za * xs * ys,
                                za * xa,
                                zs * ys - za * xs * ya,
                                -xa * ys,
                                xs,
                                xa * ya};
        ui_model_pivot_offset(v.rot, rest_matrix, icon->ground_pivot, v.pos);
        if (getenv("MOON_ICON_PIVOT_TRACE")) {
            float center[3];
            for (int i = 0; i < 3; i++) {
                center[i] = v.pos[i];
                for (int k = 0; k < 3; k++)
                    center[i] += v.rot[i * 3 + k] * icon->ground_pivot[k];
            }
            fprintf(stderr, "historic pivot: yaw=%.3f center=%.6f,%.6f,%.6f\n", angles[1],
                    center[0], center[1], center[2]);
        }
    }
    r3d_ui_light light = {0};
    double direction[4] = {0, .629198, .777245, 0}, ambient[4] = {50, 50, 50, 255};
    double diffuse[4] = {255, 255, 255, 255}, specular[4] = {127, 127, 127, 255};
    mp_numbers(node, "l0_pos", direction, 4);
    mp_numbers(node, "l0_amb", ambient, 4);
    mp_numbers(node, "l0_dif", diffuse, 4);
    mp_numbers(node, "l0_spe", specular, 4);
    for (int i = 0; i < 3; i++) {
        light.direction[i] = (float)direction[i];
        light.ambient[i] = (float)ambient[i] / 255;
        light.diffuse[i] = (float)diffuse[i] / 255;
        light.specular[i] = (float)specular[i] / 255;
    }
    /* quick-event.mproject PrizeRoot.model supplies two lights and env0;
       the former one-light/no-environment path lost the trophy's metal. */
    r3d_lights lights = {1};
    memcpy(lights.direction[0], light.direction, sizeof light.direction);
    memcpy(lights.ambient[0], light.ambient, sizeof light.ambient);
    memcpy(lights.diffuse[0], light.diffuse, sizeof light.diffuse);
    memcpy(lights.specular[0], light.specular, sizeof light.specular);
    for (int l = 1; l < 4; l++) {
        char key[32];
        snprintf(key, sizeof key, "l%d_enable", l);
        if (!mp_number(node, key, 0))
            continue;
        int dst = lights.count++;
        double values[4] = {0};
        const char *suffix[] = {"pos", "amb", "dif", "spe"};
        float *out[] = {lights.direction[dst], lights.ambient[dst], lights.diffuse[dst],
                        lights.specular[dst]};
        for (int prop = 0; prop < 4; prop++) {
            memset(values, 0, sizeof values);
            snprintf(key, sizeof key, "l%d_%s", l, suffix[prop]);
            mp_numbers(node, key, values, 4);
            for (int axis = 0; axis < 3; axis++)
                out[prop][axis] = (float)values[axis] / (prop ? 255.f : 1.f);
        }
    }
    r3d_begin_ui_environment(x + w * .5f - k * w, y + h * .5f - k * h, 2 * k * w, 2 * k * h,
                             environment);
    r3d_draw_lit(icon->gpu, &v, &lights);
    r3d_end();
}

static int is_popup(const char *name);
static void movie_layer(menu *m);
static void dialog_background(menu *m);
/* Is page `name` on top, or right under a popup on top (SettingPopup over
   CarRoot keeps the showroom visible)? */
static int page_below_popup(menu *m, const char *name)
{
    ui_project *p = menu_ui(m);
    int depth = menu_page_depth(m);
    const char *top = ui_root_name(p, menu_active_root(m));
    if (top && !strcmp(top, name))
        return 1;
    return depth > 1 && is_popup(top) && !strcmp(ui_root_name(p, menu_page_at(m, depth - 2)), name);
}
/* A popup page (SettingPopup, the confirm dialogs) is pushed over the page
   it belongs to and shows it underneath; other pushed pages (GameRoot over
   TopRoot) show only the movie layer below them (movie_layer). */
static int is_popup(const char *name)
{
    return name && (strstr(name, "Popup") || strstr(name, "Dialog"));
}
/* cursor.ad's ProgressDialog over the page while a script holds it open
   (labomode's "Now loading" around a card read) */
static void draw_pages(menu *m, int *drawn, int *skipped);
static int progress_root = -1, progress_open;
static void present_progress(menu *m, int state, const char *message, int button, float progress)
{
    (void)m;
    if (!dialog_ui)
        return;
    if (progress_root < 0)
        for (int i = 0; i < ui_root_count(dialog_ui); i++)
            if (!strcmp(ui_root_name(dialog_ui, i), "ProgressDialog"))
                progress_root = i;
    if (progress_root < 0)
        return;
    ui_widget *page = ui_root_widget(dialog_ui, progress_root),
              *pane = ui_widget_child(page, "Dialog");
    ui_widget *bar = ui_widget_child(ui_widget_child(pane, "Progress"), "Bar");
    ui_widget *fill = ui_widget_child(bar, "progress"), *bg = ui_widget_child(bar, "bg");
    if (state == 1) {
        if (pane)
            pane->y = 120; /* open(): Dialog.y = 120 */
        ui_widget *text = ui_widget_child(ui_widget_child(pane, "Message"), "message");
        if (text) {
            free(text->text);
            text->text = strdup(message ? message : "");
        }
        ui_widget *ok = ui_widget_child(ui_widget_child(pane, "Button"), "OK");
        if (ok)
            ok->visible = button != 0;
        if (fill)
            fill->progress = 1; /* the PC reads the file at once: the bar full (P) */
        progress_open = 1;
    } else if (state == 0) {
        if (progress_open && getenv("MOON_PROGRESS_SHOT")) { /* tests: the dialog as it closes */
            video_set_menu_pacing(1);
            video_set_menu_projection(1);
            plat_video_begin_frame();
            draw_pages(m, NULL, NULL);
            screenshot(getenv("MOON_PROGRESS_SHOT"));
            plat_video_end_frame();
        }
        progress_open = 0;
    } else if (fill && progress >= 0)
        fill->progress = fminf(1, progress); /* ProgressFace draws its fraction */
}

/* The green touch Exit belongs to menus. The boot project's splash and its
   opening MovieRoot have no authored back control, so it must stay away
   there just as it already does during a race. */
static int touch_overlay_hidden(menu *m, const char **phase)
{
    static const char *logged;
    ui_project *p = menu_ui(m);
    const char *project = p ? mp_string(ui_project_tree(p), "name") : NULL;
    const char *page = p ? ui_root_name(p, menu_active_root(m)) : NULL;
    const char *state = NULL;
    if (project && !strcmp(project, "BootProject") && page) {
        if (!strcmp(page, "LoadRoot"))
            state = "splash";
        else if (!strcmp(page, "MovieRoot"))
            state = "cutscene";
    }
    if (phase)
        *phase = state;
    if (!state) {
        logged = NULL;
        return 0;
    }
    if (!logged || strcmp(logged, state)) {
        fprintf(stderr, "touch overlay: hidden (%s)\n", state);
        logged = state;
    }
    return 1;
}

static void draw_pages(menu *m, int *drawn, int *skipped)
{
    ui_project *p = menu_ui(m);
#ifndef __ANDROID__
    /* Android's touch Exit is hidden in these phases below; the phase
       diagnostic is also useful from the desktop and console builds. */
    (void)touch_overlay_hidden(m, NULL);
#endif
    ui_set_scene_renderer(p, draw_pregrid_face, NULL);
    int depth = menu_page_depth(m);
    if (depth > 1 && is_popup(ui_root_name(p, menu_active_root(m))))
        ui_draw_under(p, menu_page_at(m, depth - 2));
    else
        movie_layer(m);
    ui_draw_root(p, menu_active_root(m), drawn, skipped);
    if (progress_open && dialog_ui && progress_root >= 0)
        ui_draw_root(dialog_ui, progress_root, NULL, NULL);
#if defined(__SWITCH__) || defined(__ANDROID__)
    else if (app_video_settings && dialog_ui) {
        uint64_t now = plat_time_ns();
        float dt = app_video_settings->touch_back_clock
                       ? (float)((now - app_video_settings->touch_back_clock) / 1e9)
                       : 1.f / 60;
        app_video_settings->touch_back_clock = now;
        plat_video_set_view(640, 480);
#if defined(__ANDROID__)
        if (touch_overlay_hidden(m, NULL) || ui_has_back_control(p, menu_active_root(m))) {
#else
        if (ui_has_back_control(p, menu_active_root(m))) {
#endif
            ui_hide_touch_back(dialog_ui);
            app_video_settings->touch_back.tap = (touch_dialog_state){0};
        } else
            ui_draw_touch_back(dialog_ui,
                               app_video_settings->current.touch_menu && ui_touch_active(p),
                               app_video_settings->touch_back.tap.selected != NULL, dt);
    }
#endif
}

/* GT Mode's CarFaces (dealers, garage, car wash...), drawn in place by the
   UI: action_type "normal" puts just the car over the page with the
   widget's own camera and lights (l0..l2 and world_ambient, as ModelFace
   reads them). An explicit CarModel pose also uses the authored frame;
   the remaining advanced showroom path uses the studio. */
static void draw_car_face(void *user, const ui_widget *w, float x, float y, float wd, float h,
                          float opacity)
{
    (void)opacity;
    menu *m = user;
    showroom *cars = presentation_cars;
    int slot = 0;
    /* arcade.ad SplitRoot.set_car/move_car: independent P1/P2 CarFaces,
       each with its own resident model, paint and animation clock. */
    if (!strcmp(ui_root_name(menu_ui(m), w->root), "SplitRoot"))
        for (const ui_widget *p = w->parent; p; p = p->parent) {
            int port = p->name && !strcmp(p->name, "P1")   ? 0
                       : p->name && !strcmp(p->name, "P2") ? 1
                                                           : -1;
            if (port < 0)
                continue;
            if (!split_selection_cars[port] && w->car_code)
                split_selection_cars[port] = showroom_open(presentation_disc);
            cars = split_selection_cars[port];
            slot = port + 1;
            break;
        }
    static uint64_t last[3];
    uint64_t now = plat_time_ns();
    float dt = last[slot] ? (float)((now - last[slot]) / 1e9) : 0;
    last[slot] = now;
    if (!cars || !w->car_code)
        return;
    showroom_set_wheel(cars, w->car_wheel);
    showroom_set_wing(cars, w->car_wing);
    showroom_set_selection(cars, w->car_code, w->car_color_index);
    if (!w->car_normal &&
        (!w->car_pose_set || !strcmp(ui_root_name(menu_ui(m), w->root), "BuyCarRoot"))) {
        /* the studio fills the view: its own scene target (MSAA, FSR) */
        video_begin_scene();
        SWP_ASSET_DRAW(showroom_draw(cars, w->car_code, dt));
        video_end_scene();
    } else {
        /* over the page as drawn so far: straight into the frame (the scene
           target is cleared opaque and would cover the page's background) */
        showroom_face f;
        memset(&f, 0, sizeof f);
        f.x = x;
        f.y = y;
        f.w = wd;
        f.h = h;
        f.surface = ui_root_name(menu_ui(m), w->root); /* the camera log names this surface */
        f.crew_data = w->car_crew_data;
        f.crew_size = w->car_crew_size;
        f.crew_serial = w->car_crew_serial;
        f.crew_motion = w->car_crew_motion;
        f.crew_time = w->car_crew_time;
        f.crew_loop = w->car_crew_loop;
        f.models = w->car_models;
        f.model_count = w->car_model_count;
        f.motions = w->car_motions;
        f.motion_count = w->car_motion_count;
        memcpy(f.service_geometry, w->car_service_geometry, sizeof f.service_geometry);
        f.service_geometry_valid = w->car_service_geometry_valid;
        f.additional_scale = w->car_additional_scale;
        /* Original: BuyCarRoot.CarFace.move_car instructions 18..76.
           Dealers use the same authored pose bridge as other CarFaces. */
        f.pose_valid = w->car_pose_set;
        f.service_opacity = w->car_model_opacity;
        /* func_00157378: advanced owner copies x/y, negates z and adds
           exactly 180 degrees to the pose yaw before scene projection. */
        float yaw = w->model_angles[1];
        if (!w->car_normal)
            yaw = yaw + 180.0f;
        f.car_angle = yaw * .017453292519943295f;
        memcpy(f.car_position, w->car_position, sizeof f.car_position);
        if (!w->car_normal)
            f.car_position[2] = -f.car_position[2];
        double trans[3] = {0, .98, 9}, rot[3] = {-3, 0, 0}, angle[1] = {25};
        mp_numbers(w->src, "camera_trans", trans, 3);
        mp_numbers(w->src, "camera_rotate", rot, 3);
        mp_numbers(w->src, "view_angle", angle, 1);
        for (int i = 0; i < 3; i++) {
            f.eye[i] = (float)trans[i];
            f.rotate[i] = (float)rot[i];
        }
        f.view_angle = (float)angle[0];
        if (w->car_camera) {
            float fov = 0;
            float time = w->car_camera_loop ? gt4_mot1_wrap(w->car_camera, w->car_camera_time)
                                            : w->car_camera_time;
            f.look_valid = gt4_mot1_camera(w->car_camera, time, f.look, &fov);
            if (f.look_valid)
                f.view_angle = fov * 57.29577951308232f;
            /* 001570A4..001570D8: the service camera also drives body lift. */
            gt4_mot1_value(w->car_camera, "lift", time, &f.service_lift);
        }
        /* LightSet ctor 00210830 +0x9c..0x12c: world RGB 0,
           position (0,1,0,0), ambient 0x3e999999, diffuse/specular
           0.5; only light 0 enabled. Preserve normalized defaults before
           overriding them with authored byte-valued attributes. */
        double world[4], default_pos[4], default_amb[4], default_dif[4], default_spe[4];
        ui_light_defaults(world, default_pos, default_amb, default_dif, default_spe);
        mp_numbers(w->src, "world_ambient", world, 4);
        for (int l = 0; l < 3; l++) {
            char key[16];
            double on[1] = {l == 0}, pos[4], amb[4], dif[4], spe[4];
            memcpy(pos, default_pos, sizeof pos);
            memcpy(amb, default_amb, sizeof amb);
            memcpy(dif, default_dif, sizeof dif);
            memcpy(spe, default_spe, sizeof spe);
            if (l) {
                snprintf(key, sizeof key, "l%d_enable", l);
                mp_numbers(w->src, key, on, 1);
            }
            if (!on[0])
                continue;
            snprintf(key, sizeof key, "l%d_pos", l);
            mp_numbers(w->src, key, pos, 4);
            snprintf(key, sizeof key, "l%d_amb", l);
            mp_numbers(w->src, key, amb, 4);
            snprintf(key, sizeof key, "l%d_dif", l);
            mp_numbers(w->src, key, dif, 4);
            snprintf(key, sizeof key, "l%d_spe", l);
            mp_numbers(w->src, key, spe, 4);
            int n = f.lights.count++;
            for (int k = 0; k < 3; k++) {
                f.lights.direction[n][k] = (float)pos[k];
                f.lights.ambient[n][k] = (float)(amb[k] + (n == 0 ? world[k] : 0)) / 255;
                f.lights.diffuse[n][k] = (float)dif[k] / 255;
                f.lights.specular[n][k] = (float)spe[k] / 255;
            }
        }
        f.lights.specular_scale = .18f; /* as the arcade CarFace path */
        if (w->car_camera && w->car_motion_count) {
            showroom_service_channels(&f, w->car_camera, w->car_camera_time);
            f.service_opacity *= w->car_model_opacity; /* 00217058: CarModel+4C. */
        }
        SWP_ASSET_DRAW(showroom_draw_face(cars, w->car_code, dt, &f));
    }
}

static void draw_arcade_car_face(void *user, const ui_widget *w, float x, float y, float wd,
                                 float h, float opacity)
{
    menu *m = user;
    /* CarRoot's full showroom is already drawn by the main scene path.
       SplitRoot's two authored normal CarFaces draw inside their panels. */
    if (!strcmp(ui_root_name(menu_ui(m), w->root), "SplitRoot"))
        draw_car_face(user, w, x, y, wd, h, opacity);
}

/* Script sync waits keep presenting actors, without re-entering the VM. */
static int music_faded;
static void fade_menu_music(float seconds)
{
    music_faded = 1;
    plat_audio_fade_music(seconds);
}

/* While the loading overlay reveals the destination, its authored fade and
   widget actors must advance too. Keep the cursor at the outgoing selection. */
static void animate_loading_destination(menu *m, float dt)
{
    ui_project *p = menu_ui(m);
    int paused = ui_cursor_pause(p, 1);
    menu_animate(m, dt > 1.0f / 30.0f ? 1.0f / 30.0f : dt);
    ui_cursor_pause(p, paused);
}

static void update_present_pcm(menu *m);
static void present_wait(menu *m, float seconds, void *user)
{
    (void)user;
    /* Loading readiness follows the original fade actors, independently of
       asset-read latency; no additional fixed sleep is needed. */
    uint64_t start = plat_time_ns();
    int trace = getenv("MOON_PRESENT_TRACE") && seconds >= 1 && menu_active_root(m) >= 0 &&
                !strcmp(ui_root_name(menu_ui(m), menu_active_root(m)), "PresentRoot");
    static unsigned ceremony_wait;
    unsigned trace_id = trace ? ++ceremony_wait : 0;
    int captured = 0;
    if (trace) {
        printf("award Sync %u begin %.3f loading=%d\n", trace_id, seconds, loading_screen_active());
        fflush(stdout);
    }
    if (!presentation_clock)
        presentation_clock = start;
    /* Sync(1/60) is one field on the PS2. Wait to a running deadline, not
       from now: with vsync a frame lasts almost exactly 1/60 s, and timing
       from each call's start made every other Sync present two frames
       (17/33 ms, the Tune Shop dialog's entrance stuttering). A stale
       deadline (no Sync for a while) restarts from now. */
    static uint64_t deadline;
    uint64_t span = (uint64_t)(seconds * 1e9);
    if (!deadline || start > deadline + UINT64_C(50000000) ||
        deadline > start + UINT64_C(1000000000))
        deadline = start;
    deadline += span;
    const uint64_t slack = UINT64_C(2000000);
    do {
        SDL_PumpEvents();
        if (SDL_HasEvent(SDL_QUIT)) {
            ui_fade(menu_ui(m), 0, 0, 0);
            break;
        }
        uint64_t now = plat_time_ns();
        float dt = (float)fmin((now - presentation_clock) / 1e9, 1.0 / 30.0);
        if (loading_screen_revealing())
            animate_loading_destination(m, dt);
        else if (!loading_screen_active())
            menu_animate(m, dt);
        presentation_clock = now;
        update_present_pcm(m);
        video_set_menu_pacing(1);
        video_set_menu_projection(1);
        plat_video_begin_frame();
        if (loading_screen_active()) {
            if (loading_screen_revealing())
                draw_pages(m, NULL, NULL);
            loading_screen_draw();
            if (loading_shot && loading_screen_ready()) {
                screenshot(loading_shot);
                loading_shot = NULL;
            }
            plat_video_end_frame();
            plat_sleep_ms(1);
            continue;
        }
        if (page_below_popup(m, "CarRoot")) {
            video_begin_scene();
            showroom_set_wheel(presentation_cars, NULL); /* Arcade's cars are stock */
            showroom_set_wing(presentation_cars, NULL);
            showroom_draw(presentation_cars, menu_selected_car(m), dt);
            showroom_set_color(presentation_cars, menu_car_color(m));
            video_end_scene();
        }
        draw_pages(m, NULL, NULL);
        if (trace && !captured && (plat_time_ns() - start) / 1e9 > .1) {
            const char *prefix = getenv("MOON_PRESENT_CAPTURE");
            if (prefix) {
                char path[512];
                snprintf(path, sizeof path, "%s-%u.png", prefix, trace_id);
                screenshot(path);
            }
            captured = 1;
        }
        if (trace && captured == 1 && (plat_time_ns() - start) / 1e9 > .75) {
            const char *prefix = getenv("MOON_PRESENT_CAPTURE");
            if (prefix) {
                char path[512];
                snprintf(path, sizeof path, "%s-%u-late.png", prefix, trace_id);
                screenshot(path);
            }
            captured = 2;
        }
        plat_video_end_frame();
        plat_sleep_ms(1);
    } while (plat_time_ns() + slack < deadline ||
             (loading_screen_active() && !loading_screen_revealing() && !loading_screen_ready()));
    if (trace) {
        printf("award Sync %u end\n", trace_id);
        fflush(stdout);
    }
}

static int load_car_face(menu *m, const ui_widget *w, void *user)
{
    (void)user;
    if (!presentation_cars)
        return 0;
    uint64_t start = plat_time_ns();
    int state;
    while ((state = showroom_prepare(presentation_cars, w->car_code)) == 0) {
        if (plat_time_ns() - start > UINT64_C(30000000000) || SDL_HasEvent(SDL_QUIT))
            return 0;
        present_wait(m, 1.f / 60, NULL);
    }
    return state == 1;
}

/* the cars' ratings the Arcade picks its opponents by (the physics' setup) */
static gt4_car_ratings *app_car_ratings;
static gt4_career_vehicle *app_car_factory;
static void prepare_menu_audio(menu *m, void *user);
static int rate_cars(void *ctx, const uint64_t *keys, int n, float *out)
{
    return gt4_car_ratings_get(ctx, keys, n, out);
}

/* CPU/script and GL asset preparation share this presentation-thread pump.
   It paints only the independent loader and never re-enters the menu VM. */
static void pump_loading_work(void *user)
{
    (void)user;
    if (!loading_screen_active() || loading_screen_revealing())
        return;
    uint64_t now = plat_time_ns();
    if (presentation_clock && now - presentation_clock < UINT64_C(16000000))
        return;
    if (getenv("MOON_LOADING_TRACE"))
        fprintf(stderr, "loading work frame: gap=%.3f ms\n",
                presentation_clock ? (now - presentation_clock) / 1e6 : 0);
    SDL_PumpEvents();
    video_set_menu_pacing(1);
    video_set_menu_projection(1);
    plat_video_begin_frame();
    loading_screen_draw();
    plat_video_end_frame();
    presentation_clock = plat_time_ns();
}

#include "storage_present.inc"

static menu *load(gt4_vol *v, int k)
{
    SWP_LOAD_BEGIN(projects[k]);
    char err[256] = "";
    menu *m = menu_open_presented(v, k == 7 ? "boot" : projects[k], err, sizeof err,
                                  pump_loading_work, NULL);
    if (!m)
        fprintf(stderr, "project %s: %s\n", projects[k], err);
    if (m && k == 7) {
        save_ui_attach(&save_screen, m);
        save_ui_transfer_tools(&save_screen, app_video_settings && app_video_settings->current.save_tools);
    }
    if (m) {
        ui_set_capture_renderer(menu_ui(m), video_grab);
        menu_set_profile_picker(m, present_profile_picker);
    }
    if (m && k == 17) app_jukebox_attach(m);
    menu_set_presenter(m, present_wait, NULL);
    menu_set_music_fader(m, fade_menu_music);
    menu_set_audio_preparer(m, prepare_menu_audio, NULL);
    menu_set_dialog_presenter(m, present_dialog);
    menu_set_progress_presenter(m, present_progress);
    menu_set_save_presenter(m, present_save);
    menu_set_load_presenter(m, present_load);
    menu_set_profile_change_presenter(m, present_profile_change);
    menu_set_event_presenter(m, present_event, NULL);
    if (app_car_ratings)
        menu_set_car_rater(m, rate_cars, app_car_ratings);
    if (m && k == 3 && app_video_settings)
        video_settings_attach(app_video_settings, m);
    /* every project's ModelFace: the map's icons, the event prize's trophy */
    if (m)
        ui_set_model_renderer(menu_ui(m),
                              (ui_model_renderer){ui_mesh_create, ui_mesh_draw, ui_mesh_destroy});
    if (m && !strcmp(projects[k], "gtmode")) {
        ui_set_car_renderer(menu_ui(m), draw_car_face, m);
        menu_set_car_loader(m, load_car_face, NULL);
    }
    if (m && !strcmp(projects[k], "arcade"))
        ui_set_car_renderer(menu_ui(m), draw_arcade_car_face, m);
    if (m)
        ui_prepare_dialogs(menu_ui(m), pump_loading_work, NULL);
#ifdef __SWITCH__
    if (m && !strcmp(projects[k], "arcade")) {
        /* P: GameRoot/CategoryRoot's first paint otherwise generates font
           distance fields and uploads images after the player has clicked.
           Warm their visibility variants before TopRoot becomes interactive. */
        int own_loading = !loading_screen_active();
        if (own_loading)
            loading_screen_begin();
        SWP_LOAD_BEGIN("arcade-2d-warmup");
        ui_project *ui = menu_ui(m);
        for (int root = 0; root < ui_root_count(ui); root++) {
            const char *name = ui_root_name(ui, root);
            if (!strcmp(name, "GameRoot") || !strcmp(name, "CategoryRoot") ||
                !strcmp(name, "CourseRoot"))
                ui_prepare_root_states(ui, root, pump_loading_work, NULL);
        }
        SWP_LOAD_END();
        if (own_loading)
            loading_screen_end();
    }
#endif
    SWP_LOAD_END();
    return m;
}

static void switch_to(gt4_vol *vol, menu **cache, menu **mn, int *proj, char *next)
{
    char *requested_root = strchr(next, ':');
    if (requested_root)
        *requested_root++ = 0;
    for (int i = 0; i < NPROJECTS; i++)
        if (!strcmp(next, projects[i])) {
            if (getenv("MOON_MENU_TRACE"))
                printf("project route: %s -> %s\n", projects[*proj], projects[i]);
            char photo_course[32] = "", photo_car[96] = "";
            int photo_color = 0;
            if (photo_project(i) &&
                !menu_photo_request(*mn, photo_course, photo_car, &photo_color)) {
                menu_confirm(*mn, "Photo Travel",
                             "Choose a car in your garage before entering Photo Travel.");
                ui_fade(menu_ui(*mn), 0, 0, 0);
                return;
            }
            int loading_boundary = (i == 1 && *proj != 1) || (*proj == 1 && i == 0) ||
                                   race_quick_project(i) || race_quick_project(*proj) ||
                                   photo_project(i) || photo_project(*proj);
            int own_loading = loading_boundary && !loading_screen_active();
            if (own_loading) {
                plat_audio_set_music(NULL, NULL);
                music_faded = 1;
                loading_screen_begin();
            }
            /* Give cached project loads the same visible, stationary loading
           cursor as page loads. This minimum delay is presentation only. */
            ui_cursor_icon(menu_ui(*mn), "load");
            ui_cursor_hold(menu_ui(*mn), 1);
            present_wait(*mn, menu_loading_duration(*mn), NULL);
            ui_cursor_hold(menu_ui(*mn), 0);
            ui_cursor_icon(menu_ui(*mn), NULL);
            if (!cache[i])
                cache[i] = load(vol, i);
            int arcade_return = -1;
            if (i == 0 && race_quick_project(*proj) && cache[i]) {
                /* ArcadeProject.onLoad creates a new render context/project;
                   retain main.game, not the race's hidden cursor and showroom. */
                menu_save_capture(cache[i]);
                menu *fresh = load(vol, i);
                if (fresh)
                    arcade_return = menu_arcade_reload_state(fresh, cache[i]);
                if (arcade_return < 0) {
                    if (fresh)
                        menu_close(fresh);
                    if (own_loading)
                        loading_screen_end();
                    return;
                }
                menu_close(cache[i]);
                cache[i] = fresh;
            }
            if (!cache[i]) {
                if (own_loading)
                    loading_screen_end();
                return;
            }
            if (photo_project(i) &&
                !photo_travel_load_from(presentation_photo, cache[i], *mn, photo_course, photo_car,
                                        photo_color, pump_loading_work, NULL)) {
                fprintf(stderr, "Photo Travel: scene/car load failed: %s / %s\n", photo_course,
                        photo_car);
                ui_fade(menu_ui(*mn), 0, 0, 0);
                if (own_loading)
                    loading_screen_end();
                return;
            }
            if ((*proj == 10 || *proj == 11) && i == 1 && career_race.active &&
                !menu_commit_career_distances(cache[1], *mn, career_race.a_distance,
                                              career_race.b_distance, &career_race.has_distances)) {
                fprintf(stderr, "cannot commit career mileage before return\n");
                return;
            }
            if (*proj == 11 && i == 1 && career_race.active && career_race.championship) {
                /* 00119618..19664 / 00119BD8: Exit after a completed stage
               advances before suspension; final stage and Retire clear it. */
                uint8_t live[0x90];
                uint64_t keys[6];
                int32_t paints[6];
                if (!menu_championship_context_get(*mn, live)) {
                    fprintf(stderr, "cannot read championship context\n");
                    return;
                }
                for (unsigned j = 0; j < 6; j++) {
                    memcpy(keys + j, career_race.bodies[j] + (j ? 8 : 0), 8);
                    paints[j] = (int8_t)career_race.bodies[j][0x18];
                }
                int finished_final =
                    career_race.result_applied && career_race.round + 1 == career_race.rounds;
                unsigned round =
                    career_race.round + (career_race.result_applied && !finished_final);
                gt4_career_championship_context(live, !career_race.retired && !finished_final,
                                                round, career_race.rounds - 1, career_race.series,
                                                keys, paints, career_race.points);
                if (!menu_championship_context_set(*mn, live)) {
                    fprintf(stderr, "cannot store championship context\n");
                    return;
                }
                menu_save_refresh(cache[1]);
            }
            if (*proj == 13 && i == 1 && career_license.active) {
                if (!menu_license_leave(cache[1], career_license.code, &career_license.visit)) {
                    fprintf(stderr, "cannot record licence visit\n");
                    return;
                }
                career_license.active = 0;
                menu_license_return_prepare(cache[1]);
                menu_save_refresh(*mn);
            }
            if (*proj == 4 && !menu_setting_end(*mn)) {
                fprintf(stderr, "cannot finish setting\n");
                return;
            }
            if (*proj == 10 || *proj == 11 || *proj == 13 || *proj == 15)
                menu_save_capture(*mn);
            if (i == 4)
                menu_save_capture(*mn);
            int career_return =
                ((*proj == 10 || *proj == 11 || *proj == 15) && i == 1 && career_race.active) ||
                (*proj == 13 && i == 1);
            if ((i == 10 || i == 11 || i == 15) && career_race.active)
                menu_quick_event_context(cache[i], career_race.race);
            if (i == 13 && career_license.active)
                menu_quick_event_context(cache[i], career_license.code);
            int new_career = i == 7 ? menu_take_new_career(*mn) : 0;
            char labo_arg[32] = "";
            if (i == 12)
                menu_take_labo_arg(*mn, labo_arg, sizeof labo_arg);
            if (i == 7)
                save_screen.enter_gtmode = menu_take_gtmode_save_entry(*mn);
            if (*proj == 17 && i != 17) {
                app_jukebox_leave();
                menu_jukebox_finish(*mn);
            }
            menu_set_origin(cache[i], projects[*proj]);
            if (i == 4)
                menu_setting_context(cache[i], *mn);
            if (!strcmp(projects[i], "option"))
                menu_option_context(cache[i], *mn);
            if (i == 17) {
                menu_jukebox_context(cache[i], *mn);
                app_jukebox_attach(cache[i]);
            }
            if (i == 18 || i == 19)
                menu_setting_context(cache[i],
                                     *mn); /* FreeRun/Practice exits read major_menu_project/page */
            if (i == 12 &&
                (!strcmp(labo_arg, "PhotoRoot") || !strcmp(labo_arg, "ReplayTheaterRoot")))
                menu_setting_context(
                    cache[i], *mn); /* labomode.ad PhotoRoot exit reads major_menu_project/page */
            /* Project onLoad owns its reveal. Prepare it before any entry
               script so cached state cannot erase a freshly requested fade. */
            menu_project_entry(cache[i]);
            if (i == 4 && !menu_setting_begin(cache[i])) {
                fprintf(stderr, "cannot open setting\n");
                if (own_loading)
                    loading_screen_end();
                return;
            }
            /* Project changes share the cursor just like pages and common dialogs:
           retain its outgoing position before the destination takes focus. */
            ui_cursor_transfer(menu_ui(cache[i]), menu_ui(*mn));
            *mn = cache[i];
            *proj = i;
            int target = arcade_return >= 0 ? arcade_return :
                         menu_active_root(*mn) < 0 ? 0 : menu_active_root(*mn);
            if (requested_root)
                for (int k = 0; k < ui_root_count(menu_ui(*mn)); k++)
                    if (!strcmp(ui_root_name(menu_ui(*mn), k), requested_root)) {
                        target = k;
                        break;
                    }
            /* Clear a cached outgoing fade before authored onStart/panIn runs;
           never replace the game's transition after it has been selected. */
            if (career_return) {
                if (!menu_enter_career_return(*mn))
                    fprintf(stderr, "career reward entry failed\n");
            } else if (i == 12 && labo_arg[0]) {
                if (!menu_open_labo(*mn, labo_arg))
                    fprintf(stderr, "labomode: %s failed\n", labo_arg);
            } else if (!requested_root && race_quick_project(i)) {
                if (!menu_enter_quick_start(*mn))
                    fprintf(stderr, "career quick selector failed\n");
            } else
                menu_enter(*mn, target);
            if (new_career)
                save_ui_new_career(&save_screen);
            /* PresentRoot.onInitialize queues get_paper/get_car (gtmode.ad
           L14466/L14473). Finish the loading overlay before dispatching:
           their authored Syncs are part of the visible ceremony. */
            if (own_loading) {
                ui_prepare_root(menu_ui(*mn), menu_active_root(*mn), pump_loading_work, NULL);
                loading_screen_end();
                plat_video_set_view(640, 480);
            }
            if (!loading_screen_active())
                menu_tick(*mn);
            presentation_clock = plat_time_ns();
            return;
        }
    fprintf(stderr, "menu: unsupported project '%s'\n", next);
}

static void switch_requested(gt4_vol *vol, menu **cache, menu **mn, int *proj)
{
    char next[64];
    if (!menu_take_project(*mn, next, sizeof next))
        return;
    switch_to(vol, cache, mn, proj, next);
}

/* The race the quick menu (quick-arcade's ArcadeRoot) shows before and
   after it: the grid from pole, the player last (the Arcade's grid), each
   car's power and weight as the game computes them. */
/* the session's best time (00112E78 opens it at 0x157529FF, 00112620
   lowers it after each race to the player's best lap) */
#include "main_session.inc"

static int input_name(const char *name, menu_input *out)
{
    static const char *names[] = {"up",   "down",     "left",   "right", "accept",
                                  "back", "triangle", "circle", "square"};
    for (int i = 0; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(name, names[i])) {
            *out = (menu_input)i;
            return 1;
        }
    return 0;
}

/* Saves the current frame (before present) as a PNG. */
static void screenshot(const char *path)
{
    int w = 0, h = 0;
    unsigned char *px = plat_sdl_read_pixels(&w, &h);
    if (px) {
        size_t len = 0;
        void *png = tdefl_write_image_to_png_file_in_memory(px, w, h, 4, &len);
        FILE *f = png ? fopen(path, "wb") : NULL;
        if (f) {
            fwrite(png, 1, len, f);
            fclose(f);
            printf("saved %s\n", path);
        }
        mz_free(png);
    }
    free(px);
}

/* Consume before dispatch: a script may enter a nested modal event loop
   during this input and must receive the following queued input there. */
static char *next_scripted_key(void)
{
    char *key = scripted_keys;
    if (!key || !*key)
        return NULL;
    char *next = strchr(key, ',');
    if (next)
        *next++ = 0;
    scripted_keys = next;
    return key;
}
static int desktop_menu_event(menu *m, plat_key_event *ev)
{
#if defined(__SWITCH__) || defined(__ANDROID__)
    if (ev->touch) {
        int enabled = app_video_settings && app_video_settings->current.touch_menu;
        if (!enabled) ui_touch_mode(menu_ui(m),0);
        int back = 0;
        if (app_video_settings &&
            touch_back_input(dialog_ui,
                             enabled && !ui_has_back_control(menu_ui(m), menu_active_root(m)), m,
                             menu_active_root(m), ev, &app_video_settings->touch_back, &back)) {
            menu_touch(m, 0, 4, 0, 0, 0);
            ui_touch_mode(menu_ui(m), 1);
            if (back)
                menu_send_input(m, MENU_BACK);
            return 1;
        }
        return menu_touch(m, enabled, ev->touch, ev->touch_id, ev->touch_x * 640.f,
                          ev->touch_y * video_view_height());
    }
    if (ev->pad || ev->key) {
        ui_touch_mode(menu_ui(m),0);
        if (app_video_settings)
            app_video_settings->touch_back.tap = (touch_dialog_state){0};
        menu_touch(m, 0, 4, 0, 0, 0);
    }
#else
    if (ev->touch)
        return 1;
#endif
    desktop_input_hints(m,app_video_settings && app_video_settings->current.input_hints,ev);
    return desktop_input(m, app_video_settings && app_video_settings->current.desktop, ev);
}
static int present_event(menu *m, void *user)
{
    (void)user;
    present_wait(m, scripted_interval > 0 ? scripted_interval : 1.0f / 60, NULL);
    if (dialog_shot && !dialog_shot_scripted) {
        video_set_menu_pacing(1);
        video_set_menu_projection(1);
        plat_video_begin_frame();
        draw_pages(m, NULL, NULL);
        screenshot(dialog_shot);
        plat_video_end_frame();
        dialog_shot = NULL;
    }
    plat_key_event ev;
    while (plat_sdl_poll(&ev)) {
        if (desktop_menu_event(m, &ev)) continue;
        const Uint8 *held = SDL_GetKeyboardState(NULL);
        menu_set_pad_held(m, plat_sdl_menu_buttons() | (held[SDL_SCANCODE_Q] ? MENU_PAD_L1 : 0) |
                                 (held[SDL_SCANCODE_E] ? MENU_PAD_R1 : 0));
        menu_input input;
        int valid = 1;
        switch (ev.key) {
        case SDLK_UP:
            input = MENU_UP;
            break;
        case SDLK_DOWN:
            input = MENU_DOWN;
            break;
        case SDLK_LEFT:
            input = MENU_LEFT;
            break;
        case SDLK_RIGHT:
            input = MENU_RIGHT;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            input = MENU_ACCEPT;
            break;
        case SDLK_ESCAPE:
            input = ev.pad ? MENU_CIRCLE : MENU_BACK;
            break;
        case SDLK_F4:
            input = MENU_TRIANGLE;
            break;
        case SDLK_INSERT:
            if (ev.pad)
                input = MENU_SQUARE;
            else
                valid = 0;
            break;
        default:
            valid = 0;
            break;
        }
        if (valid) {
            menu_send_input(m, input);
            break;
        }
    }
    if (plat_should_quit())
        return 0;
    char *key = next_scripted_key();
    if (key) {
        menu_input input;
        if (!strcmp(key, "shot") &&
            dialog_shot) { /* tests: --dialog-shot taken here, inside the modal */
            video_set_menu_pacing(1);
            video_set_menu_projection(1);
            plat_video_begin_frame();
            draw_pages(m, NULL, NULL);
            screenshot(dialog_shot);
            plat_video_end_frame();
            dialog_shot = NULL;
        } else if (strcmp(key, "wait") && input_name(key, &input)) {
            menu_send_input(m, input);
            printf("modal input %s -> %s focus=%s\n", key,
                   ui_root_name(menu_ui(m), menu_active_root(m)), menu_focus_name(m));
        }
    } else if (plat_sdl_get_dialog_answer() >= 0) {
        menu_send_input(m, plat_sdl_get_dialog_answer() ? MENU_ACCEPT : MENU_BACK);
    }
    menu_tick(m);
    return !plat_should_quit();
}

static void dialog_text(ui_widget *w, const char *text)
{
    if (w) {
        free(w->text);
        w->text = strdup(text ? text : "");
    }
}

/* Synchronous to the script, but rendered inside the game without VM reentry. */
static int present_dialog(menu *m, const char *title, const char *message, int type,
                          const char *yes, const char *no)
{
    /* cursor.adc cases 0/2: OK; 1/4: Yes; 3: No initially. */
    int confirm = type == 1 || type == 3 || type == 4;
    if (!dialog_ui)
        return -1;
    int root = -1;
    for (int i = 0; i < ui_root_count(dialog_ui); i++)
        if (!strcmp(ui_root_name(dialog_ui, i), "ConfirmDialog"))
            root = i;
    if (root < 0)
        return -1;
    ui_widget *page = ui_root_widget(dialog_ui, root), *pane = ui_widget_child(page, "Dialog");
    ui_widget *heading = ui_widget_child(pane, "Title"), *body = ui_widget_child(pane, "Message"),
              *buttons = ui_widget_child(pane, "Buttons");
    ui_widget *single = ui_widget_child(buttons, "OK"), *pair = ui_widget_child(buttons, "YesNo");
    ui_widget *positive = ui_widget_child(pair, "Yes"), *negative = ui_widget_child(pair, "No");
    title = moon_dialog_style(dialog_ui, page, type, title);
    dialog_text(ui_widget_child(body, "message"), message);
    ui_widget *message_face = ui_widget_child(body, "message");
    message_face->w = 300;
    message_face->h = 50;
    message_face->scale_x = message_face->scale_y = 1;
    ui_text_wrap(dialog_ui, message_face, 300);
    ui_text_adjust_scale(dialog_ui, message_face);
    float message_size[2];
    ui_text_size(dialog_ui, message_face, message_size);
    body->h = fmaxf(82, message_size[1] + 32);
    buttons->y = body->y + body->h;
    pane->h = buttons->y + buttons->h;
    pane->y = (480 - pane->h) * .5f;
    ui_widget *dialog_bg = ui_widget_child(pane, "bg");
    dialog_bg->h = pane->h;
    single->visible = !confirm;
    pair->visible = confirm;
    dialog_text(ui_widget_child(positive, "label"), yes && *yes ? yes : "OK");
    dialog_text(ui_widget_child(negative, "label"), no && *no ? no : "Cancel");
    /* Common dialog uses bit 2 for focus, represented by native FBox focus. */
    int selected = type == 3 ? 0 : 1, forced = plat_sdl_get_dialog_answer(), result = -1;
    if (forced >= 0)
        selected = confirm ? forced : 1;
    /* cursor.adc open_motion/close_motion; native MMoveActor 002C1960
       and MFadeActor 0020C478. Run actors and cursor on the same original
       update clock, including the speed ratio and unsnapped panel tail. */
    float resting_y = pane->y, motion_time = 0;
    float speed = menu_cursor_speed(m);
    float move_ratio = 0.20000000298023224f * speed;
    float fade_period = 0.5f / speed, fade_step = 1.0f / (fade_period * 60.0f);
    double motion_ticks = 0;
    int closing = 0;
    pane->y = 480;
    page->opacity = 0;
    positive->focused = confirm && selected;
    negative->focused = confirm && !selected;
    single->focused = !confirm;
    ui_cursor_transfer(dialog_ui, menu_ui(m));
    /* Popup actors do not inherit the outgoing-page adapter pause. */
    ui_fade(dialog_ui, 0, 0, 0);
    ui_cursor_pause(dialog_ui, 0);
    ui_cursor_select(dialog_ui, confirm ? (selected ? positive : negative) : single);
    ui_cursor_speed(dialog_ui, speed);
    uint64_t last = plat_time_ns();
#if defined(__SWITCH__) || defined(__ANDROID__)
    touch_dialog_state touch = {0};

#endif
    printf("in-game dialog: %s / %s\n", title, message);
    do {
        plat_key_event ev;
        while (plat_sdl_poll(&ev)) {
            ui_widget *mouse_items[] = {confirm ? negative : single, positive};
            int mouse_action = 0;
            int hovered = desktop_dialog_input(dialog_ui,
                app_video_settings && app_video_settings->current.desktop,
                mouse_items, confirm ? 2 : 1, &ev, &mouse_action);
#if defined(__SWITCH__) || defined(__ANDROID__)
            int touch_enabled = app_video_settings && app_video_settings->current.touch_menu &&
                                !closing && motion_time >= .5f;
            hovered = touch_dialog_input(dialog_ui, touch_enabled, mouse_items, confirm ? 2 : 1,
                                         &ev, &touch, &mouse_action);
#endif
            if (closing || motion_time < .5f) continue;
            if (hovered >= 0 && confirm && selected != hovered) {
                selected = hovered;
                menu_play_effect(m, "cursor");
            }
            if (mouse_action) {
                result = mouse_action == 2 ? 0 : confirm ? selected : 1;
                menu_play_effect(m, mouse_action == 2 ? "cancel" : "ok");
                break;
            }
            if ((ev.key == SDLK_ESCAPE && !ev.pad) || ev.key == SDLK_F4 ||
                (ev.pad && ev.key == SDLK_INSERT)) {
                menu_play_effect(m, "cancel");
                result = 0;
                break;
            }
            if (ev.key == SDLK_LEFT || ev.key == SDLK_RIGHT || ev.key == SDLK_UP ||
                ev.key == SDLK_DOWN)
                if (confirm) {
                    selected = !selected;
                    menu_play_effect(m, "cursor");
                }
            if (ev.key == SDLK_RETURN || ev.key == SDLK_KP_ENTER ||
                (ev.pad && ev.key == SDLK_ESCAPE)) {
                menu_play_effect(m, "ok");
                result = selected;
                break;
            }
        }
        if (plat_should_quit())
            return 0;
        uint64_t now = plat_time_ns();
        float dt = (float)((now - last) / 1e9);
        last = now;
        dt = fminf(dt, .1f);
        if (result >= 0 && !closing) {
            closing = 1;
            motion_time = 0;
            ui_cursor_hold(dialog_ui, 1);
        }
        positive->focused = confirm && selected;
        negative->focused = confirm && !selected;
        single->focused = !confirm;
        ui_cursor_select(dialog_ui, confirm ? (selected ? positive : negative) : single);
        motion_ticks += (double)dt * 60.0;
        while (motion_ticks >= 1.0 - 1e-6) {
            motion_ticks -= 1;
            motion_time += 1.0f / 60.0f;
            float destination = closing ? (result ? -pane->h : 480) : resting_y;
            pane->y = gt4_move_value(pane->y, destination, move_ratio);
            page->opacity =
                gt4_fade_value(page->opacity, closing ? 0 : 1, closing ? -fade_step : fade_step);
            menu_animate(m, 1.0f / 60.0f);
            ui_animate(dialog_ui, root, 1.0f / 60.0f);
        }
        video_set_menu_pacing(1);
        video_set_menu_projection(1);
        uint64_t draw_start = plat_time_ns();
        plat_video_begin_frame();
        if (page_below_popup(m, "CarRoot")) {
            video_begin_scene();
            showroom_set_wheel(presentation_cars, NULL); /* Arcade's cars are stock */
            showroom_set_wing(presentation_cars, NULL);
            showroom_draw(presentation_cars, menu_selected_car(m), dt);
            video_end_scene();
        }
        dialog_background(m);
        ui_draw_root(dialog_ui, root, NULL, NULL);
#if defined(__SWITCH__) || defined(__ANDROID__)
        ui_hide_touch_back(dialog_ui);
#endif
        if (dialog_shot && !closing && motion_time >= .5f) {
            screenshot(dialog_shot);
            dialog_shot = NULL;
        }
        plat_video_end_frame();
        if (getenv("MOON_DIALOG_TRACE"))
            fprintf(stderr, "dialog frame: %.3f ms\n", (plat_time_ns() - draw_start) / 1e6);
        if (forced >= 0 && !closing && motion_time >= .5f) {
            menu_play_effect(m, "ok");
            result = selected;
        }
        SDL_Delay(1);
    } while (!closing || motion_time < .5f);
    printf("dialog motion: entry=480 exit=%.1f result=%d close=%.3fs\n", result ? -pane->h : 480,
           result, motion_time);
    pane->y = resting_y;
    page->opacity = 1;
    ui_cursor_transfer(menu_ui(m), dialog_ui);
    ui_cursor_hold(menu_ui(m), 0);
    presentation_clock = plat_time_ns();
    return result;
}

/* P: profile chooser replaces a PS2 card slot without switching the
   active player. Uses the existing in-game dialog widgets and pad input. */
static int present_profile_picker(menu *m)
{
    int slots[GT4_SAVE_SLOTS], count = 0;
    gt4_save_info info[GT4_SAVE_SLOTS];
    for (int i = 1; i <= GT4_SAVE_SLOTS; i++)
        if (gt4_save_info_get(i, &info[count]) > 0)
            slots[count++] = i;
    if (!count) {
        present_dialog(m, "Player Profiles",
                       "No saved profiles found. Create a profile from Save Games.", 0, "OK", NULL);
        return 0;
    }
    int root = -1;
    for (int i = 0; i < ui_root_count(dialog_ui); i++)
        if (!strcmp(ui_root_name(dialog_ui, i), "ConfirmDialog"))
            root = i;
    if (root < 0)
        return 0;
    ui_widget *page = ui_root_widget(dialog_ui, root), *pane = ui_widget_child(page, "Dialog");
    ui_widget *body = ui_widget_child(pane, "Message"), *buttons = ui_widget_child(pane, "Buttons"),
              *bg = ui_widget_child(pane, "bg");
    ui_widget *group = ui_widget_child(pane, "Profiles");
    if (!group) {
        group = ui_widget_from_text(
            dialog_ui, root,
            "Composite {name string {\"Profiles\"} geometry rectangle {0 45 320 300}}");
        ui_widget_append(pane, group);
        ui_widget *sample = ui_widget_child(ui_widget_child(buttons, "YesNo"), "Yes");
        for (int i = 0; i < GT4_SAVE_SLOTS; i++) {
            ui_widget *row = ui_widget_copy(dialog_ui, sample);
            row->x = 30;
            row->y = i * 32;
            row->w = 260;
            row->h = 24;
            for (int k = 0; k < row->nkids; k++) {
                row->kids[k]->w = 260;
                row->kids[k]->h = 24;
            }
            ui_widget_append(group, row);
        }
    }
    float old_y = pane->y, old_h = pane->h, old_bg_h = bg->h;
    int bv = body->visible, bt = buttons->visible;
    ui_widget *message = ui_widget_child(body, "message"),
              *caution = ui_widget_child(body, "caution"), *title = ui_widget_child(pane, "Title");
    int mv = message->visible, cv = caution->visible;
    message->visible = caution->visible = 0;
    ui_widget_child(title, "bgE")->visible = 0;
    ui_widget_child(title, "titleE")->visible = 0;

    pane->h = 60 + count * 32;
    pane->y = (480 - pane->h) * .5f;
    bg->h = pane->h;
    body->visible = 1;
    buttons->visible = 0;
    group->visible = 1;
    page->opacity = 1;
    dialog_text(ui_widget_child(ui_widget_child(pane, "Title"), "title"), "Select Player Profile");
    for (int i = 0; i < GT4_SAVE_SLOTS; i++) {
        group->kids[i]->visible = i < count;
        group->kids[i]->focused = 0;
        if (i < count) {
            char label[160];
            snprintf(label, sizeof label, "%d: %s", slots[i], info[i].name);
            dialog_text(ui_widget_child(group->kids[i], "label"), label);
        }
    }
    int selected = 0, result = -1;
#if defined(__SWITCH__) || defined(__ANDROID__)
    touch_dialog_state touch = {0};
    touch_back_state back_touch = {0};
#endif
    ui_fade(dialog_ui, 0, 0, 0);
    ui_cursor_transfer(dialog_ui, menu_ui(m));
    ui_cursor_hold(dialog_ui, 0);
    ui_cursor_pause(dialog_ui, 0);
    uint64_t last = plat_time_ns();
    while (result < 0 && !plat_should_quit()) {
        char *key = next_scripted_key();
        menu_input input;
        int capture = 0;
        if (key && !strcmp(key, "shot"))
            capture = 1;
        else if (key && input_name(key, &input)) {
            if (input == MENU_BACK)
                result = 0;
            if (input == MENU_UP || input == MENU_DOWN)
                selected = (selected + (input == MENU_UP ? count - 1 : 1)) % count;
            if (input == MENU_ACCEPT)
                result = slots[selected];
        }
        plat_key_event ev;
        while (plat_sdl_poll(&ev)) {
            int mouse_action = 0;
            int hovered = desktop_dialog_input(dialog_ui,
                app_video_settings && app_video_settings->current.desktop,
                group->kids, count, &ev, &mouse_action);
#if defined(__SWITCH__) || defined(__ANDROID__)
            int touch_enabled = app_video_settings && app_video_settings->current.touch_menu;
            int back = 0;
            if (touch_back_input(dialog_ui, touch_enabled, group, root, &ev, &back_touch, &back)) {
                touch = (touch_dialog_state){0};
                hovered = -1;
                mouse_action = back ? 2 : 0;
            } else
                hovered = touch_dialog_input(dialog_ui, touch_enabled, group->kids, count, &ev,
                                             &touch, &mouse_action);
#endif
            if (hovered >= 0 && selected != hovered) {
                selected = hovered;
                menu_play_effect(m, "cursor");
            }
            if (mouse_action == 2) result = 0;
            if (mouse_action == 1 && hovered >= 0) result = slots[hovered];
            if ((ev.key == SDLK_ESCAPE && !ev.pad) || ev.key == SDLK_F4 ||
                (ev.pad && ev.key == SDLK_INSERT))
                result = 0;
            if (ev.key == SDLK_UP || ev.key == SDLK_DOWN) {
                selected = (selected + (ev.key == SDLK_UP ? count - 1 : 1)) % count;
                menu_play_effect(m, "cursor");
            }
            if (ev.key == SDLK_RETURN || ev.key == SDLK_KP_ENTER ||
                (ev.pad && ev.key == SDLK_ESCAPE))
                result = slots[selected];
        }
        for (int i = 0; i < count; i++)
            group->kids[i]->focused = i == selected;
        ui_cursor_select(dialog_ui, group->kids[selected]);
        uint64_t now = plat_time_ns();
        ui_animate(dialog_ui, root, fminf(.1f, (float)((now - last) / 1e9)));
        last = now;
        video_set_menu_pacing(1);
        video_set_menu_projection(1);
        plat_video_begin_frame();
        ui_draw_under(menu_ui(m), menu_active_root(m));
        ui_draw_root(dialog_ui, root, NULL, NULL);
#if defined(__SWITCH__) || defined(__ANDROID__)
        ui_draw_touch_back(dialog_ui,
                           app_video_settings && app_video_settings->current.touch_menu &&
                               ui_touch_active(dialog_ui),
                           back_touch.tap.selected != NULL, 1.f / 60);
#endif
        if (capture && dialog_shot) {
            screenshot(dialog_shot);
            dialog_shot = NULL;
        }
        plat_video_end_frame();
    }
    group->visible = 0;
    for (int i = 0; i < count; i++)
        group->kids[i]->focused = 0;
    pane->y = old_y;
    pane->h = old_h;
    bg->h = old_bg_h;
    body->visible = bv;
    buttons->visible = bt;
    message->visible = mv;
    caution->visible = cv;
    ui_cursor_transfer(menu_ui(m), dialog_ui);
    ui_cursor_hold(menu_ui(m), 0);
    presentation_clock = plat_time_ns();
    printf("local profile selected: %d\n", result > 0 ? result : 0);
    menu_play_effect(m, result > 0 ? "ok" : "cancel");
    return result > 0 ? result : 0;
}

/* Semantic state for headless screenshot/integration checks. */
static void report_choices(const ui_widget *w)
{
    if (!w)
        return;
    if (w->name && (!strcmp(w->name, "NoticeBase") ||
                    (w->parent && w->parent->name && !strcmp(w->parent->name, "Notice"))))
        printf("notice %s visible=%d opacity=%.3f rect=%.1f,%.1f,%.1f,%.1f text=%s\n", w->name,
               w->visible, w->opacity, w->x, w->y, w->w, w->h, w->text ? w->text : "");
    if (!strcmp(w->type, "ModelFace"))
        printf("model %s/%s mode=%d yaw=%.3f speed=%.3f\n", w->parent ? w->parent->name : "",
               w->name, w->model_target_mode, w->model_angles[1], w->model_velocity[1]);
    if (!strcmp(w->type, "CarFace") && w->car_code)
        printf("car pose %s motion=%d explicit=%d yaw=%.3f speed=%.3f\n", w->car_code,
               w->car_motion, w->car_pose_set, w->model_angles[1], w->car_spin);
    if (!strcmp(w->type, "OptionMenu")) {
        ui_widget *item = ui_option_item(w, w->focus_index);
        printf("choice %s/%s index=%d item=%s\n", w->parent ? w->parent->name : "", w->name,
               w->focus_index, item ? item->name : "<none>");
        return;
    }
    for (int i = 0; i < w->nkids; i++)
        report_choices(w->kids[i]);
}
/* tests: MOON_UI_TREE prints the page's widgets (layout checks) */
static void report_tree(const ui_widget *w, int depth)
{
    if (!w || depth > 14)
        return;
    printf("tree %*s%s %s vis=%d ins=%d geo=%d rect=%.0f,%.0f,%.0f,%.0f%s%s\n", depth * 2, "",
           w->type, w->name ? w->name : "-", w->visible, w->insensitive, w->has_geometry, w->x,
           w->y, w->w, w->h, w->text ? " text=" : "", w->text ? w->text : "");
    for (int i = 0; i < w->nkids; i++)
        report_tree(w->kids[i], depth + 1);
}

/* Slow disc reads, decoder startup and disposal never run in draw_movie.
   Each worker owns a separate volume/file cursor. Requests coalesce when
   the player moves through several cards before decoding finishes. */
#include "main_movie.inc"

#include "main_audio.inc"
