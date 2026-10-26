#include "career_original.h"
/* ---- the quick menus' race (MQuickWork, func_001255F8's class) ---- */
static menu_quick_race quick_race;
static uint64_t quick_result_generation = 1;
void menu_update_quick_player(int32_t power, int32_t weight)
{
    if (quick_race.player < 0 || quick_race.player >= quick_race.entries)
        return;
    quick_race.grid[quick_race.player].power = power;
    quick_race.grid[quick_race.player].weight = weight;
    quick_result_generation++;
}
static uint64_t quick_tune_generation = 1;
void menu_set_quick_race(const menu_quick_race *q)
{
    if (q)
        quick_race = *q;
    else
        memset(&quick_race, 0, sizeof quick_race);
    quick_result_generation++;
    quick_tune_generation++;
}
/* quick-arcade.ad L73..80: replay icons belong to the completed session. */
static uint32_t quick_best = 0x157529ffu;
void menu_set_quick_best(uint32_t best)
{
    quick_best = best;
}
static int32_t quick_best_speed;
void menu_set_quick_best_speed(int32_t speed)
{
    quick_best_speed = speed;
}
static int quick_ghost;
void menu_set_quick_ghost(int on)
{
    quick_ghost = !!on;
}
/* The PS2 loads the Arcade again when the race's quick menu leaves.
   SettingPopup's module code (arcade.ad L4651..4654) then hides the Veil
   its OK raised (black with the course and car names, L4981..4987) and
   re-arms the three fade actors; CarRoot's CarFace (hidden at L4991)
   comes back as authored. The host's cached Arcade runs the same. */
int menu_arcade_return(menu *m)
{
    ah_val popup, veil, root, face, part;
    if (!m || !m->project || strcmp(ah_module_name(m->project), "ArcadeProject"))
        return -1;
    if (ah_get_member(m->vm, m->project, "SettingPopup", &popup) && ah_as_obj(popup, AO_MODULE)) {
        ah_obj *sp = popup.u.o;
        if (ah_get_member(m->vm, sp, "Veil", &veil) && ah_as_obj(veil, AO_MODULE)) {
            ah_set_member(m->vm, veil.u.o, "visible", ah_int(0));
            static const char *const parts[] = {"bg", "Course", "Car"};
            for (int k = 0; k < 3; k++)
                if (ah_get_member(m->vm, veil.u.o, parts[k], &part) && ah_as_obj(part, AO_MODULE)) {
                    ah_val ret;
                    if (ah_call_member(m->vm, sp, "setFadeActor", 1, &part, &ret) < 0)
                        fprintf(stderr, "arcade return: setFadeActor failed\n");
                }
        }
    }
    if (ah_get_member(m->vm, m->project, "CarRoot", &root) && ah_as_obj(root, AO_MODULE) &&
        ah_get_member(m->vm, root.u.o, "CarFace", &face) && ah_as_obj(face, AO_MODULE))
        ah_set_member(m->vm, face.u.o, "visible", ah_int(1));
    /* Original: ArcadeProject.onLoad (arcade.adc, instructions 108..213, L43..69): reload opens
       major_menu_page, not the cached last active page. GameRoot records
       CategoryRoot normally and CourseRoot for favorites-only selection. */
    ah_val page, current_game;
    int selected = ah_call_member(m->vm, game_object(m), "menuRef", 0, NULL, &current_game) > 0 &&
                   current_game.t != AV_NIL;
    if (selected && ah_get_member(m->vm, game_object(m), "major_menu_page", &page) && ah_cstr(page))
        for (int i = 0; i < ui_root_count(m->ui); i++)
            if (!strcmp(ui_root_name(m->ui, i), ah_cstr(page)))
                return i;
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "TopRoot"))
            return i;
    return -1;
}
int menu_arcade_reload_state(menu *fresh, menu *previous)
{
    /* Original: ArcadeProject.onLoad L27..32, L40..69: a fresh render context
       and authored project state, while main.game survives the reload.
       SettingPopup.OK L4980 hides the outgoing context's cursor. */
    uint8_t *data = NULL;
    size_t size = 0;
    if (!fresh || !previous ||
        !gt4_state_pack(previous->vm, game_object(previous), &data, &size))
        return -1;
    int restored = gt4_state_replace(fresh->vm, game_object(fresh), data, size);
    free(data);
    if (!restored)
        return -1;
    menu_bind_favorites(fresh, game_object(fresh));
    menu_setting_context(fresh, previous);
    /* Original: func_00165FC0 +0x00 uses the live selection stack. It is
       transient game state, deliberately excluded from the career file. */
    gt4_career_menu_stack stack;
    if (gt4_career_blob_get(previous->vm, game_object(previous), "_menu_stack_ps2",
                            (uint8_t *)&stack, sizeof stack) &&
        !gt4_career_blob_set(fresh->vm, game_object(fresh), "_menu_stack_ps2",
                             (const uint8_t *)&stack, sizeof stack))
        return -1;
    ah_val mode, game;
    if (ah_call_member(fresh->vm, game_object(fresh), "menuRef", 0, NULL, &mode) > 0 &&
        mode.t != AV_NIL && ah_get_member(fresh->vm, fresh->project, "GameRoot", &game))
        ah_set_member(fresh->vm, game.u.o, "current_game", mode);
    return menu_arcade_return(fresh);
}
void menu_set_quick_completed(menu *m, int completed)
{
    quick_race.can_replay = !!completed;
    /* The host caches projects; the native race caller must re-run L65..173
       when the session changes, including visibility and setIconPosition. */
    if (m && (!strcmp(ah_module_name(m->project), "QuickArcadeProject") ||
              !strcmp(ah_module_name(m->project), "QuickTimeTrialProject")))
        m->entered[0] = 0;
}
int menu_race_is_split(menu *m)
{
    ah_val v;
    return m && ah_get_member(m->vm, game_object(m), "next_sequence", &v) && ah_cstr(v) &&
           !strcmp(ah_cstr(v), "split_battle");
}
const char *menu_split_car(menu *m, int port)
{
    /* arcade.ad GarageRoot.onActivate retains the selected garage car.
       GameRoot.doGame also writes car_code in single-player modes. */
    if (!m || port < 0 || port > 1)
        return NULL;
    ah_val v = ah_nil();
    ah_obj *game = game_object(m);
    ah_get_member(m->vm, game, port ? "car_garage_2p" : "car_garage", &v);
    const char *label = car_label_of(m->vm, v);
    if (label && *label)
        return label;
    ah_get_member(m->vm, game, port ? "car_code_2p" : "car_code", &v);
    label = ah_cstr(v);
    return label ? label : "";
}
const char *menu_riding_car(menu *m)
{
    ah_val garage, car, code;
    if (!m || !ah_get_member(m->vm, game_object(m), "garage", &garage) ||
        garage.t == AV_NIL || !ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &car) ||
        car.t == AV_NIL || !ah_get_member(m->vm, ah_as_obj(car, AO_MODULE), "car_code", &code))
        return "";
    return ah_cstr(code) ? ah_cstr(code) : "";
}

int menu_riding_color(menu *m)
{
    ah_val garage, car, color;
    if (!m || !ah_get_member(m->vm, game_object(m), "garage", &garage) ||
        garage.t == AV_NIL || !ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &car) ||
        car.t == AV_NIL || !ah_get_member(m->vm, ah_as_obj(car, AO_MODULE), "color_index", &color))
        return 0;
    return (int)ah_num(color);
}

int menu_split_owned(menu *m, int port, uint8_t body[0x4b8], int *color)
{
    if (!m || port < 0 || port > 1 || !body || !color)
        return -1;
    ah_obj *game = game_object(m);
    ah_val code = ah_nil(), car = ah_nil();
    *color = (int)menu_game_number(m, port ? "car_color_2p" : "car_color", 0);
    ah_get_member(m->vm, game, port ? "car_garage_2p" : "car_garage", &car);
    if (car.t == AV_NIL) {
        ah_get_member(m->vm, game, port ? "car_code_2p" : "car_code", &code);
        return ah_cstr(code) && *ah_cstr(code) ? 0 : -1;
    }
    /* 00441248 selects its sheet; +0x18 is signed paint. */
    uint8_t setup[0x178];
    if (!gt4_game_car_body(m->vm, ah_as_obj(car, AO_MODULE), body) ||
        !gt4_career_active_setup(body, setup))
        return -1;
    *color = (int8_t)setup[0x18];
    return 1;
}
int menu_race_is_time_trial(menu *m)
{
    ah_val v;
    return m && ah_get_member(m->vm, game_object(m), "next_sequence", &v) && ah_cstr(v) &&
           !strcmp(ah_cstr(v), "time_trial");
}

int menu_get_career_request(menu *m, menu_career_request *out, char *error, size_t capacity)
{
    ah_val sequence = ah_nil(), race = ah_nil(), garage = ah_nil(), car = ah_nil();
    const char *failure = "missing menu or output";
    if (out)
        memset(out, 0, sizeof *out);
    if (!m || !out)
        goto fail;
    ah_obj *game = game_object(m);
    ah_get_member(m->vm, game, "next_sequence", &sequence);
    const char *kind = ah_cstr(sequence);
    failure = "not a career event sequence";
    if (!kind || (strcmp(kind, "solo_event") && strcmp(kind, "championship")))
        goto fail;
    /* _tmp_gtmode.ad L10920 and L10925..10928: preserve the authored
       label and route; a championship code is not an Arcade course. */
    ah_get_member(m->vm, game, "race_code", &race);
    const char *label = ah_cstr(race);
    failure = "missing or oversized race label";
    if (!label || !*label || strlen(label) >= sizeof out->race)
        goto fail;
    ah_get_member(m->vm, game, "garage", &garage);
    ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &car);
    failure = "equipped garage car has no complete original body";
    /* 003EE3C8 / 00444840: session setup comes from the owned car;
       gt4_game_car_body also carries its current +0x498 wear state. */
    if (!gt4_game_car_body(m->vm, ah_as_obj(car, AO_MODULE), out->player_body))
        goto fail;
    snprintf(out->race, sizeof out->race, "%s", label);
    out->championship = !strcmp(kind, "championship");
    if (error && capacity)
        *error = 0;
    return 1;
fail:
    if (out)
        memset(out, 0, sizeof *out);
    if (error && capacity)
        snprintf(error, capacity, "cannot read career event request: %s", failure);
    return 0;
}
#include "menu_license_requests.inc"

/* gtmode.ad MissionRoot.onActivate: race_code "l0m%04d", exit_menu("mission") */
/* gtmode.ad CourseEntryRoot FreeRun / Practice: exit_menu("freerun") or
   ("practice") (00117668 / 00118670), Photo Drive (0011ABE0):
   1 Free Run, 2 Practice, 3 Photo Drive, 0 neither */
int menu_race_is_free(menu *m)
{
    ah_val kind;
    if (!m || !ah_get_member(m->vm, game_object(m), "next_sequence", &kind) || !ah_cstr(kind))
        return 0;
    return !strcmp(ah_cstr(kind), "freerun")       ? 1
           : !strcmp(ah_cstr(kind), "practice")    ? 2
           : !strcmp(ah_cstr(kind), "photo_drive") ? 3
           : !strcmp(ah_cstr(kind), "machinetest")
               ? 4
               : 0; /* gtmode.ad L3607/L3667/L3698: Power & Speed (00115A90) */
}
/* the course they chose (game.course_code) and the garage's riding car:
   its label, paint and the body the race takes as it is; *laps those of
   the race_code Practice sets (practice_info.race_code), 0 without one */
/* a string of the game object (main.game.<name>), "" without one */
const char *menu_game_text(menu *m, const char *name)
{
    ah_val v;
    if (!m || !ah_get_member(m->vm, game_object(m), name, &v) || !ah_cstr(v))
        return "";
    return ah_cstr(v);
}
int menu_free_request(menu *m, char course[32], char car[96], int *color, uint8_t body[0x4b8],
                      int *laps)
{
    if (!m)
        return 0;
    ah_obj *game = game_object(m);
    ah_val scene = ah_nil(), garage = ah_nil(), vehicle = ah_nil(), paint = ah_nil(),
           code = ah_nil();
    ah_get_member(m->vm, game, "course_code", &scene);
    uint8_t row[151];
    *laps = 0;
    load_race_tables(m);
    if (ah_get_member(m->vm, game, "race_code", &code) && ah_cstr(code) && *ah_cstr(code) &&
        gt4_career_race_row(gt4_game_race_lookup_data(), m->race_labels, m->race_labels_size,
                            m->race_db, m->race_db_size, ah_cstr(code), row))
        *laps = gt4_career_race_field(row, GT4_RACE_LAPS);
    ah_get_member(m->vm, game, "garage", &garage);
    ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &vehicle);
    const char *label = car_label_of(m->vm, vehicle);
    ah_get_member(m->vm, ah_as_obj(vehicle, AO_MODULE), "color_index", &paint);
    if (!ah_cstr(scene) || !*ah_cstr(scene) || !label || !*label)
        return 0;
    snprintf(course, 32, "%s", ah_cstr(scene));
    snprintf(car, 96, "%s", label);
    *color = ah_is_num(paint) ? (int)ah_num(paint) : 0;
    return gt4_game_car_body(m->vm, ah_as_obj(vehicle, AO_MODULE), body);
}
/* 0011B160 +0..20 -> 003BB440: Photo Drive uses the disc's "photo_drive"
   RACE row, not Arcade or FreeRun rules. No RACES opponent list exists. */
int menu_photo_drive_rules(menu *m, uint8_t row[151])
{
    if (!m || !row || menu_race_is_free(m) != 3)
        return 0;
    load_race_tables(m);
    return gt4_career_race_row(gt4_game_race_lookup_data(), m->race_labels, m->race_labels_size,
                               m->race_db, m->race_db_size, "photo_drive", row);
}
int menu_race_is_mission(menu *m)
{
    ah_val kind;
    return m && ah_get_member(m->vm, game_object(m), "next_sequence", &kind) && ah_cstr(kind) &&
           !strcmp(ah_cstr(kind), "mission");
}
int menu_get_mission_request(menu *m, char code[32])
{
    ah_val value;
    if (!m || !code || !menu_race_is_mission(m) ||
        !ah_get_member(m->vm, game_object(m), "race_code", &value))
        return 0;
    const char *text = ah_cstr(value);
    if (!text || strlen(text) != 7 || strncmp(text, "l0m", 3) ||
        strspn(text + 3, "0123456789") != 4)
        return 0;
    snprintf(code, 32, "%s", text);
    return 1;
}
int menu_apply_mission_result(menu *m, const char *race, int finished, int32_t position,
                              uint32_t race_time, int32_t aspec, uint32_t prize, uint32_t *award)
{
    if (!m || !gt4_game_apply_mission_result(m->vm, game_object(m), race, finished, position,
                                             race_time, aspec, prize, award))
        return 0;
    menu_save_capture(m);
    return 1;
}
#include "menu_license_results.inc"

int menu_apply_car_wear(menu *m, const uint8_t wear[32])
{
    if (!m || !gt4_game_apply_car_wear(m->vm, game_object(m), wear))
        return 0;
    menu_save_capture(m);
    return 1;
}
int menu_apply_event_distances(menu *m, uint32_t a, uint32_t b)
{
    if (!m || !gt4_game_apply_event_distances(m->vm, game_object(m), a, b))
        return 0;
    menu_save_capture(m);
    return 1;
}
int menu_commit_career_distances(menu *career, menu *quick, uint32_t a, uint32_t b, int *pending)
{
    if (!career || !quick || !pending)
        return 0;
    if (!*pending)
        return 1;
    /* 00113B48 +0x4fc..0x530 (00114044..78) and 00118EF0
       +0x86c..0x890 (0011975c..80): unsigned metres enter persistent
       u64 totals before returning to GT Mode. Preserve PrizeRoot's paid
       balance across the separate native VMs, then refresh its snapshot
       so the outgoing capture/Auto Save cannot overwrite the totals. */
    menu_save_capture(quick);
    menu_save_refresh(career);
    if (!menu_apply_event_distances(career, a, b))
        return 0;
    menu_save_refresh(quick);
    *pending = 0;
    return 1;
}
int menu_apply_career_result(menu *m, const char *race, int finished, int32_t position,
                             uint32_t best_lap, int32_t mode, int32_t aspec, uint32_t prize,
                             uint8_t *best, uint32_t *award)
{
    if (!m || !gt4_game_apply_event_result(m->vm, game_object(m), race, finished, position,
                                           best_lap, mode, aspec, prize, best, award))
        return 0;
    menu_save_capture(m);
    return 1;
}

int menu_apply_championship_result(menu *m, const char *race, const char *series, int finished,
                                   int final_session, int32_t position, uint32_t lap, int32_t mode,
                                   int32_t aspec, uint32_t prize, const int32_t points[6],
                                   int16_t units, uint32_t *award, uint32_t *grand)
{
    if (!m || !gt4_game_apply_championship_result(m->vm, game_object(m), race, series, finished,
                                                  final_session, position, lap, mode, aspec, prize,
                                                  points, units, award, grand))
        return 0;
    menu_save_capture(m);
    return 1;
}
void menu_quick_event_result(menu *m, int finished, uint32_t prize)
{
    quick_race.finished = !!finished;
    quick_race.prize = prize;
    quick_result_generation++;
    if (m && (!strcmp(ah_module_name(m->project), "RaceEventProject") ||
              !strcmp(ah_module_name(m->project), "ChampionshipProject") ||
              !strcmp(ah_module_name(m->project), "MissionProject")))
        for (int i = 0; i < ui_root_count(m->ui); i++)
            m->entered[i] = 0;
}
void menu_quick_event_context(menu *m, const char *race)
{
    if (!m)
        return;
    /* 0010FBD8..0010FCA0 rebuilds quick-license for the next exam.
       Cached pages must finalize before onInitialize runs again:
       quick-license.ad L312 restores Info.h to info_h; otherwise L256
       compares against the previous enlarged height and hides the bar. */
    if (!strcmp(ah_module_name(m->project), "LicenseProject"))
        while (m->depth)
            finalize_page(m, m->pages[--m->depth]);
    /* _tmp_gtmode.ad L10920..10922 / quick-event.ad L201: return route
       remains GT Mode even though this project has its own native VM. */
    ah_obj *game = game_object(m);
    ah_set_member(m->vm, game, "race_code", ah_str(m->vm, race));
    ah_set_member(m->vm, game, "major_menu_project", ah_str(m->vm, "gtmode"));
    ah_set_member(m->vm, game, "next_sequence", ah_str(m->vm, "menu"));
    ah_set_member(m->vm, game, "next_menu_project", ah_str(m->vm, "gtmode"));
    /* 0010FBD8..0010FCA0 rebuilds quick-license for every new exam.
       Our cached project must run its initialization again for that work. */
    if (!strcmp(ah_module_name(m->project), "LicenseProject"))
        for (int i = 0; i < ui_root_count(m->ui); i++)
            m->entered[i] = 0;
}
int menu_enter_quick_start(menu *m)
{
    ah_val page;
    if (!m)
        return 0;
    /* 0010FE40 reload / quick-license.ad L312: finalize the cached
       Information area on same-exam returns too (Options/Retry/result). */
    if (!strcmp(ah_module_name(m->project), "LicenseProject"))
        while (m->depth)
            finalize_page(m, m->pages[--m->depth]);
    /* 00115A90 +0x380/+0x3A4 -> 0010FE40 reloads each menu cycle.
       Its onInitialize L99/L175 refreshes replay buttons and session best.
       The native cache must not retain the first entry's widget values. */
    for (int i = 0; i < ui_root_count(m->ui); i++)
        m->entered[i] = 0;
    /* quick-license.ad, LicenseProject.onLoad +29..33: startPage is
       LicenseRoot. This project does not define getStartPage. */
    if (!strcmp(ah_module_name(m->project), "LicenseProject")) {
        if (!ah_get_member(m->vm, m->project, "LicenseRoot", &page))
            return 0;
    } else {
        ah_val selector;
        if (!ah_get_member(m->vm, m->project, "getStartPage", &selector)) {
            menu_enter(m, menu_active_root(m) < 0 ? 0 : menu_active_root(m));
            return 1;
        }
        if (ah_call_member(m->vm, m->project, "getStartPage", 0, NULL, &page) < 0)
            return 0;
    }
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (ah_as_obj(page, AO_MODULE) == widget_module(m, ui_root_widget(m->ui, i))) {
            menu_enter(m, i);
            return 1;
        }
    return 0;
}
int menu_enter_career_return(menu *m)
{
    if (!m)
        return 0;
    menu_save_refresh(m);
    ah_val root, list;
    /* GTmodeProject.onLoad L141..174 starts a freshly loaded project.
       Native cached roots must initialize again against the returned save;
       TopRoot.onInitialize L1152 checks dated course presents and HomeRoot
       refreshes the owned car/status. Keeping entered skips both. */
    for (int i = 0; i < ui_root_count(m->ui); i++)
        m->entered[i] = 0;
    if (!ah_get_member(m->vm, m->project, "PresentRoot", &root) || !ah_as_obj(root, AO_MODULE) ||
        ah_call_member(m->vm, root.u.o, "makeEntitledPresentListByProject", 0, NULL, &list) < 0 ||
        !ah_as_obj(list, AO_ARRAY))
        return 0;
    /* GT Mode onLoad L141..154: the original script decides eligibility,
       consumes pending results and fills the original presentation list. */
    /* win() also consumes per-race results when the whole series has not
       qualified yet. Capture even an empty list before menu_enter refresh. */
    menu_save_capture(m);
    if (ah_array_size(list.u.o) > 0) {
        ah_set_member(m->vm, root.u.o, "current_list", list);
        for (int i = 0; i < ui_root_count(m->ui); i++)
            if (widget_module(m, ui_root_widget(m->ui, i)) == root.u.o) {
                m->entered[i] = 0;
                menu_enter(m, i);
                return 1;
            }
        return 0;
    }
    menu_enter(m, menu_active_root(m) < 0 ? 0 : menu_active_root(m));
    return 1;
}
/* option.ad RaceRoot.reflect/apply L2092/L2106: index 0 AT, 1 MT,
   automatic_gear = 1-index. GT Mode has no Arcade game.manual field;
   003D6350 +18..20 reads the global gearbox option for the player. */
int menu_driving_manual(menu *m)
{
    return menu_option_number(m, "automatic_gear", 1) == 0;
}
double menu_game_number(menu *m, const char *name, double fallback)
{
    ah_val v;
    if (!m || !ah_get_member(m->vm, game_object(m), name, &v) || !ah_is_num(v))
        return fallback;
    return ah_num(v);
}
int menu_pending_command(const menu *m)
{
    return m && m->quick_command[0];
}
static ui_widget *find_named(ui_widget *w, const char *name)
{
    if (!w)
        return NULL;
    if (w->name && !strcmp(w->name, name))
        return w;
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *f = find_named(w->kids[i], name);
        if (f)
            return f;
    }
    return NULL;
}
/* tests: the active page's widget of that name focused and chosen */
int menu_test_activate(menu *m, const char *name)
{
    int root = m ? menu_active_root(m) : -1;
    ui_widget *w = root >= 0 ? find_named(ui_root_widget(m->ui, root), name) : NULL;
    if (!w)
        return 0;
    set_focus(m, w);
    menu_send_input(m, MENU_ACCEPT);
    return 1;
}
void menu_test_quick_command(menu *m, const char *command)
{
    if (m && command)
        snprintf(m->quick_command, sizeof m->quick_command, "%s", command);
}
int menu_take_quick_command(menu *m, char *out, size_t size)
{
    if (!m || !m->quick_command[0])
        return 0;
    snprintf(out, size, "%s", m->quick_command);
    m->quick_command[0] = 0;
    return 1;
}
static int quick_index(int argc, const ah_val *argv)
{
    int i = argc && ah_is_num(argv[0]) ? (int)ah_num(argv[0]) : -1;
    return i >= 0 && i < quick_race.entries && i < 6 ? i : -1;
}
/* 001109D4 calls 00445880 after garage construction (paint starts at
   zero): an index >= the linked VARIATION row count is rejected, keeping
   zero. 004458D0 +0xA4 selects order == paint+1; 00445B28 +0x1C..+0x3C
   copies the four chip words at +0x24. Do not index a sorted model list. */
static int quick_paint_chip(menu *m, const char *car, int color, uint8_t chip[4][4])
{
    load_car_tables(m);
    uint32_t id, key;
    uint8_t row[256];
    int count = 0;
    memset(chip, 0, 16);
    if (!car || !gt4_specdb_label(m->car_labels, m->car_labels_size, car, &id) ||
        gt4_specdb_row(m->car_links, m->car_links_size, id, row, sizeof row) != 8)
        return 0;
    key = car_u32(row);
    while (gt4_specdb_row_nth(m->car_variants, m->car_variants_size, key, count, row, sizeof row) ==
           52)
        count++;
    if (color >= count)
        color = 0;         /* 00445880 rejects; the new garage keeps zero */
    color = (int8_t)color; /* garage +0x18: sb, then lb in 00445AE8 */
    if (color == -1)
        return 0;
    for (int i = 0; i < count; i++) {
        gt4_specdb_row_nth(m->car_variants, m->car_variants_size, key, i, row, sizeof row);
        if (car_u32(row + 4) == (uint32_t)color + 1) {
            memcpy(chip, row + 36, 16);
            return 1;
        }
    }
    return 0;
}
/* getPower (func_00124F68: quick work +0x364), getWeight (+0x37C),
   getTireType (+0x394), getGridCarName (+4, the car's name),
   getColorChipInfo (+0x304: four colours, VARIATION +0x24 of its paint) */
static int nat_quick_car(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                         ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    int which = (int)(intptr_t)d, i = quick_index(argc, argv);
    if (which == 0) {
        *ret = ah_int(i < 0 ? 0
                            : (quick_race.championship ? quick_race.total_points[i]
                                                       : quick_race.grid[i].power));
        return 0;
    }
    if (which == 1) {
        *ret = ah_int(i < 0 ? 0
                            : (quick_race.championship ? quick_race.round_points[i]
                                                       : quick_race.grid[i].weight));
        return 0;
    }
    if (which == 2) {
        *ret = ah_int(i < 0 ? 0 : quick_race.tire);
        return 0;
    }
    /* 00127DF8 loads the raw result word at quick +0x454 +4*index. */
    if (which == 5) {
        *ret = ah_int(i < 0 ? 0 : (int32_t)quick_race.grid[i].time);
        return 0;
    }
    if (which == 3) {
        const char *name = i < 0 ? NULL : car_name(m, quick_race.grid[i].car);
        *ret = ah_str(vm, name ? name : "");
        return 0;
    }
    {
        ah_obj *a = ah_array_new(vm);
        uint8_t chip[4][4];
        int valid =
            i >= 0 && quick_paint_chip(m, quick_race.grid[i].car, quick_race.grid[i].color, chip);
        for (int k = 0; k < 4; k++) {
            double rgba[4] = {0, 0, 0, 0};
            if (valid)
                for (int j = 0; j < 4; j++)
                    rgba[j] = chip[k][j] / 255.0;
            ah_array_push(vm, a, color_value(vm, rgba));
        }
        *ret = ah_objv(a);
        return 0;
    }
}
/* MUtility.GetTimeString: 001B0D78 -> 0042E478; strings 006A4F70/80/98.
   0x7f is the original dash glyph. Arithmetic is unsigned milliseconds. */
static int nat_time_string(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self;
    (void)d;
    uint32_t t = argc ? (uint32_t)ah_num(argv[0]) : 0x157529ffu;
    char text[64];
    if (t == 0x157529ffu)
        strcpy(text, "\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f");
    else if (t / 3600000u)
        snprintf(text, sizeof text, "%u:%02u'%02u.%03u", t / 3600000u, t / 60000u % 60,
                 t / 1000u % 60, t % 1000);
    else
        snprintf(text, sizeof text, "%u'%02u.%03u", t / 60000u % 60, t / 1000u % 60, t % 1000);
    *ret = ah_str(vm, text);
    return 0;
}
static ah_val calendar_label(menu *m, uint64_t key)
{
    /* 00133370 -> 00447CA0: shared SpecDB key-to-label resolver. */
    unsigned table = (unsigned)(key >> 32);
    const char *name = table == 0      ? "GENERIC_CAR"
                       : table == 0x26 ? "TUNED_CAR"
                       : table == 0x23 ? "RACE"
                       : table == 0x22 ? "COURSE"
                       : table == 29   ? "WHEEL"
                       : table == 30   ? "WING"
                                       : NULL;
    if (!name)
        return ah_nil();
    char path[96];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s.idi", name);
    size_t size = 0;
    uint8_t *labels = gt4_vol_read(m->vol, path, &size);
    const char *label = gt4_specdb_label_for_id(labels, size, (uint32_t)key);
    ah_val value = label ? ah_str(m->vm, label) : ah_nil();
    free(labels);
    return value;
}
static int nat_calendar_get(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    menu *m = data;
    uint8_t e[16];
    int found =
        gt4_game_calendar_event(vm, profile_module(vm, game_object(m), "calendar"), argc, argv, e);
    *ret = ah_nil();
    if (found < 0)
        return ah_error(vm, "invalid original diary history");
    if (!found)
        return 0;
    const gt4_career_home_data *home = gt4_game_home_data();
    const gt4_career_race_lookup_data *race = gt4_game_race_lookup_data();
    const gt4_career_part_data *parts = gt4_game_part_data();
    if (!home || !race || !parts || e[0] > 8)
        return ah_error(vm, "original diary enums unavailable");
    ah_obj *event = ah_array_new(vm);
    ah_array_push(vm, event, ah_str(vm, home->event_names[e[0]]));
    uint64_t key = 0;
    for (unsigned i = 0; i < 8; i++)
        key |= (uint64_t)e[8 + i] << (8 * i);
    if (e[0])
        ah_array_push(vm, event, calendar_label(m, key));
    /* Callback 00133370 +0x17c..0x1850: preserve each event's distinct
       array schema, consumed directly by DiaryRoot's authored scripts. */
    if (e[0] == 1 || e[0] == 2) {
        ah_array_push(vm, event, ah_int(e[1]));
        ah_array_push(vm, event,
                      e[0] == 2  ? ah_int(e[2] | (unsigned)e[3] << 8)
                      : e[2] < 5 ? ah_str(vm, home->car_event_names[e[2]])
                                 : ah_nil());
    } else if (e[0] == 6) {
        ah_array_push(vm, event, e[1] < 5 ? ah_str(vm, home->wheel_event_names[e[1]]) : ah_nil());
        ah_array_push(vm, event, ah_int(e[2]));
    } else if (e[0] == 3 || e[0] == 4 || e[0] == 5 || e[0] == 8) {
        if (e[0] == 8)
            ah_array_push(vm, event,
                          e[3] < 8 ? ah_str(vm, home->course_event_names[e[3]]) : ah_nil());
        const char *grade = gt4_career_result_name(race, e[1]), *method = NULL;
        for (unsigned i = 0; i < 15; i++)
            if (race->method_values[i] == e[2]) {
                method = race->method_names[i];
                break;
            }
        ah_array_push(vm, event, grade ? ah_str(vm, grade) : ah_nil());
        ah_array_push(vm, event, method ? ah_str(vm, method) : ah_nil());
        ah_val time = ah_int(car_u32(e + 4)), text;
        nat_time_string(vm, NULL, NULL, 1, &time, &text);
        ah_array_push(vm, event, text);
    }
    *ret = ah_objv(event);
    return 0;
}
static int nat_calendar_init(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)argc;
    (void)argv;
    ah_set_member(vm, self, "getEvent",
                  ah_bind(vm, self, ah_native(vm, "getEvent", nat_calendar_get, data)));
    *ret = ah_nil();
    return 0;
}
static int power_unit(menu *m);
/* 004729A0: retain the original float words, including HP's last bit. */
static float power_display(float value, int unit)
{
    if (unit == 1 || unit == 2 || unit == 4) {
        uint32_t bits = unit == 4 ? 0x3f3c49bau : 0x3f7c76f2u;
        float factor;
        memcpy(&factor, &bits, 4);
        value *= factor;
    }
    return value;
}
static int nat_photo_format(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                            ah_val *ret)
{
    (void)self;
    (void)d;
    const char *fmt = argc ? ah_cstr(argv[0]) : NULL;
    char text[96];
    /* gtmode.ad L3995/L14895: MUtility's integer format, 00490A40. */
    if (argc == 2 && fmt && !strcmp(fmt, "%d") && ah_is_num(argv[1])) {
        snprintf(text, sizeof text, "%d", (int32_t)to_int(argv[1]));
        *ret = ah_str(vm, text);
        return 0;
    }
    /* 001B0ED0 +70..84 -> 00490A40. PhotoShoot L106/L2350 uses
       %.1t; Setting.setupSlider L111 uses %.3t for gear ratios. */
    int precision = fmt && !strcmp(fmt, "%.1t") ? 1 : fmt && !strcmp(fmt, "%.2t") ? 2 :
                    fmt && !strcmp(fmt, "%.3t") ? 3 : 0;
    if (argc != 2 || !precision || !ah_is_num(argv[1]) || !isfinite(ah_num(argv[1])) ||
        fabs(ah_num(argv[1])) >= 9223372036854775808.)
        return ah_error(vm, "unsupported menu format");
    int64_t v = (int64_t)ah_num(argv[1]);
    if (v == INT64_MIN)
        return ah_error(vm, "invalid fixed decimal");
    uint64_t a = v < 0 ? (uint64_t)-v : (uint64_t)v;
    unsigned scale = precision == 1 ? 10 : precision == 2 ? 100 : 1000;
    snprintf(text, sizeof text, "%s%llu.%0*llu", v < 0 ? "-" : "", (unsigned long long)(a / scale),
             precision, (unsigned long long)(a % scale));
    *ret = ah_str(vm, text);
    return 0;
}
/* 0013DAA0/E0E0/E228 -> 00445FF8/004460F8/00446138: ENGINE
   string keys +0/+8/+4. 0013DBE8/DD90/DF38 format nonzero dimensions.
   00146358/001463E0 use kg and return ---- for undisclosed mass. */
static int nat_catalog_value(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    menu *m = ah_user(vm);
    int field = (int)(intptr_t)d;
    const char *label = car_label_of(vm, ah_objv(self));
    char text[128];
    if (field < 3) {
        uint8_t e[128];
        int drive;
        size_t size;
        if (!gt4_arcade_catalog_parts(arcade_of(m), label, e, &drive))
            return ah_error(vm, "catalog engine unavailable");
        uint8_t *strings = gt4_vol_read(m->vol, "/specdb/GT4_US2560/UnistrDB.sdb", &size);
        const char *value = strings ? gt4_specdb_string(strings, size,
                                                        car_u32(e + (field == 2   ? 8
                                                                     : field == 1 ? 4
                                                                                  : 0)))
                                    : NULL;
        *ret = ah_str(vm, value ? value : "no value");
        free(strings);
        return 0;
    }
    unsigned value;
    if (field == 7) {
        int32_t stats[3];
        if (!gt4_game_car_stats(vm, self, stats))
            return ah_error(vm, "current car statistics unavailable");
        value = (unsigned)stats[2];
    } else {
        uint16_t geometry[4];
        if (!gt4_arcade_catalog_geometry(arcade_of(m), label, geometry))
            return ah_error(vm, "catalog geometry unavailable");
        value = geometry[field - 3];
    }
    if (field >= 6) {
        if (argc != 1 || !ah_cstr(argv[0]) || strcmp(ah_cstr(argv[0]), "%d%s"))
            return ah_error(vm, "unsupported mass format");
        snprintf(text, sizeof text, value ? "%ukg" : "----", value);
    } else
        snprintf(text, sizeof text, value ? "%u" : "-", value);
    *ret = ah_str(vm, text);
    return 0;
}
/* 00127C48/00127D20 -> stock ENGINE via 00445EE8..00445FB8.
   Only the two catalog formats authored by share/catalog_util.ad are accepted.
   Decimal/rounding parity with 00491690 remains A pending formatter oracle. */
static int nat_quick_catalog(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    uint8_t e[128];
    int drive;
    int player = quick_race.player;
    const char *label = self ? car_label_of(vm, ah_objv(self)) : NULL;
    if (!label) {
        if (player < 0 || player >= quick_race.entries)
            return ah_error(vm, "quick player unavailable");
        label = quick_race.grid[player].car;
    }
    if (!gt4_arcade_catalog_parts(arcade_of(m), label, e, &drive))
        return ah_error(vm, "stock catalog parts unavailable");
    int mode = (int)(intptr_t)d, torque = mode & 1;
    const char *format = argc ? ah_cstr(argv[0]) : NULL;
    if (!format || strcmp(format, torque ? "%.2t%s/%s%s" : "%d%s/%s%s"))
        return ah_error(vm, "unsupported catalog format");
    size_t size;
    uint8_t *strings = gt4_vol_read(m->vol, "/specdb/GT4_US2560/UnistrDB.sdb", &size);
    const char *rpm =
        strings ? gt4_specdb_string(strings, size, car_u32(e + (torque ? 20 : 16))) : NULL;
    if (!rpm) {
        free(strings);
        return ah_error(vm, "catalog RPM string unavailable");
    }
    char text[128];
    int32_t stats[3] = {0};
    if (mode >= 2 && !gt4_game_car_stats(vm, self, stats)) {
        free(strings);
        return ah_error(vm, "current car statistics unavailable");
    }
    /* 00145FE8/00146198: undisclosed RPM ('-') suppresses the whole value. */
    if (rpm[0] == '-') {
        free(strings);
        *ret = ah_str(vm, "----");
        return 0;
    }
    if (!torque) {
        static const char *units[] = {"PS", "HP", "BHP", "CH", "kW", "CV", "PF"};
        int unit = power_unit(m);
        float value = mode >= 2 ? (float)stats[0] / 100.0f : (float)(e[26] | (unsigned)e[27] << 8);
        value = power_display(value, unit);
        snprintf(text, sizeof text, "%d%s/%srpm", (int)floorf(value + 0.5f), units[unit], rpm);
    } else {
        const char *unit = menu_option_text(m, "unit_torque", "FTLB"), *label = "kgf.m";
        float value = mode >= 2 ? (float)stats[1] : (float)(e[28] | (unsigned)e[29] << 8);
        /* 00472A08 +0x38/+0x50; torque is stored in hundredths. */
        if (!strcmp(unit, "FTLB")) {
            uint32_t bits = 0x40e773ea;
            float factor;
            memcpy(&factor, &bits, 4);
            value *= factor;
            label = "ft.lb";
        } else if (!strcmp(unit, "NM")) {
            uint32_t bits = 0x411ccccc;
            float factor;
            memcpy(&factor, &bits, 4);
            value *= factor;
            label = "NM";
        }
        snprintf(text, sizeof text, "%.2f%s/%srpm", floorf(value + 0.5f) / 100.0, label, rpm);
    }
    free(strings);
    *ret = ah_str(vm, text);
    return 0;
}
static int quick_aspec_available(menu *m, const char *race)
{
    ah_vm *vm = m->vm;
    uint8_t row[151];
    unsigned passed = 0;
    load_race_tables(m);
    if (!race || !*race ||
        !gt4_career_race_row(gt4_game_race_lookup_data(), m->race_labels, m->race_labels_size,
                             m->race_db, m->race_db_size, race, row))
        return 0;
    unsigned need = row[0x45];
    if (need >= 1 && need <= 5) {
        static const char *series[] = {"l0b", "l0a", "lib", "lia", "l0s"};
        ah_val records, result;
        if (ah_get_member(vm, game_object(m), "race_record", &records) && records.t == AV_OBJ) {
            ah_val arg = ah_str(vm, series[need - 1]);
            if (ah_call_member(vm, records.u.o, "isLicensePassed", 1, &arg, &result) >= 0 &&
                ah_truthy(result))
                passed = 1u << (need - 1);
        }
    }
    return gt4_career_aspec_allowed((uint8_t)need, passed);
}
int menu_career_aspec_available(menu *m, const char *race)
{
    return m ? quick_aspec_available(m, race) : 0;
}
/* MQuickWork(): this VM's view of the quick race, its settings kept
   between calls (selectedCommand, cursorPosition, the quick tune) */
static int nat_quick_work(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    (void)self;
    (void)d;
    (void)argc;
    (void)argv;
    menu *m = ah_user(vm);
    ah_obj *q = m->quick_work;
    ah_val v;
    if (!q) {
        static const char *const methods[] = {"getPower",       "getWeight",        "getTireType",
                                              "getGridCarName", "getColorChipInfo", "getGridTime"};
        q = m->quick_work = ah_module_new(vm, "MQuickWork", NULL, NULL, NULL);
        ah_pin(vm, ah_objv(q));
        for (int i = 0; i < 6; i++)
            ah_set_member(vm, q, methods[i],
                          ah_native(vm, methods[i], nat_quick_car, (void *)(intptr_t)i));
        static const char *const settings[] = {"QuickTunePowerLevel",   "QuickTuneWeightLevel",
                                               "QuickTuneTransmission", "QuickTuneDrivingAssist",
                                               "QuickTuneGearMaxSpeed", "IsQuickTuned",
                                               "cursorPosition"};
        for (int i = 0; i < 7; i++)
            ah_set_member(vm, q, settings[i], ah_int(0));
        ah_set_member(vm, q, "selectedCommand", ah_str(vm, ""));
        ah_set_member(vm, q, "getCatPs", ah_native(vm, "getCatPs", nat_quick_catalog, NULL));
        ah_set_member(vm, q, "getCatTq", ah_native(vm, "getCatTq", nat_quick_catalog, (void *)1));
    }
    ah_set_member(vm, q, "numberOfEntries", ah_int(quick_race.entries));
    ah_set_member(vm, q, "playerGridNumber", ah_int(quick_race.player));
    ah_set_member(vm, q, "courseLabel", ah_str(vm, quick_race.course));
    /* 00119AA0..00119AB4 populate MQuickWork +0x4f4/+0x4f0. */
    ah_set_member(vm, q, "Laps", ah_int(quick_race.laps));
    ah_set_member(vm, q, "CourseLength", ah_int(quick_race.course_length));
    /* 00114250 initializes event work +0x46c (completion), +0x490
       (driver mode), +0x4a4 (prize); 00114498 supplies the actual prize.
       quick-event.ad L22..39 / quick-championship.getStartPage consume
       these fields. Refresh on every call: cached VMs must not retain the
       preceding race's result when the next pre-race work is installed. */
    ah_set_member(vm, q, "raceLabel", ah_str(vm, quick_race.race));
    /* 0011D340: exam limits from MRaceData, not Arcade lap records. */
    if (quick_race.license) {
        ah_set_member(vm, q, "canLoadGhost", ah_int(quick_race.license_load_ghost));
        ah_set_member(vm, q, "DisableLogger", ah_int(quick_race.license_disable_logger));
        /* 00127AF0 +0x20..0x34 calls 0042E478: these properties are
           formatted strings, directly assigned by quick-license.ad L276..278. */
        const char *names[] = {"goldTime", "silverTime", "bronzeTime"};
        uint32_t times[] = {quick_race.gold_time, quick_race.silver_time, quick_race.bronze_time};
        for (int i = 0; i < 3; i++) {
            ah_val value = ah_int(times[i]), formatted;
            nat_time_string(vm, NULL, NULL, 1, &value, &formatted);
            ah_set_member(vm, q, names[i], formatted);
        }
    }
    ah_set_member(vm, q, "IsSessionFinished", ah_int(!!quick_race.finished));
    /* quick-championship.ad L1049..1050 consumes these writable fields.
       Re-entering/replaying must not restore an already paid award. */
    if (m->quick_result_generation != quick_result_generation) {
        ah_set_member(vm, q, "prize", ah_int((int32_t)quick_race.prize));
        ah_set_member(vm, q, "GrandPrize", ah_int((int32_t)quick_race.grand_prize));
        m->quick_result_generation = quick_result_generation;
    }
    /* 00119918/0011A168/00119F28: championship work getters. */
    ah_set_member(vm, q, "SessionNumber", ah_int(quick_race.session_number));
    ah_set_member(vm, q, "IsFinalSession", ah_int(quick_race.final_session));
    ah_set_member(vm, q, "SeriesRank", ah_int(quick_race.series_rank));
    /* quick-arcade.ad L601 consults main.game.course_code in QuickTune.
       Native projects have separate VMs; carry the race caller's course. */
    ah_set_member(vm, game_object(m), "course_code", ah_str(vm, quick_race.course));
    ah_set_member(vm, q, "canReplay", ah_int(quick_race.can_replay));
    ah_set_member(vm, q, "CanLogger", ah_int(logger_can()));
    /* quick-tt.ad L72: SaveGhost shown while the session has a best lap
       to save (menu_set_quick_ghost). 00112E78 sets BestTime (+0x498)
       to the no-time 0x157529FF when the session opens; 00112620 lowers
       it to each race's best lap. */
    ah_set_member(vm, q, "canSaveGhost", ah_int(quick_ghost));
    ah_set_member(vm, q, "BestTime", ah_int((int32_t)quick_best));
    ah_set_member(vm, q, "BestMaxSpeed", ah_int(quick_best_speed)); /* 00115CF4..00115D70: +0x49C */
    if (quick_race.player >= 0 && quick_race.player < quick_race.entries) {
        const char *car = quick_race.grid[quick_race.player].car;
        uint32_t id;
        uint8_t row[24], engine[128];
        int drive;
        load_car_tables(m);
        if (gt4_specdb_label(m->car_labels, m->car_labels_size, car, &id) &&
            gt4_specdb_row(m->car_db, m->car_db_size, id, row, sizeof row) == 24 &&
            gt4_arcade_catalog_parts(arcade_of(m), car, engine, &drive)) {
            /* Quick work +0x4E4/4E8/4EC, filled at 00112F44..00112F78.
               00448908: category %100; 004488A0: model year. */
            ah_set_member(vm, q, "driveTrainType", ah_int(drive));
            ah_set_member(vm, q, "carCategory", ah_int(row[17] % 100));
            ah_set_member(vm, q, "carYear", ah_int(gt4_career_car_year(row)));
        }
    }
    ah_set_member(vm, q, "IsBSpec", ah_int(!!quick_race.bspec));
    /* 00114334..00114340 / 005C4610: IsASpec is licence eligibility,
       independent of IsBSpec. quick-event.ad:311 consults it on Start. */
    ah_val race_label = ah_nil();
    ah_get_member(vm, q, "raceLabel", &race_label);
    ah_set_member(vm, q, "IsASpec", ah_int(menu_career_aspec_available(m, ah_cstr(race_label))));
    ah_set_member(vm, q, "IsDryCourse", ah_int(quick_race.dry));
    if (m->quick_tune_generation != quick_tune_generation) {
        /* 00111B50 +0x19C..0x238, for each race prepared: no power or
           weight change, the tyres the race runs, the options' gearbox
           (00430878) and aids (00110B70: 2 both, 1 TCS alone), the
           gearbox's top speed and range, IsQuickTuned off */
        /* quick_tune_popup.ad L370..404 reads the same game.option */
        ah_val options, o;
        ah_obj *opt = ah_get_member(vm, game_object(m), "option", &options)
                          ? ah_as_obj(options, AO_MODULE)
                          : NULL;
        int manual = opt && ah_get_member(vm, opt, "automatic_gear", &o) && ah_is_num(o)
                         ? (int)ah_num(o) == 0
                         : 0;
        int asm_on = opt && ah_get_member(vm, opt, "assist_asm", &o) && ah_is_num(o)
                         ? (int)ah_num(o) != 0
                         : 0;
        int tcs_on = opt && ah_get_member(vm, opt, "assist_tcs", &o) && ah_is_num(o)
                         ? (int)ah_num(o) != 0
                         : 1;
        ah_set_member(vm, q, "QuickTunePowerLevel", ah_int(0));
        ah_set_member(vm, q, "QuickTuneWeightLevel", ah_int(0));
        ah_set_member(vm, q, "QuickTuneFrontTireType", ah_int(quick_race.tire));
        ah_set_member(vm, q, "QuickTuneRearTireType", ah_int(quick_race.tire));
        ah_set_member(vm, q, "QuickTuneTransmission", ah_int(manual));
        ah_set_member(vm, q, "QuickTuneDrivingAssist", ah_int(asm_on ? (tcs_on ? 2 : 0) : tcs_on));
        ah_set_member(vm, q, "QuickTuneGearMaxSpeed", ah_int(quick_race.gear));
        ah_set_member(vm, q, "IsQuickTuned", ah_int(0));
        m->quick_tune_generation = quick_tune_generation;
    }
    ah_set_member(vm, q, "QuickTuneGearMaxSpeedMin", ah_int(quick_race.gear_min));
    ah_set_member(vm, q, "QuickTuneGearMaxSpeedMax", ah_int(quick_race.gear_max));
    *ret = ah_objv(q);
    return 0;
}
/* The QuickTune as 00111DC0 takes it at the Arcade's start; IsQuickTuned
   is consumed (+0x488 = 0). 0 without a quick work. */
int menu_quick_tune(menu *m, menu_quick_tune_values *out)
{
    if (!m || !m->quick_work || !out)
        return 0;
    ah_obj *q = m->quick_work;
    ah_val v;
    static const char *const names[] = {"IsQuickTuned",          "QuickTunePowerLevel",
                                        "QuickTuneWeightLevel",  "QuickTuneFrontTireType",
                                        "QuickTuneRearTireType", "QuickTuneGearMaxSpeed",
                                        "QuickTuneTransmission", "QuickTuneDrivingAssist"};
    int *fields[] = {&out->tuned,     &out->power, &out->weight,       &out->front_tire,
                     &out->rear_tire, &out->gear,  &out->transmission, &out->assist};
    for (unsigned i = 0; i < 8; i++)
        *fields[i] = ah_get_member(m->vm, q, names[i], &v) && ah_is_num(v) ? (int)ah_num(v) : 0;
    ah_set_member(m->vm, q, "IsQuickTuned", ah_int(0));
    return 1;
}
/* MUnit.GetUnitOfPower (func_001AF648) and GetPowerForDisplay (func_001AF598:
   PS converted by the option's unit, func_004729A0, rounded): PS, HP
   (x 0.98619), BHP (the same), CH, kW (x 0.7355), CV, PF */
static int power_unit(menu *m)
{
    /* option.ad PowerString uses KW; 004726B8 displays it as kW. */
    static const char *const names[] = {"PS", "HP", "BHP", "CH", "KW", "CV", "PF"};
    const char *u = "HP";
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
        if (saved_options[i].valid && saved_options[i].type == 1 &&
            !strcmp(saved_options[i].name, "unit_power"))
            u = saved_options[i].text;
    if (m)
        u = menu_option_text(m, "unit_power", u);
    if (u && !strcmp(u, "kW"))
        return 4; /* old native preference files */
    for (int i = 0; i < 7; i++)
        if (u && !strcmp(u, names[i]))
            return i;
    return 1;
}
/* MUnit 001AF3B0: km/h hundredths -> the option's hundredths, rounded;
   001AF480 -> 00472680 supplies the suffix. This does not change physics. */
static int nat_velocity_unit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                             ah_val *ret)
{
    (void)self;
    menu *m = ah_user(vm);
    int mph = strcmp(menu_option_text(m, "unit_velocity", "MPH"), "KMPH") != 0;
    if (!d) {
        *ret = ah_str(vm, mph ? "mph" : "km/h");
        return 0;
    }
    if (argc < 1 || !ah_is_num(argv[0]))
        return ah_error(vm, "GetVelocityForDisplay requires a value");
    float v = (float)ah_num(argv[0]);
    if (mph) {
        uint32_t word = 0x3e8e38e3u;
        float factor;
        memcpy(&factor, &word, 4);
        v = v * factor;
        v = v * 3.6f;
        v = v * 0.6213712f;
    }
    *ret = ah_int((int32_t)(v + 0.5f));
    return 0;
}
static int nat_power_unit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                          ah_val *ret)
{
    static const char *const names[] = {"PS", "HP", "BHP", "CH", "kW", "CV", "PF"};
    (void)self;
    int unit = power_unit(ah_user(vm));
    if (!d) {
        *ret = ah_str(vm, names[unit]);
        return 0;
    }
    float ps = argc && ah_is_num(argv[0]) ? (float)ah_num(argv[0]) : 0, v = power_display(ps, unit);
    *ret = ah_int((int32_t)(v + 0.5f));
    return 0;
}

/* MUnit.GetTorqueForDisplay: 001AF760 +34..58 converts through
   00472A08, adds 0.5, and cvt.w.s under the game's toward-zero mode.
   GetUnitOfTorque 001AF810 -> 00472758 returns the original unit labels.
   These are display values only; no dynamics or car parameters change. */
static int torque_unit(menu *m)
{
    const char *unit = "FTLB";
    for (unsigned i = 0; i < sizeof saved_options / sizeof saved_options[0]; i++)
        if (saved_options[i].valid && saved_options[i].type == 1 &&
            !strcmp(saved_options[i].name, "unit_torque"))
            unit = saved_options[i].text;
    if (m)
        unit = menu_option_text(m, "unit_torque", unit);
    return !strcmp(unit, "FTLB") ? 1 : !strcmp(unit, "NM") ? 2 : 0;
}
static int nat_torque_unit(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv,
                           ah_val *ret)
{
    static const char *const names[] = {"kgf.m", "ft.lb", "NM"};
    (void)self;
    int unit = torque_unit(ah_user(vm));
    if (!d) {
        *ret = ah_str(vm, names[unit]);
        return 0;
    }
    float value = argc && ah_is_num(argv[0]) ? (float)ah_num(argv[0]) : 0;
    if (unit) {
        uint32_t bits = unit == 1 ? 0x40e773ea : 0x411ccccc;
        float factor;
        memcpy(&factor, &bits, 4);
        value *= factor;
    }
    *ret = ah_int((int32_t)(value + 0.5f));
    return 0;
}

static int nat_car_name(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                        ah_val *ret)
{
    (void)self;
    const char *code = argc ? ah_cstr(argv[0]) : NULL;
    menu *m = ah_user(vm);
    const char *name = code ? car_name(m, code) : NULL;
    if (data) { /* 0012DFC8 ->00448280: CAR_NAME's short string, field +8. */
        uint32_t id;
        uint8_t row[16];
        name = NULL;
        if (code && gt4_specdb_label(m->car_labels, m->car_labels_size, code, &id) &&
            gt4_specdb_row(m->car_names, m->car_names_size, id, row, sizeof row) == 16)
            name = gt4_specdb_string(m->car_strings, m->car_strings_size, car_u32(row + 8));
    }
    *ret = ah_str(vm, name ? name : "");
    return 0;
}

/* Retail US 001AD528 constructs false; 001AF928 returns D_00693048 ("kg"). */
static int nat_ui_constant(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                           ah_val *ret)
{
    (void)self; (void)argc; (void)argv;
    *ret = data ? ah_str(vm, "kg") : ah_int(0);
    return 0;
}
