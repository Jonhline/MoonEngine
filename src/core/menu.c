#include "gtmode_game.h"
#include "menu.h"

#include "adhoc/adhoc_vm.h"
#include "mod_manifest.h"
#include "platform/platform.h"
#ifdef __SWITCH__
#include "platform/switch_file.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "sound.h"
#include "save.h"
#include "career_blob.h"
#include "career_parts.h"
#include "specdb.h"
#include "favorites.h"
#include "arcade.h"
#include "career_admission.h"
#include "replay_file.h"
#include "disc_replay.h"
#include "ps2_media.h"
static char film_loaded[96];
#include "pad_config.h"
#include "album_slide.h"
#include "tex1.h"
#include "gpb.h"
#include "pcm_music.h"
#include "ranking_code.h"
static char memcard_origin[64]; /* where the memory card screen goes back to */
static int replay_action;       /* MMemoryCardManager::setAction: 1 plays the replay loaded */
static char replay_loaded[64];  /* the replay a memory card screen loaded (below) */
static int
    replay_theater; /* labomode opened at ReplayTheaterRoot: a replay played there comes back to it */
/* the playlist played (commandPlayList: menu_result "list"), one replay
   after the other, in order or shuffled (option replay_play_mode) */
static char replay_queue[128][64];
static int replay_queue_n, replay_queue_at;
#ifdef _WIN32
#include <windows.h>
#endif

#include "menu_internal.h"

static int demo_movie_count; /* manager +6C, remaining until movie */
static int attract_index, attract_left, attract_cancelled;
static char attract_pending[64], attract_origin[64];
static void attract_next(menu *m);
int menu_attract_active(void)
{
    return attract_left > 0;
}
int menu_take_attract_replay(char *name, size_t size)
{
    if (!attract_pending[0])
        return 0;
    snprintf(name, size, "%s", attract_pending);
    attract_pending[0] = 0;
    return 1;
}
void menu_attract_finish(menu *m, int cancelled)
{
    if (!m)
        return;
    if (cancelled || attract_cancelled || attract_left <= 1) {
        attract_left = 0;
        attract_cancelled = 0;
        attract_pending[0] = 0;
        m->idle_frames = 0;
        menu_request_project(m, attract_origin);
    } else {
        attract_left--;
        attract_next(m);
    }
}
static void attract_next(menu *m)
{
    /* 00434770 +18..90/+144..168: movie countdown, independent tape index.
       0010BE20 +230..238 loops until the fourth presentation completes. */
    int interval = (int)menu_option_number(m, "demo_movie_interval", 3);
    /* arcade.ad _start_page L52..55 already completed panOut/syncOut.
       Preserve that black surface until the movie/replay takes over. */
    m->idle_frames = 0;
    if (demo_movie_count <= 0 && interval > 0)
        menu_request_project(m, "boot:MovieRoot");
    else {
        char files[32][64];
        int n = gt4_disc_replay_list(m->vol, files, 32, 0);
        if (n <= 0) {
            menu_attract_finish(m, 1);
            return;
        }
        if (attract_index >= n)
            attract_index = 0;
        snprintf(attract_pending, sizeof attract_pending, "%s", files[attract_index]);
        attract_index = (attract_index + 1) % n;
    }
    if (--demo_movie_count < 0)
        demo_movie_count = interval - 1;
}

static void log_line(void *user, const char *msg)
{
    (void)user;
    fprintf(stderr, "script: %s\n", msg);
}

static int to_int(ah_val v);

static int bar_input(menu *m, ui_widget *w, menu_input input);
#include "menu_widgets.inc"

#include "menu_engine_objects.inc"

#include "menu_options.inc"

#include "menu_catalog.inc"

#include "menu_records.inc"

#include "menu_car.inc"
#include "menu_favorites.inc"

#include "menu_storage.inc"

#include "menu_save.inc"

#include "menu_race_data.inc"

#include "menu_quick.inc"

#include "menu_logger.inc"

static void make_engine(menu *m)
{
    ah_vm *vm = m->vm;
    ah_obj *main_ = submodule(vm, ah_root(vm), "main");

    ah_obj *game = submodule(vm, main_, "game");
    ah_obj *option = submodule(vm, game, "option");
    ah_set_member(vm, option, "apply", ah_native(vm, "apply", nat_option_apply, m));
    static const char *const selection_methods[] = {"menuPush", "menuRef", "menuPop",
                                                    "menuClearStack"};
    for (int i = 0; i < 4; i++)
        ah_set_member(vm, game, selection_methods[i],
                      ah_native(vm, selection_methods[i], nat_menu_selection, (void *)(intptr_t)i));
    ah_obj *demo = submodule(vm, game, "demonstration");
    ah_set_member(vm, demo, "movie_count", ah_int(0));
    ah_set_member(vm, demo, "resetMovieCount", ah_native(vm, "resetMovieCount", nat_demo_reset, m));
    ah_set_member(vm, game, "isCourseAvailable",
                  ah_native(vm, "isCourseAvailable", nat_course_available, m));
    ah_set_member(vm, game, "setCourseAvailable",
                  ah_native(vm, "setCourseAvailable", nat_course_unlock, m));
    ah_set_member(vm, game, "isCarAvailable",
                  ah_native(vm, "isCarAvailable", nat_car_available, m));
    ah_set_member(vm, option, "limit_favorite_car_only", ah_int(0));
    ah_set_member(vm, option, "limit_favorite_course_only", ah_int(0));
    ah_set_member(vm, game, "setNeedSave", ah_native(vm, "setNeedSave", nat_need_save, m));
    ah_set_member(vm, game, "setReturnToLastMenuProject",
                  ah_native(vm, "setReturnToLastMenuProject", nat_return_last, m));
    gt4_pad_bind_option(vm, option);
    /* PhotoShoot Mode.Reset L1143 -> 00165CA8 / 00462EC8. */
    ah_set_member(vm, game, "loadShutterSound",
                  ah_native(vm, "loadShutterSound", nat_photo, "loadShutterSound"));
    ah_set_member(vm, game, "need_save", ah_int(0));
    /* Options for a fresh game. Values are the port's defaults until the real
       ones are read from the game (save data / engine). */
    ah_set_member(vm, option, "game_zone", ah_str(vm, "us"));
    ah_set_member(vm, option, "special_arcade_tuner", ah_str(vm, ""));
    ah_set_member(vm, option, "wide_mode", ah_int(0));
    /* Defaults of a fresh option, as 004363A0 builds GranTurismo4Option
       (getters 0017FD20..00189A60 name the offsets): race_laps +0x10C0 2,
       tire_damage +0x10C4 0, automatic_gear +0x10C8 1, assist_asm +0x10D0
       and assist_tcs +0x10D4 both 1, split_boost_level +0x1108 1,
       album_slide_effect +0x1170 1, play time
       +0x1174 8, transition +0x1180 4. demo_movie_interval (+0x116C) is 3 there and in
       the native sequence too. A native save
       overrides them. */
    ah_set_member(vm, option, "automatic_gear", ah_int(1));
    ah_set_member(vm, option, "race_display_config", ah_int(0));
    ah_set_member(vm, option, "race_display_map", ah_int(1));
    ah_set_member(vm, option, "race_display_steering", ah_int(0));
    ah_set_member(vm, option, "can_watch_ending", ah_int(0));
    ah_set_member(vm, option, "sound_type", ah_str(vm, "STEREO"));
    ah_set_member(vm, option, "enableAC3", ah_int(0));
    static const struct {
        const char *name;
        int value;
    } defaults[] = {{"replay_display_enable", 0},
                    {"replay_memory_card_slot", 0},
                    {"opening_after_autoload", 1},
                    {"demo_movie_interval", 3},
                    {"auto_save", 1},
                    {"race_laps", 2},
                    {"tire_damage", 0},
                    {"assist_asm", 1},
                    {"assist_tcs", 1},
                    {"arcade_time_limit", 0},
                    {"split_race_laps", 2},
                    {"split_tire_damage", 0},
                    {"split_handicap", 0},
                    {"split_boost_level", 1},
                    {"album_slide_effect", 1},
                    {"album_slide_play_time", 8},
                    {"album_slide_transition_time", 4},
                    {"album_memory_card_slot", 0},
                    {"vibration_1p", 1},
                    {"vibration_2p", 1},
                    {"active_steering_1p", 0},
                    {"active_steering_2p", 0},
                    {"menu_bgm_volume_raw", 120},
                    {"menu_se_volume_raw", 120},
                    {"race_bgm_volume_raw", 120},
                    {"race_se_volume_raw", 120},
                    {"replay_bgm_volume_raw", 120},
                    {"replay_se_volume_raw", 120},
                    {"slide_bgm_volume_raw", 120},
                    {"narration", 1}};
    for (unsigned i = 0; i < sizeof defaults / sizeof defaults[0]; i++)
        ah_set_member(vm, option, defaults[i].name, ah_int(defaults[i].value));
    static const char *const string_defaults[][2] = {{"photo_shutter", "typeA"},
                                                     {"replay_mode", "NORMAL"},
                                                     {"replay_play_mode", "STRAIGHT"},
                                                     {"penalty_type", "NONE"},
                                                     {"photo_quality", "STANDARD"},
                                                     {"ustorage_photo_quality", "STANDARD"},
                                                     {"steering_assist_1p", "AMATEUR"},
                                                     {"steering_assist_2p", "AMATEUR"},
                                                     {"unit_velocity", "MPH"},
                                                     {"unit_power", "HP"},
                                                     {"unit_torque", "FTLB"}};
    for (unsigned i = 0; i < sizeof string_defaults / sizeof string_defaults[0]; i++)
        ah_set_member(vm, option, string_defaults[i][0], ah_str(vm, string_defaults[i][1]));
    /* 004363A0 +128..134: manual/pro modes start at zero.
       A: 4x3 names the script default branch; enum table pending. */
    ah_set_member(vm, option, "photo_manual_focus", ah_int(0));
    ah_set_member(vm, option, "photo_professional", ah_int(0));
    ah_set_member(vm, option, "photo_aspect", ah_str(vm, "4x3"));
    const char *correction[] = {"race_color_correction", "replay_color_correction"};
    for (int i = 0; i < 2; i++) {
        ah_obj *a = ah_array_new(vm);
        for (int j = 0; j < 4; j++)
            ah_array_push(vm, a, ah_int(0));
        ah_set_member(vm, option, correction[i], ah_objv(a));
    }
    ah_obj *languages = ah_array_new(vm);
    ah_array_push(vm, languages, ah_str(vm, "US"));
    ah_set_member(vm, main_, "UsingLanguages", ah_objv(languages));
    ah_set_member(vm, main_, "UsingLanguageNum", ah_int(1));

    ah_set_member(vm, game, "last_sequence", ah_str(vm, ""));
    ah_set_member(vm, game, "labo_folder", ah_int(-1)); /* labomode.ad: none remembered */
    ah_set_member(vm, game, "labo_list_index", ah_int(-1));
    ah_set_member(vm, game, "major_menu_page", ah_str(vm, ""));
    ah_set_member(vm, game, "last_menu_project", ah_str(vm, ""));
    ah_set_member(vm, game, "next_sequence", ah_str(vm, "menu"));
    ah_set_member(vm, game, "major_menu_project", ah_str(vm, "arcade"));
    ah_set_member(vm, game, "course_code", ah_str(vm, ""));
    ah_set_member(vm, option, "timeout_count_to_autodemo",
                  ah_int(30)); /* native movie-stage attract timeout */

    ah_obj *sound = submodule(vm, main_, "sound");
    ah_set_member(vm, sound, "play", ah_native(vm, "play", nat_sound_play, m));
    ah_set_member(vm, sound, "load", ah_native(vm, "load", nat_sound_load, m));
    ah_set_member(vm, sound, "unload", ah_native(vm, "unload", nat_sound_unload, m));
    ah_set_member(vm, sound, "keepMusic", ah_native(vm, "keepMusic", nat_music, NULL));
    ah_set_member(vm, sound, "fadeOutMusic", ah_native(vm, "fadeOutMusic", nat_music, (void *)1));
    /* 0023F38C registers startStream -> 0023E5C8 -> 0023FA78.
       PaperPane (gtmode.ad:14712) uses the two-argument PCM request.
       Reuse the existing host stream/gain adapter, rather than a stub. */
    ah_set_member(vm, sound, "startStream", ah_native(vm, "startStream", nat_quick_stream, NULL));
    ah_set_member(vm, sound, "isPlayingStream",
                  ah_native(vm, "isPlayingStream", nat_pcm_playing, NULL));
    ah_set_member(vm, sound, "startStreamEx",
                  ah_native(vm, "startStreamEx", nat_quick_stream, NULL));
    ah_set_member(vm, sound, "stopStream",
                  ah_native(vm, "stopStream", nat_quick_stream, (void *)1));
    ah_set_member(vm, sound, "pauseStream", ah_native(vm, "pauseStream", nat_pcm_pause, NULL));
    /* 00180AA8 +0x78..+0xA4: saved raw menu BGM volume / 128. */
    {
        ah_val raw;
        double volume = ah_get_member(vm, option, "menu_bgm_volume_raw", &raw) ? ah_num(raw) : 120;
        ah_set_member(vm, option, "menu_bgm_volume", ah_float((float)(volume / 128.)));
    }

    ah_obj *menu_ = submodule(vm, main_, "menu");
    ah_obj *calendar_class = ah_native_class(vm, "MCalendar", nat_calendar_init, m);
    gt4_calendar_bind(vm, calendar_class);
    ah_set_member(vm, menu_, "MCalendar", ah_objv(calendar_class));
    ah_obj *utility = submodule(vm, menu_, "MUtility");
    ah_set_member(vm, utility, "GetTimeString",
                  ah_native(vm, "GetTimeString", nat_time_string, NULL));
    ah_set_member(vm, utility, "GetFormatString",
                  ah_native(vm, "GetFormatString", nat_photo_format, NULL));
    ah_set_member(vm, utility, "GetMoneyString",
                  ah_native(vm, "GetMoneyString", nat_money_string, m));
    ah_obj *race = ah_native_class(vm, "MRaceData", nat_race_init, m);
    ah_set_member(vm, race, "available", ah_native(vm, "available", nat_race_available, m));
    ah_set_member(vm, menu_, "MRaceData", ah_objv(race));
    ah_obj *color = ah_native_class(vm, "MColorObject", nat_color, NULL);
    ah_set_member(vm, color, "interpolate", ah_native(vm, "interpolate", nat_color, (void *)1));
    ah_set_member(vm, menu_, "MColorObject", ah_objv(color));
    ah_set_member(vm, menu_, "MProgress", ah_native(vm, "MProgress", nat_progress, NULL));
    ah_set_member(vm, menu_, "MLoggerControl",
                  ah_native(vm, "MLoggerControl", nat_logger_control, m));
    static const char *const transitions[] = {"MColorTransition", "MCrossTransition",
                                              "MBlockTransition"};
    for (int i = 0; i < 3; i++)
        ah_set_member(vm, menu_, transitions[i],
                      ah_native(vm, transitions[i], nat_transition, (void *)transitions[i]));
    ah_obj *unit = submodule(vm, menu_, "MUnit");
    ah_set_member(vm, unit, "GetUnitOfVelocity",
                  ah_native(vm, "GetUnitOfVelocity", nat_velocity_unit, NULL));
    ah_set_member(vm, unit, "GetVelocityForDisplay",
                  ah_native(vm, "GetVelocityForDisplay", nat_velocity_unit, (void *)1));
    ah_set_member(vm, unit, "GetCourseLengthString",
                  ah_native(vm, "GetCourseLengthString", nat_course_units, NULL));
    ah_set_member(vm, unit, "GetCourseHeightString",
                  ah_native(vm, "GetCourseHeightString", nat_course_units, (void *)1));
    ah_set_member(vm, unit, "GetUnitOfPower",
                  ah_native(vm, "GetUnitOfPower", nat_power_unit, NULL));
    ah_set_member(vm, unit, "GetPowerForDisplay",
                  ah_native(vm, "GetPowerForDisplay", nat_power_unit, (void *)1));
    ah_set_member(vm, unit, "GetUnitOfTorque",
                  ah_native(vm, "GetUnitOfTorque", nat_torque_unit, NULL));
    ah_set_member(vm, unit, "GetTorqueForDisplay",
                  ah_native(vm, "GetTorqueForDisplay", nat_torque_unit, (void *)1));
    ah_set_member(vm, menu_, "MQuickWork", ah_native(vm, "MQuickWork", nat_quick_work, m));
    ah_obj *car = submodule(vm, menu_, "MCarData");
    ah_set_member(vm, car, "GetCarName", ah_native(vm, "GetCarName", nat_car_name, m));
    static const char *const methods[] = {"GetCarLabel",      "GetTuner",     "GetCountry",
                                          "GetOpenModelType", "GetModelYear", "GetCategory",
                                          "GetPrice",         "NotForSell",   "GetDescriptionID"};
    for (int i = 0; i < 9; i++)
        ah_set_member(vm, car, methods[i],
                      ah_native(vm, methods[i], nat_car_metadata, (void *)methods[i]));
    static const char *const arcade_car[] = {"GetRaceCode",  "IsTireNormal",   "RaceForbidden",
                                             "ShiftCarCode", "IsDirtRunnable", "IsTestCar"};
    for (unsigned i = 0; i < sizeof arcade_car / sizeof *arcade_car; i++)
        ah_set_member(vm, car, arcade_car[i],
                      ah_native(vm, arcade_car[i], nat_arcade_car, (void *)arcade_car[i]));
    ah_set_member(vm, car, "PickUpEnemyCars",
                  ah_native(vm, "PickUpEnemyCars", nat_pick_enemies, m));
    ah_obj *garage_class = ah_native_class(vm, "MCarGarage", nat_car_garage, m);
    const char *sorts[] = {"sortByRideHistory", "sortByPower", "sortByWeight",     "sortByPWR",
                           "sortByDate",        "sortByPrice", "sortByYear",       "sortByCountry",
                           "sortByTuner",       "sortByName",  "sortByDriveTrain", "sortByColor"};
    for (unsigned i = 0; i < 12; i++)
        ah_set_member(vm, garage_class, sorts[i],
                      ah_native(vm, sorts[i], nat_garage_sort_history, (void *)(intptr_t)i));
    ah_set_member(vm, garage_class, "sortByAspiration",
                  ah_native(vm, "sortByAspiration", nat_garage_sort_history, (void *)12));
    ah_set_member(vm, menu_, "MCarGarage", ah_objv(garage_class));
    { /* MRandom::GetValue, and MRandom() whose getValue(min, max) labomode.ad's initBG draws */
        ah_obj *rnd = ah_native_class(vm, "MRandom", nat_random_new, m);
        ah_set_member(vm, menu_, "MRandom", ah_objv(rnd));
        ah_set_member(vm, rnd, "GetValue", ah_native(vm, "GetValue", nat_random_value, m));
    }
    ah_set_member(vm, menu_, "MMemoryCardPlayList",
                  ah_objv(ah_native_class(vm, "MMemoryCardPlayList", nat_playlist_new, m)));
    /* the Arcade's race (the game object's fields, func_00128738: zero) */
    ah_set_member(vm, game, "difficulty", ah_str(vm, "EASY"));
    ah_set_member(vm, game, "enemy_level", ah_int(0));
    ah_set_member(vm, game, "race_code", ah_str(vm, ""));
    ah_set_member(vm, game, "max_entry", ah_int(1));
    ah_set_member(vm, game, "overwrite_enemy_num", ah_int(0));
    ah_set_member(vm, game, "setEntryCarCode",
                  ah_native(vm, "setEntryCarCode", nat_entry_car, (void *)1));
    ah_set_member(vm, game, "getEntryCarCode",
                  ah_native(vm, "getEntryCarCode", nat_entry_car, NULL));
    ah_set_member(vm, option, "limit_enemies_to_favorite_car_only", ah_int(0));
    ah_set_member(vm, profile_module(vm, game, "course_record"), "getUnit",
                  ah_native(vm, "getUnit", nat_score_unit, m));
    ah_set_member(vm, profile_module(vm, game, "machine_record"), "getMTUnit",
                  ah_native(vm, "getMTUnit", nat_machine_unit, m));
    menu_bind_favorites(m, game);
    ah_obj *course = submodule(vm, menu_, "MCourseData");
    ah_set_member(vm, course, "GetMaxCarNum",
                  ah_native(vm, "GetMaxCarNum", nat_course_capacity, m));
    ah_set_member(vm, course, "GetType", ah_native(vm, "GetType", nat_course_text, NULL));
    ah_set_member(vm, course, "GetAttrString",
                  ah_native(vm, "GetAttrString", nat_course_text, (void *)1));
    static const char *const actors[] = {"MFadeActor",  "MMoveActor",   "MBlinkActor",
                                         "MScaleActor", "MRotateActor", "MAnchorActor",
                                         "MChaseActor"};
    for (size_t i = 0; i < sizeof actors / sizeof actors[0]; i++)
        ah_set_member(vm, menu_, actors[i], ah_native(vm, actors[i], nat_actor, (void *)actors[i]));
    ah_set_member(vm, menu_, "MScriptWatcher", ah_native(vm, "MScriptWatcher", nat_watcher, m));
    ah_set_member(vm, menu_, "MScriptEvent",
                  ah_native(vm, "MScriptEvent", nat_event_new, (void *)1));
    ah_set_member(vm, menu_, "MFunctionEvent",
                  ah_native(vm, "MFunctionEvent", nat_event_new, NULL));
    ah_set_member(vm, menu_, "MKeyPressEvent",
                  ah_native(vm, "MKeyPressEvent", nat_photo_event, (void *)0));
    ah_set_member(vm, menu_, "MKeyReleaseEvent",
                  ah_native(vm, "MKeyReleaseEvent", nat_photo_event, (void *)1));
    ah_set_member(vm, menu_, "MActivateEvent",
                  ah_native(vm, "MActivateEvent", nat_photo_event, (void *)2));
    ah_obj *mc = ah_native_class(vm, "MMemoryCardManager", nat_pc_manager, m);
    ah_set_member(vm, menu_, "MMemoryCardManager", ah_objv(mc));
    ah_set_member(vm, mc, "getExistValidGameData",
                  ah_native(vm, "getExistValidGameData", nat_save_status, NULL));
    {
        static const char *const statics[] = {"setAction", "setExistValidGameData", "getPrintList"};
        for (unsigned i = 0; i < 3; i++)
            ah_set_member(vm, mc, statics[i],
                          ah_native(vm, statics[i], nat_mc_static, (void *)statics[i]));
    }
    ah_obj *system = submodule(vm, menu_, "MSystem");
    ah_set_member(vm, system, "GetLanguage", ah_native(vm, "GetLanguage", nat_language, NULL));
    ah_set_member(vm, system, "SetLanguage", ah_native(vm, "SetLanguage", nat_language, (void *)1));
    ah_set_member(vm, system, "GetRegionCode", ah_native(vm, "GetRegionCode", nat_region, NULL));
    ah_set_member(vm, system, "GetVideoSystem",
                  ah_native(vm, "GetVideoSystem", nat_video_system, NULL));
    ah_set_member(vm, system, "GetVersionBranch",
                  ah_native(vm, "GetVersionBranch", nat_version_branch, NULL));
    /* 001AD97C registers 001AC248 -> 00104E30: selects the loading
       presentation by string. The PC application's loading_screen_begin
       owns that boundary; it does not create the PS2 loading thread. */
    ah_set_member(vm, system, "SetLoadingMode", ah_native(vm, "SetLoadingMode", nat_noop, m));

    m->context = ah_module_new(vm, "context", NULL, NULL, NULL);
    ah_set_member(vm, m->context, "getUpdateContext",
                  ah_native(vm, "getUpdateContext", nat_update_context, m));
    ah_set_member(vm, m->context, "getPadButtonState",
                  ah_native(vm, "getPadButtonState", nat_pad_buttons, m));
    ah_set_member(vm, m->context, "getPadAnalogChannel",
                  ah_native(vm, "getPadAnalogChannel", nat_pad_analog, m));
    ah_set_member(vm, m->context, "enterEventLoop",
                  ah_native(vm, "enterEventLoop", nat_event_loop, NULL));
    ah_set_member(vm, m->context, "leaveEventLoop",
                  ah_native(vm, "leaveEventLoop", nat_event_loop, (void *)1));
    ah_set_member(vm, m->context, "cursor_speed_ratio", ah_float(1));
    ah_set_member(vm, m->context, "createRenderContext",
                  ah_native(vm, "createRenderContext", nat_render_context, (void *)1));
    ah_set_member(vm, m->context, "getRenderContext",
                  ah_native(vm, "getRenderContext", nat_render_context, NULL));
    ah_set_member(vm, m->context, "unloadGpb", ah_native(vm, "unloadGpb", nat_noop, m));
    ah_set_member(vm, m->context, "loadGpb", ah_native(vm, "loadGpb", nat_load_gpb, m));
    ah_set_member(vm, m->context, "existGpbBinary",
                  ah_native(vm, "existGpbBinary", nat_exist_gpb, m));
    ah_set_member(vm, m->context, "translate", ah_native(vm, "translate", nat_translate, m));
    ah_set_member(vm, m->context, "pushPage", ah_native(vm, "pushPage", nat_page, (void *)1));
    ah_set_member(vm, m->context, "startPage", ah_native(vm, "startPage", nat_page, (void *)2));
    ah_set_member(vm, m->context, "closePage", ah_native(vm, "closePage", nat_page, (void *)3));
    ah_set_member(vm, m->context, "pushEvent", ah_native(vm, "pushEvent", nat_push_event, m));
    ah_set_member(vm, m->context, "finish", ah_native(vm, "finish", nat_finish, m));
    /* The context starts with a colour (black) transition: the arcade
       scripts never assign one, yet _start_page's panOutIn fades through
       black between pages in PCSX2 (menu entry, maker <-> showroom). */
    {
        ah_val transition = ah_nil();
        if (nat_transition(vm, NULL, (void *)"MColorTransition", 0, NULL, &transition) == 0)
            ah_set_member(vm, m->context, "transition", transition);
        else
            ah_set_member(vm, m->context, "transition", ah_nil());
    }
    ah_set_member(vm, m->context, "event_mask", ah_int(-1));
    ah_set_member(vm, m->context, "time_after_last_input", ah_float(0));
    ah_set_member(vm, m->context, "flushKeyEvent", ah_native(vm, "flushKeyEvent", nat_noop, m));
    /* Numeric waits keep presentation moving without reentering the VM. */
    ah_set_member(vm, m->context, "sync", ah_native(vm, "sync", nat_sync, m));
    ah_set_member(vm, m->context, "getCommonPage",
                  ah_native(vm, "getCommonPage", nat_common_page, m));
    ah_set_member(vm, m->context, "getPrelightWidget",
                  ah_native(vm, "getPrelightWidget", nat_prelight, m));
    ah_pin(vm, ah_objv(m->context));
}

/* ---------------------------------------------------------------- menu */

menu *menu_open(gt4_vol *vol, const char *project, char *err, size_t err_len)
{
    return menu_open_presented(vol, project, err, err_len, NULL, NULL);
}

/* A mod's car joins the DEALERSHIP's own catalog. The showroom lists what the
   disc's authored catalog says -- `<maker>_carlist` plus its own `<maker>_carnum`
   count, inside the containers the script declares (`NewCarData` for the new-car
   dealers, `CompleteCarData` for the full browse, both read from
   /projects/gtmode/US_carlist.adc the moment the project opens).
 *
 * So a car that is not in those lists can be driven in Arcade but never bought.
   The label is pushed in and the COUNT IS BUMPED -- a script that walks its own
   list by count would otherwise never see the addition. From there the purchase
   path is the game's own: the label resolves through the same merged GENERIC_CAR
   tables, GetPrice reads the row the SDK wrote, and the garage takes it.
 *
 * Only a car whose manifest names a maker is filed: a brand-less car stays out of
   the dealership rather than being invented into a random brand. */
static void mod_cars_into_catalog(menu *m)
{
    static const char *labels[MOON_MOD_MAX], *makers[MOON_MOD_MAX];
    size_t n = moon_mod_added_cars(labels, makers, MOON_MOD_MAX);
    if (!n || !m)
        return;
    static const char *const containers[] = {"NewCarData", "CompleteCarData", "CarData"};
    int filed = 0;
    for (size_t c = 0; c < sizeof containers / sizeof containers[0]; c++) {
        ah_val data;
        if (!ah_get_member(m->vm, m->project, containers[c], &data))
            continue;
        ah_obj *store = ah_as_obj(data, AO_MODULE);
        if (!store)
            continue;
        for (size_t i = 0; i < n; i++) {
            if (!makers[i][0])
                continue;
            char key[192], num_key[192];
            snprintf(key, sizeof key, "%s_carlist", makers[i]);
            snprintf(num_key, sizeof num_key, "%s_carnum", makers[i]);
            ah_val cars;
            if (!ah_get_member(m->vm, store, key, &cars))
                continue;
            ah_obj *list = ah_as_obj(cars, AO_ARRAY);
            if (!list)
                continue;
            int have = 0;
            for (int k = 0; k < ah_array_size(list); k++) {
                const char *e = ah_cstr(ah_array_get(list, k));
                if (e && !strcmp(e, labels[i])) {
                    have = 1;
                    break;
                }
            }
            if (!have) {
                ah_array_push(m->vm, list, ah_str(m->vm, labels[i]));
                filed++;
            }
            ah_set_member(m->vm, store, num_key, ah_int(ah_array_size(list)));
            printf("menu: %s.%s gains \"%s\" (a mod's car), list now %d\n", containers[c], key,
                   labels[i], ah_array_size(list));
        }
    }
    if (!filed)
        printf("menu: a mod's car could not be filed in the dealership catalog "
               "(no maker, or the project declares no new-car list)\n");
}

menu *menu_open_presented(gt4_vol *vol, const char *project, char *err, size_t err_len,
                          void (*progress)(void *), void *user)
{
    menu *m = calloc(1, sizeof *m);
    if (!m)
        return NULL;
    m->vol = vol;
    snprintf(m->project_label, sizeof m->project_label, "%s", project);
    /* MPhotoMapWindow ctor 00192BE0 +94/+B4/+BC. */
    m->photo.view_angle = 70;
    m->photo.aperture = 7;
    m->photo.lightness = 10;
    m->photo.image_scale = 1;
    strcpy(m->photo.operate_mode, "none");
    strcpy(m->photo.color_filter, "none");
    if (!strcmp(project, "photo_shoot"))
        m->photo_apertures_ready =
            gt4_photo_apertures_load(gt4_vol_source(vol), m->photo_apertures);
    m->car_color = -1;
    m->ui = ui_project_load(vol, project, err, err_len);
    if (!m->ui) {
        free(m);
        return NULL;
    }
    ui_set_native_renderer(m->ui, logger_draw, m);
    int n = ui_root_count(m->ui);
    m->entered = calloc((size_t)(n ? n : 1), 1);
    m->loaded = calloc((size_t)(n ? n : 1), 1);
    m->focus = calloc((size_t)(n ? n : 1), sizeof *m->focus);
    m->vm = ah_vm_new();
    ui_set_prepare_presenter(m->ui, progress, user);
    ah_set_progress(m->vm, progress, user);
    if (progress)
        progress(user);
    ah_set_user(m->vm, m);
    ah_set_log(m->vm, log_line, m);
    make_engine(m);
    /* main.game.option exists before project bytecode runs on the PS2.
       quick_tune.ad module construction calls MQuickWork, whose initial
       transmission (00111B50 +1B0..1BC) must see the current preference,
       not make_engine's default followed by a late menu_enter restore. */
    if (gt4_save_active())
        gt4_save_restore(m->vm, game_object(m));
    else
        options_restore(m);

    menu_bind_favorites(m, game_object(m));
    const char *pname = mp_string(ui_project_tree(m->ui), "name");
    m->project = ah_module_new(m->vm, pname ? pname : project, ah_root(m->vm), NULL, NULL);
    ah_set_member(m->vm, ah_root(m->vm), pname ? pname : project, ah_objv(m->project));
    ah_set_member(m->vm, m->project, "load",
                  ah_bind(m->vm, m->project, ah_native(m->vm, "load", nat_project_load, m)));
    for (int i = 0; i < n; i++) {
        bind_tree(m, ui_root_widget(m->ui, i));
        if (progress)
            progress(user);
    }

    char path[256];
    size_t size = 0;
    snprintf(path, sizeof path, "/projects/%s/%s.adc", project, project);
    uint8_t *data = gt4_vol_read(vol, path, &size);
    if (!data) {
        fprintf(stderr, "menu: no script %s, screens only\n", path);
        return m;
    }
    char e[256];
    ah_code *code = ah_load(data, size, e, sizeof e);
    free(data);
    if (!code) {
        fprintf(stderr, "menu: %s: %s\n", path, e);
        return m;
    }
    if (!strcmp(project, "gtmode")) {
        size_t catalog_size = 0;
        uint8_t *catalog = gt4_vol_read(vol, "/projects/gtmode/US_carlist.adc", &catalog_size);
        if (catalog) {
            ah_code *catalog_code = ah_load(catalog, catalog_size, e, sizeof e);
            free(catalog);
            if (catalog_code)
                ah_run_module(m->vm, catalog_code, m->project);
            else
                fprintf(stderr, "menu: GT Mode catalog: %s\n", e);
        }
    }
    if (progress)
        progress(user);
    if (ah_run_script(m->vm, code) < 0)
        fprintf(stderr, "menu: %s stopped with an error\n", path);
    {
        ah_val check;
        if (ah_get_member(m->vm, m->project, "checkGameData", &check))
            ah_set_member(m->vm, m->project, "checkGameData",
                          ah_native(m->vm, "checkGameData", nat_pc_check_profile, m));
    }
    /* the Arcade's car lists are its own CarData.<maker>_carlist (arcade.ad
       getCurrentCarList L2924), filled by the script just run: a mod's car joins them
       as it joins the dealership's */
    if (!strcmp(project, "arcade"))
        mod_cars_into_catalog(m);
    if (!strcmp(project, "photo_shoot")) {
        ah_val ctx = ah_objv(m->context), result;
        if (ah_call_member(m->vm, m->project, "onLoad", 1, &ctx, &result) < 0)
            fprintf(stderr, "menu: photo_shoot project onLoad failed\n");
    }
    if (!strcmp(project, "gtmode")) {
        ah_val used_page_value, used_module_value;
        ah_get_member(m->vm, m->project, "UsedCarRoot", &used_page_value);
        ah_get_member(m->vm, ah_as_obj(used_page_value, AO_MODULE), "UsedCarModule",
                      &used_module_value);
        ah_obj *used_module = ah_as_obj(used_module_value, AO_MODULE);
        ah_module_attach_host(used_module, &project_loader_host);
        /* Native profile data is restored before authored panel construction. */
        ah_obj *game = game_object(m);
        ah_set_member(m->vm, game, "major_menu_widget", ah_str(m->vm, ""));
        ah_set_member(m->vm, game, "major_menu_page", ah_str(m->vm, ""));
        ah_set_member(m->vm, game, "username", ah_str(m->vm, "Player"));
        ah_set_member(m->vm, game, "money", ah_int(10000));
        ah_set_member(m->vm, game, "isNeedSave",
                      ah_native(m->vm, "isNeedSave", nat_save_status, (void *)1));
        ah_obj *garage = submodule(m->vm, game, "garage");
        ah_set_member(m->vm, garage, "hasRidingCar",
                      ah_native(m->vm, "hasRidingCar", nat_empty_favorites, m));
        ah_obj *calendar = submodule(m->vm, game, "calendar"), *date = ah_array_new(m->vm);
        ah_array_push(m->vm, date, ah_int(2005));
        ah_array_push(m->vm, date, ah_int(4));
        ah_array_push(m->vm, date, ah_int(2));
        ah_set_member(m->vm, calendar, "today", ah_objv(date));
        ah_set_member(m->vm, calendar, "getElapsedDate",
                      ah_native(m->vm, "getElapsedDate", nat_empty_favorites, m));
        ah_obj *record = submodule(m->vm, game, "race_record");
        ah_set_member(m->vm, record, "isLicensePassed",
                      ah_native(m->vm, "isLicensePassed", nat_empty_favorites, m));
        ah_val main_, main_menu, unit;
        ah_get_member(m->vm, ah_root(m->vm), "main", &main_);
        ah_get_member(m->vm, ah_as_obj(main_, AO_MODULE), "menu", &main_menu);
        ah_get_member(m->vm, ah_as_obj(main_menu, AO_MODULE), "MUnit", &unit);
        ah_set_member(m->vm, ah_as_obj(unit, AO_MODULE), "IsCalendarTypeDayFirst",
                      ah_native(m->vm, "IsCalendarTypeDayFirst", nat_calendar_day_first, m));
        menu_save_refresh(m);
        /* Project onLoad constructs the shared Common panel from Template.
           It must run before any page's onLoad/onInitialize. */
        ah_val ctx = ah_objv(m->context), result;
        if (ah_call_member(m->vm, m->project, "onLoad", 1, &ctx, &result) < 0)
            fprintf(stderr, "menu: gtmode project onLoad failed\n");
        /* After the project is up: its CarData exists by now, so a mod's car can
           be filed. Doing this before onLoad asked a catalog that was not there. */
        mod_cars_into_catalog(m);
    }
    if (!strcmp(project, "labomode"))
        album_music_bind(m);
    if (!strcmp(project, "slide")) {
        album_music_bind(m);
        jukebox_bind(m);
    }
    if (!strcmp(project, "labomode"))
        ui_set_album_renderer(m->ui, album_draw, m);
    for (int i = 0; i < ui_root_count(m->ui); i++)
        bind_save_buttons(m, ui_root_widget(m->ui, i));
    ah_set_member(m->vm, m->project, "autoSave", ah_native(m->vm, "autoSave", nat_save_auto, m));
    if (!strcmp(project, "gtmode"))
        ah_set_member(m->vm, m->project, "autoSaveImpl",
                      ah_native_refs(m->vm, "autoSaveImpl", nat_pc_autosave, m, 1u << 3));
    return m;
}

void menu_close(menu *m)
{
    if (!m)
        return;
    gt4_sound_close(m->sound);
    gt4_disc_replay_close(m->disc_replays);
    free(m->race_labels);
    free(m->race_db);
    free(m->car_labels);
    free(m->car_db);
    free(m->car_names);
    free(m->car_strings);
    free(m->car_links);
    free(m->car_variants);
    gt4_arcade_close(m->arcade);
    for (int i = 0; i < 6; i++)
        free(m->entry_car[i]);
    free(m->course_db);
    free(m->course_labels);
    ah_vm_free(m->vm); /* before the widgets its finalizers look at */
    ui_project_free(m->ui);
    free(m->watchers);
    free(m->entered);
    free(m->loaded);
    free(m->focus);
    free(m);
}

ui_project *menu_ui(menu *m)
{
    return m->ui;
}

static int movie_finish_tree(menu *m, ui_widget *w)
{
    if (!w)
        return 0;
    if (!strcmp(w->type, "MovieFace") && w->script) {
        ah_val fn, ctx = ah_objv(m->context), result;
        if (ah_get_member(m->vm, w->script, "onMovieFinish", &fn)) {
            ah_call_member(m->vm, w->script, "onMovieFinish", 1, &ctx, &result);
            apply_pages(m);
            return 1;
        }
    }
    for (int i = 0; i < w->nkids; i++)
        if (movie_finish_tree(m, w->kids[i]))
            return 1;
    return 0;
}

int menu_movie_finished(menu *m)
{
    if (!m || menu_active_root(m) < 0)
        return 0;
    return movie_finish_tree(m, ui_root_widget(m->ui, menu_active_root(m)));
}

/* The widgets' own onLoad, below the page's: gtmode.ad's TuningRoot::Pane
   defines its categories' parts_types there (L7132..7158) and only its
   onInitialize is called by the script (L6959), so the engine runs the
   loads. Pre-order, a widget before its children. */
static void load_widgets(menu *m, ui_widget *w, ah_val context)
{
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *k = w->kids[i];
        ah_val fn, result;
        if (k->script && ah_get_own_member(m->vm, k->script, "onLoad", &fn) && fn.t != AV_NIL &&
            ah_call_member(m->vm, k->script, "onLoad", 1, &context, &result) < 0)
            fprintf(stderr, "menu: %s.onLoad stopped with an error\n", k->name ? k->name : "?");
        load_widgets(m, k, context);
    }
}
static void load_page_data(menu *m, int root, ah_val context)
{
    if (!m->loaded || m->loaded[root])
        return;
    m->loaded[root] = 1;
    ui_widget *page = ui_root_widget(m->ui, root);
    ah_val result;
    ah_obj *module = widget_module(m, page);
    if (ah_call_member(m->vm, module, "onLoadInitialize", 1, &context, &result) < 0)
        fprintf(stderr, "menu: %s.onLoadInitialize stopped with an error\n", page->name);
    if (ah_call_member(m->vm, module, "onLoad", 1, &context, &result) < 0)
        fprintf(stderr, "menu: %s.onLoad stopped with an error\n", page->name);
    load_widgets(m, page, context);
}

void menu_project_entry(menu *m)
{
    if(!m) return;
    /* Original project onLoad fragments: logger.ad L20..25, setting.ad
       L30..33 and the corresponding retail project scripts. Quick race
       projects construct this transition but do not call panIn here. */
    static const char *const reveal[]={"arcade","gtmode","boot","setting","event",
        "labomode","photo_shoot","logger","slide","photo_save"};
    ui_fade(m->ui,0,0,0);
    m->pending_fade_in=0;
    m->project_fade_prepared=1;
    ah_val transition=ah_nil(),result;
    if(nat_transition(m->vm,NULL,(void *)"MColorTransition",0,NULL,&transition)<0) return;
    ah_set_member(m->vm,m->context,"transition",transition);
    for(unsigned i=0;i<sizeof reveal/sizeof *reveal;i++)
        if(!strcmp(m->project_label,reveal[i])) {
            ah_val context=ah_objv(m->context);
            /* 0022F6A0 pauses context updates while startPage prepares the
               destination. Hold black until apply_pages commits that entry,
               rather than consuming the reveal in asset-loading Syncs. */
            m->initializing++;
            ah_call_member(m->vm,transition.u.o,"panIn",1,&context,&result);
            m->initializing--;
            break;
        }
}

void menu_enter(menu *m, int root)
{
    if (root < 0 || root >= ui_root_count(m->ui))
        return;
    if (!gt4_save_active())
        options_restore(m);
    if (!m->page_transition_depth && strcmp(ah_module_name(m->project), "SettingProject"))
        menu_save_refresh(m);
    /* 00180AA8 +0x78..+0xA4: refresh the normalized getter after restoring
       saved raw preferences; quick-arcade.ad reads it during onInitialize. */
    ah_val option;
    if (ah_get_member(m->vm, game_object(m), "option", &option))
        ah_set_member(m->vm, option.u.o, "menu_bgm_volume",
                      ah_float((float)menu_option_number(m, "menu_bgm_volume_raw", 120) / 128.f));
    /* An explicit first screen (including the viewer's debug page argument)
       supersedes the project's queued initial startPage. */
    if (!m->depth)
        if(!m->project_fade_prepared) menu_project_entry(m);
    if (!m->depth)
        m->pending_kind = 0;
    if (!m->depth || menu_active_root(m) != root) {
        while (m->depth)
            finalize_page(m, m->pages[--m->depth]);
        m->pages[0] = root;
        m->depth = 1;
    }
    m->idle_frames = 0;
    /* Options has no page onStart panIn. Initial and cached project entries
       still need the same reveal as its authored start_page callbacks. */
    if (!strcmp(ah_module_name(m->project), "OptionProject") && m->pending_fade_in == 0 &&
        ui_fade_remaining(m->ui) == 0) {
        ah_val transition, period;
        if (ah_get_member(m->vm, m->context, "transition", &transition) &&
            ah_get_member(m->vm, ah_as_obj(transition, AO_MODULE), "period", &period) &&
            ah_num(period) > 0) {
            ui_fade(m->ui, 1, 1, 0);
            m->pending_fade_in = (float)ah_num(period);
        }
    }
    /* A fresh opening resets MovieRoot's authored end flag even on a cached VM. */
    if (!strcmp(ui_root_name(m->ui, root), "MovieRoot"))
        m->entered[root] = 0;
    request_music(m, mp_string(ui_root_widget(m->ui, root)->src, "bgm"));
    if (m->entered[root]) {
        ah_val ctx = ah_objv(m->context), result;
        ah_call_member(m->vm, widget_module(m, ui_root_widget(m->ui, root)), "enter", 1, &ctx,
                       &result);
        return;
    }
    m->entered[root] = 1;
    m->initializing++;
    ui_widget *w = ui_root_widget(m->ui, root);
    ah_val ctx = ah_objv(m->context), r;
    /* onLoad(context) prepares a page's data once (MakerRoot: nation flags
       and car counts), before its first onInitialize. */
    /* Both authored tab totals are read by either selection page. Prepare
       their data together without entering or focusing the other page. */
    if (w->name && (!strcmp(w->name, "MakerRoot") || !strcmp(w->name, "HistoryRoot")))
        for (int i = 0; i < ui_root_count(m->ui); i++) {
            const char *name = ui_root_name(m->ui, i);
            if (!strcmp(name, "MakerRoot") || !strcmp(name, "HistoryRoot"))
                load_page_data(m, i, ctx);
        }
    request_music(m, mp_string(w->src, "bgm"));
    /* labomode's dialogs are opened by their open() (OptionReplayDialog,
       OptionAlbumDialog), which moves the Pane's actor before pushPage:
       their onLoad (setMoveActor) has run by then. A: when the game runs
       them is not read; with the first page here. */
    if (!strcmp(ah_module_name(m->project), "LaboModeProject"))
        for (int i = 0; i < ui_root_count(m->ui); i++) {
            const char *name = ui_root_name(m->ui, i);
            size_t n = strlen(name);
            if (i != root && n > 6 && !strcmp(name + n - 6, "Dialog"))
                load_page_data(m, i, ctx);
        }
    /* gtmode.ad GetCourseDialog.onLoad L15577..15580 creates the actors
       used by open_motion L15585..15592, before pushPage at L15593.
       Prepare this resource before TopRoot can deliver a dated course.
       A: original project-wide loading phase is not yet reproduced. */
    if (!strcmp(ah_module_name(m->project), "GTmodeProject"))
        for (int i = 0; i < ui_root_count(m->ui); i++)
            if (!strcmp(ui_root_name(m->ui, i), "GetCourseDialog"))
                load_page_data(m, i, ctx);
    load_page_data(m, root, ctx);
    if (ah_call_member(m->vm, widget_module(m, w), "onInitialize", 1, &ctx, &r) < 0)
        fprintf(stderr, "menu: %s.onInitialize stopped with an error\n", w->name ? w->name : "?");
    pc_menu_entries(m, w);
    ah_call_member(m->vm, widget_module(m, w), "enter", 1, &ctx, &r);
    m->initializing--;
    /* Legacy onEnter runs after page initialization so notices created after
       setFocus are available. Modern onFocusEnter retains its existing timing. */
    if (m->focus[root]) {
        ah_obj *focus = widget_module(m, m->focus[root]);
        ah_val method;
        if (!ah_get_member(m->vm, focus, "onFocusEnter", &method))
            ah_call_member(m->vm, focus, "onEnter", 1, &ctx, &r);
    }
}

void menu_advance(menu *m, double seconds)
{
    if (!m || !isfinite(seconds) || seconds <= 0)
        return;
    if (seconds > 0.25)
        seconds = 0.25;
    menu_animate(m, (float)seconds);
    m->tick_remainder += seconds;
    while (m->tick_remainder + 1e-10 >= 1.0 / 60) {
        m->tick_remainder -= 1.0 / 60;
        menu_tick(m);
    }
}

/* a list's script callbacks (labomode.ad): visible_callback(context,
   indices) for the lists filled since the last tick, then
   update_callback(context, indices) for items still asking */
static void item_callbacks(menu *m)
{
    for (int i = 0; i < m->nitem_lists; i++) {
        ui_widget *w = m->item_lists[i];
        ah_val fn, r, args[2];
        if (!ah_get_member(m->vm, widget_module(m, w), "visible_callback", &fn) || fn.t == AV_NIL)
            continue;
        ah_obj *index = ah_array_new(m->vm);
        for (int k = 0; k < w->nkids; k++)
            ah_array_push(m->vm, index, ah_int(k));
        args[0] = ah_objv(m->context);
        args[1] = ah_objv(index);
        ah_call(m->vm, fn, 2, args, &r);
    }
    m->nitem_lists = 0;
    for (int pass = 0; pass < 1 && m->nitem_updates; pass++) {
        ui_widget *list = m->item_updates[0].list;
        ah_val fn, r, args[2];
        ah_obj *index = ah_array_new(m->vm);
        for (int k = 0; k < m->nitem_updates; k++)
            if (m->item_updates[k].list == list)
                ah_array_push(m->vm, index, ah_int(m->item_updates[k].index));
        if (!ah_get_member(m->vm, widget_module(m, list), "update_callback", &fn) ||
            fn.t == AV_NIL) {
            m->nitem_updates = 0;
            break;
        }
        args[0] = ah_objv(m->context);
        args[1] = ah_objv(index);
        ah_call(m->vm, fn, 2, args, &r);
    }
}

void menu_tick(menu *m)
{
    if (m)
        album_tick(m);
    logger_tick(m);
    ah_set_member(m->vm, m->context, "time_after_last_input", ah_float(++m->idle_frames / 60.0));
    item_callbacks(m);
    for (int i = 0; i < m->nwatchers; i++) {
        watcher *wt = &m->watchers[i];
        ah_val interval, fn, ctx, r;
        ah_get_member(m->vm, wt->obj, "interval", &interval);
        int every = (int)ah_num(interval);
        if (++wt->count < (every > 0 ? every : 1))
            continue;
        wt->count = 0;
        ah_get_member(m->vm, wt->obj, "func", &fn);
        ah_get_member(m->vm, wt->obj, "context", &ctx);
        ah_call(m->vm, fn, 1, &ctx, &r);
    }
    /* Bound work per frame; callbacks may append more events. Keep the
       current event pinned until the call returns, including failures. */
    for (int budget = 0; m->nevents && budget < 64; budget++) {
        ah_obj *e = m->events[0];
        memmove(m->events, m->events + 1, (size_t)--m->nevents * sizeof *m->events);
        ah_val av, fn, target, method, result;
        ah_get_member(m->vm, e, "arguments", &av);
        ah_obj *a = ah_as_obj(av, AO_ARRAY);
        int n = ah_array_size(a);
        ah_val *args = calloc((size_t)(n ? n : 1), sizeof *args);
        if (args) {
            for (int i = 0; i < n; i++)
                args[i] = ah_array_get(a, i);
            if (ah_get_member(m->vm, e, "function", &fn))
                ah_call(m->vm, fn, n, args, &result);
            else if (ah_get_member(m->vm, e, "target", &target) &&
                     ah_get_member(m->vm, e, "method", &method) && ah_cstr(method))
                ah_call_member(m->vm, ah_as_obj(target, AO_MODULE), ah_cstr(method), n, args,
                               &result);
            free(args);
        }
        ah_unpin(m->vm, ah_objv(e));
        apply_pages(m);
    }
    apply_pages(m);
    ah_gc(m->vm);
}

int menu_active_root(const menu *m)
{
    return m && m->depth ? m->pages[m->depth - 1] : -1;
}
int menu_page_depth(const menu *m)
{
    return m ? m->depth : 0;
}
int menu_page_at(const menu *m, int i)
{
    return m && i >= 0 && i < m->depth ? m->pages[i] : -1;
}
const char *menu_focus_name(const menu *m)
{
    int root = menu_active_root(m);
    ui_widget *w = root >= 0 ? m->focus[root] : NULL;
    return w && w->name ? w->name : "";
}

static int event_call(menu *m, ui_widget *handler, ui_widget *target, const char *method, int key,
                      int argc, ah_val *result)
{
    ah_obj *e = ah_module_new(m->vm, "event", NULL, NULL, NULL);
    ah_set_member(m->vm, e, "widget", maybe_module(m, target));
    ah_set_member(m->vm, e, "keysym", ah_int(key));
    /* the first controller's (option.ad's secret command takes port 1 only) */
    ah_set_member(m->vm, e, "port", ah_int(0));
    ah_val args[3] = {ah_objv(m->context), ah_objv(e), maybe_module(m, target)};
    return ah_call_member(m->vm, widget_module(m, handler), method, argc, args, result);
}

static int available(ui_widget *w)
{
    for (ui_widget *p = w; p; p = p->parent)
        if (!p->visible || p->insensitive)
            return 0;
    return w != NULL;
}

/* The page's help line follows focus through its TOOLTIP|<name> text;
   onFocusEnter may replace it (tip.value) right after. */
static void set_tooltip(menu *m, int root, const ui_widget *w)
{
    char key[160];
    const char *authored = w ? mp_string(w->src, "tooltip_key") : NULL;
    /* /projects/quick-arcade/quick-arcade.mproject Buttons.*.tooltip_key names
       /projects/quick-arcade/US/rtext.rt2's TOOLTIP|icon_text_* entries. */
    snprintf(key, sizeof key, "TOOLTIP|%s", authored ? authored : w && w->name ? w->name : "");
    const char *text = ui_translate(m->ui, root, NULL, key);
    if (lan_blocked(m, (ui_widget *)w)) {
        /* P: the blocked LAN entries say so: their title, then "Coming soon" */
        char soon[160];
        const char *colon = text ? strchr(text, ':') : NULL;
        if (colon)
            snprintf(soon, sizeof soon, "%.*s: Coming soon", (int)(colon - text), text);
        else
            snprintf(soon, sizeof soon, "Coming soon");
        ui_set_tooltips(ui_root_widget(m->ui, root), soon);
        return;
    }
    /* Original: arcade.ad L4185/L4125 supplies the car specification line.
       Missing tooltip_key is not a request to erase a scripted ToolTipFace;
       the authored page uses remove() when it needs to clear that line. */
    if (text || !w)
        ui_set_tooltips(ui_root_widget(m->ui, root), text);
}

/* Scrolled lists ("clip" > "space", e.g. MakerRoot's logo map). Native
   approximation of the PS2 behaviour reported by the user: the view moves
   only when the focused item comes within one item of the edge, then eases
   there (ui_animate) at the cursor's rate. The original MScrollWindow code
   has not been traced yet. snap: jump at once (ScrollWindow.warp). */
static void scroll_to(ui_widget *w, int snap)
{
    for (ui_widget *space = w->parent; space && space->parent; space = space->parent) {
        ui_widget *clip = space->parent;
        if (!space->name || !clip->name || strcmp(space->name, "space") ||
            strcmp(clip->name, "clip"))
            continue;
        float x = w->x, y = w->y;
        for (ui_widget *p = w->parent; p && p != space; p = p->parent) {
            x += p->x;
            y += p->y;
        }
        float tx = space->scroll_set ? space->scroll_tx : space->x,
              ty = space->scroll_set ? space->scroll_ty : space->y;
        /* one row/column of margin: the spacing to the nearest neighbour */
        float px = w->w, py = w->h;
        for (int i = 0; w->parent && i < w->parent->nkids; i++) {
            const ui_widget *o = w->parent->kids[i];
            if (o == w || !o->visible)
                continue;
            float dx = fabsf((o->x + o->w * .5f) - (w->x + w->w * .5f)),
                  dy = fabsf((o->y + o->h * .5f) - (w->y + w->h * .5f));
            if (dy < 4 && dx > 4 && (px == w->w || dx < px))
                px = dx;
            if (dx < 4 && dy > 4 && (py == w->h || dy < py))
                py = dy;
        }
        float mx = fminf(px, fmaxf(0, (clip->w - w->w) * .5f)),
              my = fminf(py, fmaxf(0, (clip->h - w->h) * .5f));
        if (x + tx < mx)
            tx = mx - x;
        if (x + tx + w->w > clip->w - mx)
            tx = clip->w - mx - x - w->w;
        if (y + ty < my)
            ty = my - y;
        if (y + ty + w->h > clip->h - my)
            ty = clip->h - my - y - w->h;
        /* The GT map keeps the selected 3D landmark at the view centre.
           List/card scroll windows retain their visibility-margin behavior. */
        ui_widget *model = ui_widget_child(w, "bg");
        if (model && !strcmp(model->type, "ModelFace")) {
            tx = (clip->w - w->w) * .5f - x;
            ty = (clip->h - w->h) * .5f - y;
        }
        /* never scroll past the content */
        tx = space->w > clip->w ? fminf(0, fmaxf(clip->w - space->w, tx)) : 0;
        ty = space->h > clip->h ? fminf(0, fmaxf(clip->h - space->h, ty)) : 0;
        space->scroll_tx = tx;
        space->scroll_ty = ty;
        space->scroll_set = 1;
        if (snap) {
            space->x = tx;
            space->y = ty;
        }
    }
}

static void set_focus(menu *m, ui_widget *w)
{
    int root = menu_active_root(m);
    /* arcade.ad SplitRoot.onInitialize calls setFocus(RaceStart) before
       assigning RaceStart.visible = decided1p && decided2p. Retain the
       authored focus during initialization; ensure_focus validates it after. */
    if (m->hover_dispatch || root < 0 || (w && (w->root != root || (!m->initializing && !available(w)))))
        return;
    ui_cursor_select(m->ui, w);
    ui_widget *old = m->focus[root];
    /* The native modal-close adapter holds the outgoing cursor only while
       it owns focus. A new authored setFocus releases that old hold; it
       must not strand the arrow when SplitRoot switches P1/P2 Select.
       A: adapter state, not a new original cursor timing constant. */
    if (old != w)
        ui_cursor_hold(m->ui, 0);
    if (old == w) {
        /* A page may store its focus before activation, or retain it while
           covered by a modal page. Restore the visual state without events. */
        if (w)
            w->focused = 1;
        /* Refocusing (e.g. after tip.remove() in onInitialize) restores the
           widget's help line without repeating focus events. */
        /* Only a translated help line is restored; a script-made one
           ("Hyundai  2 cars") stays. */
        char key[160];
        const char *authored = w ? mp_string(w->src, "tooltip_key") : NULL;
        snprintf(key, sizeof key, "TOOLTIP|%s", authored ? authored : w && w->name ? w->name : "");
        const char *tip = w ? ui_translate(m->ui, root, NULL, key) : NULL;
        if (tip)
            ui_set_tooltips(ui_root_widget(m->ui, root), tip);
        return;
    }
    ah_val result;
    m->focus[root] = w;
    if (old) {
        old->option_open = 0;
        old->focused = 0;
        if (!event_call(m, old, old, "onFocusLeave", 0, 2, &result))
            event_call(m, old, old, "onLeave", 0, 1, &result);
    }
    if (w) {
        w->focused = 1;
        scroll_to(w, 0);
        if (w->parent && !strcmp(w->parent->type, "SelectBox")) {
            ui_widget *list = w->parent;
            int previous = list->focus_index;
            list->focus_index = sibling_index(w);
            /* Original: func_002B29C0 +0x9CC..0xA58: selection changes
               dispatch the registered focus_callback (arcade.ad L4077). */
            if (previous != list->focus_index) {
                ah_val callback;
                if (ah_get_member(m->vm, widget_module(m, list), "focus_callback", &callback) &&
                    callback.t != AV_NIL) {
                    ah_val args[] = {ah_objv(m->context), ah_int(list->focus_index), ah_int(previous)};
                    ah_call(m->vm, callback, 3, args, &result);
                }
            }
        }
        set_tooltip(m, root, w);
        if (!event_call(m, w, w, "onFocusEnter", 0, 2, &result) && !m->initializing)
            event_call(m, w, w, "onEnter", 0, 1, &result);
    }
}

/* Candidates come from widget semantics, never from hard-coded menu labels. */
static void candidates(ui_widget *w, ui_widget **items, int *n)
{
    if (!available(w) || *n >= 512)
        return;
    if (!strcmp(w->type, "SelectBox")) {
        for (int i = 0; i < w->nkids && *n < 512; i++)
            if (available(w->kids[i]))
                items[(*n)++] = w->kids[i];
        return;
    }
    if (!strcmp(w->type, "Button") || (!strcmp(w->type, "OptionMenu") && ui_option_count(w)) ||
        !strcmp(w->type, "SliderBar") || !strcmp(w->type, "ScaleBar") ||
        mp_number(w->src, "can_focus", 0)) {
        items[(*n)++] = w;
        return;
    }
    for (int i = 0; i < w->nkids; i++)
        candidates(w->kids[i], items, n);
}

/* P: settings rows hover as a unit, including their authored label. */
static int pointer_hit(menu *m, ui_widget *w, float x, float y)
{
    /* Main-menu hit bounds come from its painted row. Do not extend them
       to a screen-wide invisible band: blank side space is not a target. */
    if(!strcmp(m->project_label,"arcade") && w->parent && w->parent->parent &&
       !strcmp(w->parent->name,"SelectBox") &&
       !strcmp(w->parent->parent->name,"StartScroll")) {
        return ui_pointer_contains(m->ui,w,x,y);
    }
    if (ui_pointer_contains(m->ui,w,x,y)) return 1;
    if (strcmp(w->type,"OptionMenu") && strcmp(w->type,"SliderBar") &&
        strcmp(w->type,"ScaleBar")) return 0;
    ui_widget *label = w->parent ? ui_widget_child(w->parent,"label") : NULL;
    return label && ui_pointer_contains(m->ui,label,x,y);
}
static void pointer_wheel(menu *m, ui_widget *hit, float x, float y, int wheel)
{
    int root=menu_active_root(m);
    ui_widget *w=m->focus[root];
    int popup=w && !strcmp(w->type,"OptionMenu") && w->option_open;
    int bar=hit && (!strcmp(hit->type,"SliderBar") || !strcmp(hit->type,"ScaleBar"));
    ui_widget *page = ui_root_widget(m->ui, root);
    ui_widget *pane = ui_widget_child(page, "Pane");
    ui_widget *box = pane ? ui_widget_child(pane, "VBox") : NULL;
    ui_widget *regulation = box ? ui_widget_child(box, "Regulation") : NULL;
    int authored_scroll = !strcmp(ui_root_name(m->ui, root), "EventInfoDialog") &&
                          regulation && ui_pointer_contains(m->ui, regulation, x, y);
    if(!popup && !bar && !authored_scroll) {
        ui_pointer_scroll(m->ui,root,x,y,wheel);
        return;
    }
    /* Original: EventInfoDialog.onKeyPress instructions 27..116: move the
       authored actor by 40 units and clamp to the regulation viewport. */
    if(bar) set_focus(m,hit);
    int steps=wheel>0 ? wheel : -wheel;
    if(steps>32) steps=32;
    for(int i=0;i<steps && menu_active_root(m)==root;i++)
        menu_send_input(m,bar ? (wheel>0 ? MENU_RIGHT : MENU_LEFT) :
                               (wheel>0 ? MENU_UP : MENU_DOWN));
}

void menu_edge_pan(menu *m, int enabled, float seconds)
{
    float x,y;
    if(!m || !enabled || seconds<=0 || !ui_pointer_position(m->ui,&x,&y) ||
       strcmp(m->project_label,"gtmode")) return;
    int root=menu_active_root(m);
    if(root<0 || strcmp(ui_root_name(m->ui,root),"TopRoot")) return;
    ui_widget *items[512], *space=NULL;
    int n=0;
    candidates(ui_root_widget(m->ui,root),items,&n);
    for(int i=0;i<n && !space;i++) {
        ui_widget *model=ui_widget_child(items[i],"bg");
        if(!model || strcmp(model->type,"ModelFace")) continue;
        for(ui_widget *a=items[i]->parent;a && a->parent;a=a->parent)
            if(!strcmp(a->name,"space") && !strcmp(a->parent->name,"clip")) {space=a;break;}
    }
    if(!space) return;
    ui_widget *clip=space->parent;
    /* P: edge panning accepts the whole screen, including authored
       tooltip/footer bands. Icon hit testing retains its map clipping. */
    if(!ui_pointer_contains(m->ui,ui_root_widget(m->ui,root),x,y)) return;
    float *r=clip->pointer_rect;
    float values[2]={space->scroll_set?space->scroll_tx:space->x,
                     space->scroll_set?space->scroll_ty:space->y};
    float position[2]={fminf(clip->w,fmaxf(0,x-r[0])),fminf(clip->h,fmaxf(0,y-r[1]))},
          sizes[2]={clip->w,clip->h}, total[2]={space->w,space->h};
    for(int axis=0;axis<2;axis++) {
        /* P: edge zone is 10% of the viewport; full pressure pans 1.5
           viewports per second. Bounds come from the authored clip/space. */
        float edge=sizes[axis]*.1f, v=position[axis]<edge ? (edge-position[axis])/edge :
            position[axis]>sizes[axis]-edge ? (sizes[axis]-edge-position[axis])/edge : 0;
        values[axis]=fminf(0,fmaxf(fminf(0,sizes[axis]-total[axis]),values[axis]+v*sizes[axis]*1.5f*fminf(seconds,.1f)));
    }
    space->scroll_tx=values[0];space->scroll_ty=values[1];space->scroll_set=1;
}
static void pointer_set_hover(menu *m, ui_widget *hit)
{
    ui_pointer_hover(m->ui,hit);
    if(m->pointer_hover==hit) return;
    ui_widget *old=m->pointer_hover;
    m->pointer_hover=hit;
    ah_val result;
    m->hover_dispatch=1;
    /* P: reuse authored prelight animations (including rotating map icons)
       without setFocus, focus_index updates or scroll_to. */
    if(old && !event_call(m,old,old,"onFocusLeave",0,2,&result))
        event_call(m,old,old,"onLeave",0,1,&result);
    if(hit && !event_call(m,hit,hit,"onFocusEnter",0,2,&result))
        event_call(m,hit,hit,"onEnter",0,1,&result);
    if(hit && hit->parent && (!strcmp(hit->parent->type,"SelectBox") || !strcmp(hit->parent->type,"ListBox"))) {
        ui_widget *list=hit->parent;
        int index=sibling_index(hit), previous=list->focus_index;
        ah_val callback;
        if(index!=previous) {
            /* P: preview reads the hovered row during its authored callbacks,
               then restore selection so motion alone never scrolls the strip. */
            list->focus_index=index;
            if(ah_get_member(m->vm,widget_module(m,list),"focus_callback",&callback) && callback.t!=AV_NIL) {
                ah_val args[]={ah_objv(m->context),ah_int(index),ah_int(previous)};
                ah_call(m->vm,callback,3,args,&result);
            }
            ah_val args[]={ah_objv(m->context),ah_int(index)};
            ah_call_member(m->vm,widget_module(m,list),"onValueChanged",2,args,&result);
            ui_widget *chip = ui_widget_child(hit,"chip");
            int paint = chip && !strcmp(chip->type,"ColorChipFace");
            int dealer = paint || (!strcmp(m->project_label, "gtmode") &&
                         !strcmp(ui_root_name(m->ui, hit->root), "BuyCarRoot") &&
                         list->parent && (!strcmp(list->parent->name, "CarScroll") ||
                                          !strcmp(list->parent->name, "ColScroll")));
            if (dealer) {
                /* P: keep the authored preselection for highlighting/activation,
                   while pointer edge navigation owns the viewport position. */
                if (!list->desktop_scrolled) {
                    list->desktop_offset = list->strip_offset;
                    list->desktop_scrolled = 1;
                }
            } else
                list->focus_index=previous;
        }
    }
    m->hover_dispatch=0;
    if(hit) {
        set_tooltip(m,menu_active_root(m),hit);
        if (m->pointer_sound_delay<=0) {
            menu_play_effect(m,"cursor");
            m->pointer_sound_delay=.1f;
        }
    }
}
static void pointer_send_input(menu *m, menu_input input)
{
    int previous = m->pointer_dispatch;
    m->pointer_dispatch = 1;
    menu_send_input(m,input);
    m->pointer_dispatch = previous;
}
/* P: direct manipulation maps into the authored integer range/callback. */
static int pointer_adjust_bar(menu *m,ui_widget *bar,float x,float y)
{
    if(!bar || (strcmp(bar->type,"SliderBar") && strcmp(bar->type,"ScaleBar")) ||
       (!strcmp(bar->type,"ScaleBar") && !bar->slider_range_set)) return 0;
    double range[2]={0,100};mp_numbers(bar->src,"range",range,2);
    if(bar->slider_range_set) {range[0]=bar->slider_min;range[1]=bar->slider_max;}
    int axis=(int)mp_number(bar->src,"direction",0)==1;
    float *r=bar->pointer_rect;
    float origin=r[axis],extent=r[axis+2];
    ui_widget *popup=ui_widget_child(bar,"popup");
    ui_widget *pinch=popup?ui_widget_child(popup,"pinch"):NULL;
    if(pinch) {
        /* Same knob travel as ui.c: ratio*(popup size - pinch size). */
        float knob=axis?pinch->h:pinch->w;
        origin+=(axis?popup->y:popup->x)+knob*.5f;
        extent=(axis?popup->h:popup->w)-knob;
    }
    float t=extent>0 ? ((axis?y:x)-origin)/extent : 0;
    int old=bar->focus_index;
    assign_index(bar,(int)lround(range[0]+fminf(1,fmaxf(0,t))*(range[1]-range[0])));
    if(old!=bar->focus_index) {
        ah_val args[]={ah_objv(m->context),ah_int(bar->focus_index)},result;
        ah_call_member(m->vm,widget_module(m,bar),"onValueChanged",2,args,&result);
        if(m->pointer_sound_delay<=0) {menu_play_effect(m,"param");m->pointer_sound_delay=.1f;}
    }
    return 1;
}
int menu_pointer(menu *m, int enabled, float x, float y, int action, int wheel)
{
    if (!m) return 0;
    ui_pointer(m->ui, enabled, x, y);
    if (!enabled) {m->pointer_drag=NULL;return 0;}
    int root = menu_active_root(m);
    if (root < 0) return 0;
    if(m->pointer_drag) {
        ui_widget *bar=m->pointer_drag;
        if(bar->root!=root || !available(bar)) m->pointer_drag=NULL;
        else if(action==4 || action==1) {
            pointer_adjust_bar(m,bar,x,y);
            if(action==1) m->pointer_drag=NULL;
            return 1;
        }
    }
    ui_widget *items[512], *hit = NULL;
    int n = 0;
    candidates(ui_root_widget(m->ui, root), items, &n);
    for (int i = 0; i < n; i++)
        if (pointer_hit(m, items[i], x, y)) hit = items[i];
    /* P: a ListBox owns controller focus, but each painted card is a
       separate mouse target. Its original value callback drives previews. */
    if(hit && !strcmp(hit->type,"ListBox")) {
        ui_widget *list=hit;
        for(int i=0;i<list->nkids;i++)
            if(available(list->kids[i]) && ui_pointer_contains(m->ui,list->kids[i],x,y)) hit=list->kids[i];
    }
    /* Wheel owns navigation before hover. A stationary mouse must not reset
       focus/pending selection before every detent, including popup rows. */
    if (wheel) { pointer_wheel(m,hit,x,y,wheel); return 1; }
    ui_widget *focused = m->focus[root];
    if (focused && !strcmp(focused->type, "OptionMenu") && focused->option_open) {
        for (int i = 0; i < ui_option_count(focused); i++) {
            ui_widget *item = ui_option_item(focused, i);
            if (available(item) && ui_pointer_contains(m->ui, item, x, y)) {
                ui_pointer_hover(m->ui,item);
                if (action == 1) {
                    focused->option_pending = i;
                    pointer_send_input(m, MENU_ACCEPT);
                    ui_pointer(m->ui, 1, x, y);
                }
                return 1;
            }
        }
        if (action == 1 || action == 2) pointer_send_input(m, MENU_BACK);
        ui_pointer(m->ui, 1, x, y);
        return 1;
    }
    /* P: hover is presentation only. In particular, SelectBox focus must
       not follow mouse motion: its focus callback scrolls the carrousel. */
    pointer_set_hover(m,hit);
    if(hit && (action==1 || action==3) &&
       ((!strcmp(hit->type,"SliderBar")) ||
        (!strcmp(hit->type,"ScaleBar") && hit->slider_range_set))) {
        if(hit!=m->focus[root]) set_focus(m,hit);
        if(action==3) m->pointer_drag=hit;
        pointer_adjust_bar(m,hit,x,y);
        return 1;
    }
    if(action==3 || action==4) return hit!=NULL;
    if(hit && action==1 && hit->parent && !strcmp(hit->parent->type,"ListBox")) {
        ui_widget *list=hit->parent;
        list->focus_index=sibling_index(hit);
        ah_val args[]={ah_objv(m->context),ah_int(list->focus_index)},result;
        ah_call_member(m->vm,widget_module(m,list),"onValueChanged",2,args,&result);
        hit=list;
    }
    if (hit && action == 1 && hit != m->focus[root]) set_focus(m,hit);
    if (action == 2) pointer_send_input(m, MENU_BACK);
    else if (hit && action == 1) pointer_send_input(m, MENU_ACCEPT);
    ui_pointer(m->ui, 1, x, y);
    return hit != NULL || action == 2;
}

static ui_widget *touch_hit(menu *m, float x, float y)
{
    int root = menu_active_root(m);
    if (root < 0)
        return NULL;
    ui_widget *focused = m->focus[root];
    if (focused && !strcmp(focused->type, "OptionMenu") && focused->option_open) {
        for (int i = 0; i < ui_option_count(focused); i++) {
            ui_widget *item = ui_option_item(focused, i);
            if (available(item) && ui_pointer_contains(m->ui, item, x, y))
                return item;
        }
        return NULL;
    }
    ui_widget *items[512], *hit = NULL;
    int n = 0;
    candidates(ui_root_widget(m->ui, root), items, &n);
    for (int i = 0; i < n; i++)
        if (pointer_hit(m, items[i], x, y))
            hit = items[i];
    if (hit && !strcmp(hit->type, "ListBox"))
        for (int i = 0; i < hit->nkids; i++)
            if (available(hit->kids[i]) && ui_pointer_contains(m->ui, hit->kids[i], x, y))
                return hit->kids[i];
    return hit;
}
int menu_touch(menu *m, int enabled, int phase, long long id, float x, float y)
{
    if (!m)
        return 0;
    int root = menu_active_root(m);
    if (!enabled || phase == 4 || root != m->touch_root) {
        m->touch_active = 0;
        m->touch_target = m->touch_selected = NULL;
        ui_touch_preview(m->ui, NULL);
        ui_touch_press(m->ui, NULL);
        m->touch_root = root;
        menu_pointer(m, 0, 0, 0, 0, 0);
        if (!enabled || phase == 4 || phase != 1)
            return 1;
    }
    if (phase == 1 && !m->touch_active) {
        ui_touch_mode(m->ui,1);
        m->touch_active = 1;
        m->touch_id = id;
        m->touch_root = root;
        m->touch_start_x = m->touch_last_x = x;
        m->touch_start_y = m->touch_last_y = y;
        m->touch_dragging = 0;
        m->touch_target = touch_hit(m, x, y);
        ui_widget *w = m->touch_target;
        ui_touch_press(m->ui, w == m->touch_selected ? w : NULL);
        m->touch_slider = w && (!strcmp(w->type, "SliderBar") ||
                                (!strcmp(w->type, "ScaleBar") && w->slider_range_set));
        menu_pointer(m, 1, x, y, m->touch_slider ? 3 : 0, 0);
    } else if (m->touch_active && m->touch_id == id) {
        float dx = x - m->touch_start_x, dy = y - m->touch_start_y;
        if (dx * dx + dy * dy > 64) {
            m->touch_dragging = 1; /* P: eight logical pixels. */
            ui_touch_press(m->ui, NULL);
            m->touch_selected = NULL;
        }
        if (phase == 2) {
            if (m->touch_slider)
                menu_pointer(m, 1, x, y, 4, 0);
            else if (m->touch_dragging) {
                ui_pointer_hover(m->ui, NULL);
                ui_touch_drag(m->ui, root, m->touch_start_x, m->touch_start_y, x - m->touch_last_x,
                              y - m->touch_last_y);
            }
            m->touch_last_x = x;
            m->touch_last_y = y;
        } else if (phase == 3) {
            ui_touch_press(m->ui, NULL);
            int accept =
                m->touch_target && m->touch_target == touch_hit(m, x, y) && !m->touch_dragging;
            m->touch_active = 0;
            if (m->touch_slider) {
                m->touch_selected = NULL;
                menu_pointer(m, 1, x, y, 1, 0);
            } else if (accept && m->touch_selected == m->touch_target) {
                m->touch_selected = NULL;
                menu_pointer(m, 1, x, y, 1, 0);
            } else if (accept) {
                ui_widget *hit = m->touch_target;
                m->touch_selected = hit;
                ui_widget *focused = m->focus[root];
                if (focused && !strcmp(focused->type, "OptionMenu") && focused->option_open) {
                    for (int i = 0; i < ui_option_count(focused); i++)
                        if (ui_option_item(focused, i) == hit)
                            focused->option_pending = i;
                } else {
                    if (hit->parent && !strcmp(hit->parent->type, "ListBox")) {
                        ui_widget *list = hit->parent;
                        list->focus_index = sibling_index(hit);
                        ah_val args[] = {ah_objv(m->context), ah_int(list->focus_index)}, result;
                        ah_call_member(m->vm, widget_module(m, list), "onValueChanged", 2, args,
                                       &result);
                        hit = list;
                    }
                    if (hit != m->focus[root])
                        set_focus(m, hit);
                }
            } else
                m->touch_selected = NULL;
            m->touch_target = NULL;
            menu_pointer(m, 0, 0, 0, 0, 0);
        }
    }
    ui_touch_preview(m->ui, m->touch_selected);
    return 1;
}

static void ensure_focus(menu *m)
{
    int root = menu_active_root(m);
    if (root < 0)
        return;
    ui_widget *items[512];
    int n = 0;
    /* the page itself holds the focus only while nothing on it can: a list
       filled later (memcard.ad's FileList) takes it */
    if (available(m->focus[root]) && m->focus[root] == ui_root_widget(m->ui, root))
        candidates(ui_root_widget(m->ui, root), items, &n);
    if (available(m->focus[root]) && !n) {
        if (!m->focus[root]->focused)
            set_focus(m, m->focus[root]);
        return;
    }
    if (!n)
        candidates(ui_root_widget(m->ui, root), items, &n);
    ui_widget *pick = n ? items[0] : ui_root_widget(m->ui, root);
    if (pick->parent && !strcmp(pick->parent->type, "SelectBox")) {
        ui_widget *pa = pick->parent;
        if (pa->focus_index >= 0 && pa->focus_index < pa->nkids &&
            available(pa->kids[pa->focus_index]))
            pick = pa->kids[pa->focus_index];
    }
    set_focus(m, pick);
}

static void center(const ui_widget *w, float *x, float *y)
{
    *x = w->w * .5f;
    *y = w->h * .5f;
    for (; w; w = w->parent) {
        *x += w->x;
        *y += w->y;
    }
}

/* Authored navigation regions are local left/top/right/bottom bounds, not
   geometry x/y/width/height. GT Mode uses these to form a logical map grid. */
static void navigation_region(const ui_widget *w, const char *key, float r[4])
{
    double bounds[4] = {0, 0, w->w, w->h};
    mp_numbers(w->src, key, bounds, 4);
    float x = 0, y = 0;
    for (const ui_widget *p = w; p; p = p->parent) {
        x += p->x;
        y += p->y;
    }
    r[0] = x + (float)bounds[0];
    r[1] = y + (float)bounds[1];
    r[2] = x + (float)bounds[2];
    r[3] = y + (float)bounds[3];
}

static float navigation_score(const ui_widget *from, const ui_widget *to, menu_input input)
{
    int vertical = input == MENU_UP || input == MENU_DOWN;
    int step = input == MENU_UP || input == MENU_LEFT ? -1 : 1;
    double authored[4];
    if (mp_numbers(from->src, "navigate_source", authored, 4) == 4) {
        float s[4], t[4];
        navigation_region(from, "navigate_source", s);
        navigation_region(to, "navigate_target", t);
        int a = vertical ? 1 : 0, b = 1 - a;
        float along = ((t[a] + t[a + 2]) - (s[a] + s[a + 2])) * .5f * step;
        if (along <= 1)
            return 1e30f;
        float gap = fmaxf(0, fmaxf(t[b] - s[b + 2], s[b] - t[b + 2]));
        float forward = fmaxf(0, step > 0 ? t[a] - s[a + 2] : s[a] - t[a + 2]);
        /* Authored regions define navigation lanes. A target outside the
           perpendicular lane cannot receive focus, even at a map edge. */
        if (gap > 0)
            return 1e30f;
        return forward + .001f * along;
    }
    float x, y, nx, ny;
    center(from, &x, &y);
    center(to, &nx, &ny);
    float along = (vertical ? ny - y : nx - x) * step;
    return along > 1 ? along + 3 * fabsf(vertical ? nx - x : ny - y) : 1e30f;
}

static void finalize_page(menu *m, int root)
{
    ah_val ctx = ah_objv(m->context), result;
    ah_call_member(m->vm, widget_module(m, ui_root_widget(m->ui, root)), "onFinalize", 1, &ctx,
                   &result);
    m->entered[root] = 0;
    if (m->focus[root])
        m->focus[root]->focused = 0;
    m->focus[root] = NULL;
}

static void apply_pages(menu *m)
{
    /* Authored push/pop/startPage (0022F6A0) keep the live main.game.
       Restoring the separate native save VM here invalidates references
       held by callbacks such as BuyCarRoot L6246..6278 across its modal. */
    m->page_transition_depth++;
    /* Original apply scripts mark dirty before assigning the new values.
       Capture after the callback, before another page restores preferences. */
    album_music_store(m);
    if (m->options_dirty) {
        options_capture(m);
        gt4_save_capture(m->vm, game_object(m), 1);
        m->options_dirty = 0;
        if (gt4_save_active() && gt4_save_auto_enabled() && !gt4_save_store())
            ah_logf(m->vm, "Auto Save: %s", gt4_save_error());
    }
    for (int budget = 0; m->pending_kind && budget < 16; budget++) {
        int kind = m->pending_kind, next = m->pending_page;
        m->pending_kind = 0;
        int old = menu_active_root(m);
        if (kind == 3) {
            if (old != next || m->depth <= 1)
                continue;
            finalize_page(m, old);
            m->depth--;
            ui_cursor_hold(m->ui, 0);
            menu_enter(m, menu_active_root(m));
        } else if (kind == 2 && m->pending_mores) {
            /* 0022F6A0 finalizes every page of the stack (vt+0x2D8 on its
               copy), then each page is pushed */
            int more[8], mores = m->pending_mores;
            memcpy(more, m->pending_more, sizeof more);
            m->pending_mores = 0;
            while (m->depth)
                finalize_page(m, m->pages[--m->depth]);
            m->pages[m->depth++] = next;
            menu_enter(m, next);
            for (int i = 0; i < mores && m->depth < 32; i++) {
                m->pages[m->depth++] = more[i];
                menu_enter(m, more[i]);
            }
        } else {
            /* 0022F6A0 +0x104..0x184 finalizes the old pages before
               pushing the requested page, even if it is the same one.
               quick-championship.ad L1044..1045 reopens PrizeRoot to
               initialize the separate championship bonus payment. */
            if ((next == old && kind != 2) || m->depth >= 32)
                continue;
            if (kind == 2 && m->depth) {
                finalize_page(m, old);
                m->depth--;
            }
            m->pages[m->depth++] = next;
            menu_enter(m, next);
        }
    }
    ensure_focus(m);
    context_cursor_visibility(m);
    /* Reveal again even when a script requests the current page or a later
       page callback fails: panOutIn must not strand the UI behind black. */
    if (m->pending_fade_in < 0) {
        ui_transition_reveal(m->ui);
        m->pending_fade_in = 0;
    } else if (m->pending_fade_in > 0) {
        ui_fade(m->ui, 1, 0, m->pending_fade_in);
        m->pending_fade_in = 0;
    }
    m->page_transition_depth--;
}

/* The page's focus-move sound: RootWindow `navigate_sound`, default "cursor"
   (CORE 0x00232E00; played at 0x002359B0 and by ListBox at 0x002DAB50). */
static ah_val navigate_sound(menu *m, int root)
{
    const ui_widget *page = ui_root_widget(m->ui, root);
    const char *name = page ? mp_string(page->src, "navigate_sound") : NULL;
    return ah_str(m->vm, name && *name ? name : "cursor");
}

/* A PS2 button the engine hands the scripts as a key of its own
   (00252404: the D_006192E4 keysyms), pressed or let go: onKeyPress /
   onKeyRelease from the focused widget up its parents to the page, then
   the project's (memcard.ad's MemcardProject), until one answers true
   (the scripts return 2 when they take it). event.widget stays the
   focused one. */
/* MRootWindow::getPrelightWidget: the widget the cursor lights, the
   focused one here. gtmode.ad TopRoot.onKeyPress L1347..1354 activates it
   on Return and keeps the key (2) whether or not there is one. */
static int nat_prelight(ah_vm *vm, ah_obj *self, void *d, int argc, const ah_val *argv, ah_val *ret)
{
    menu *m = d;
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    int root = menu_active_root(m);
    ui_widget *w = root >= 0 ? m->focus[root] : NULL;
    float x,y;
    if(m->pointer_hover && ui_pointer_position(m->ui,&x,&y)) w=m->pointer_hover;
    *ret = w ? maybe_module(m, w) : ah_nil();
    return 0;
}

static int key_dispatch(menu *m, int keysym, int release)
{
    if (!m || menu_active_root(m) < 0 || !keysym)
        return 0;
    m->idle_frames = 0;
    ensure_focus(m);
    int root = menu_active_root(m);
    ui_widget *w = m->focus[root] ? m->focus[root] : ui_root_widget(m->ui, root);
    if (logger_key(m, w, keysym, release))
        return 1;
    const char *method = release ? "onKeyRelease" : "onKeyPress";
    for (ui_widget *h = w; h; h = h->parent) {
        ah_val result;
        int called = event_call(m, h, w, method, keysym, 2, &result);
        if (called < 0 || (called > 0 && ah_truthy(result)))
            return 1;
    }
    {
        ah_obj *e = ah_module_new(m->vm, "event", NULL, NULL, NULL);
        ah_set_member(m->vm, e, "widget", maybe_module(m, w));
        ah_set_member(m->vm, e, "keysym", ah_int(keysym));
        ah_set_member(m->vm, e, "port", ah_int(0));
        ah_val args[2] = {ah_objv(m->context), ah_objv(e)}, result;
        int called = ah_call_member(m->vm, m->project, method, 2, args, &result);
        return called < 0 || (called > 0 && ah_truthy(result));
    }
}
void menu_send_key(menu *m, int keysym, int release)
{
    if (!m || menu_active_root(m) < 0 || !keysym)
        return;
    /* P: US StatusRoot help is "%Ba Transfer game data" (Start), but
       gtmode.ad L14926..14934 listens to 0xFF8D (Triangle), whereas
       D_006192D8 +0x50..54 maps Start to 0xFF13. Honour that visible hint
       here as an alias; retain the authored Triangle route and all other
       pages' distinct Start event. Releases use the same alias. */
    if (keysym == 0xff13 && !strcmp(ah_module_name(m->project), "GTmodeProject") &&
        !strcmp(ui_root_name(m->ui, menu_active_root(m)), "StatusRoot"))
        keysym = 0xff8d;
    key_dispatch(m, keysym, release);
    apply_pages(m);
}

/* the keys a SliderBar takes (002DF848): a popup slider only X while
   closed, everything while open; a plain one only its direction's arrows */
static int slider_key(ui_widget *w, menu_input input)
{
    if ((int)mp_number(w->src, "popup", 0))
        return w->option_open || input == MENU_ACCEPT;
    if ((int)mp_number(w->src, "direction", 0) == 1)
        return input == MENU_UP || input == MENU_DOWN;
    return input == MENU_LEFT || input == MENU_RIGHT;
}
/* The bars' own keys, given to the bar w (focused, or the target of a
   script's MKeyPressEvent). 1 when the bar took the key. */
static int bar_input(menu *m, ui_widget *w, menu_input input)
{
    if (!strcmp(w->type, "ScaleBar") && !w->slider_range_set) {
        /* mScaleBar +0x244 (00238D10), popup mode (+0xF0): Return opens it
           (00238BE0, value kept) or closes it (00238C60); Escape closes it
           with cancel (+0x104), the kept value back. Open, Left/Right
           (00238A70/00238B28, horizontal +0xC0) subtract/add step clamped to
           min/max (00239098/00239048) and call onValueChanged (00238978);
           closed, the arrows are the base widget's (00263D28). */
        if (!w->option_open && input != MENU_ACCEPT)
            return 0;
        if (input == MENU_ACCEPT) {
            if (!w->option_open)
                w->photo_saved = w->photo_value;
            w->option_open = !w->option_open;
            return 1;
        }
        float before = w->photo_value;
        if (input == MENU_BACK) {
            w->photo_value = w->photo_saved;
            w->option_open = 0;
        } else if (input == MENU_LEFT || input == MENU_RIGHT) {
            float step = w->photo_step;
            if (!(step > 0))
                step = 1; /* A: pad repetition pending. */
            w->photo_value = fminf(w->photo_max, fmaxf(w->photo_min, w->photo_value + (input == MENU_LEFT ? -step : step)));
        }
        if (w->photo_value != before) {
            ah_val ctx = ah_objv(m->context), result;
            ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 1, &ctx, &result);
        }
        return 1;
    }
    if ((!strcmp(w->type, "SliderBar") ||
                !strcmp(w->type, "ScaleBar") && w->slider_range_set) &&
               slider_key(w, input)) {
        /* mSliderBar +0x244 (002DF848). With the layout's popup (+0xC0, every
           slider but option2/3's position bars) X opens it (002DF738, "cursor",
           value kept at +0xF8), the arrows of its direction step it while open
           (002DF640) and every other key is taken; X closes it (002DF7B0),
           Circle puts the kept value back. Without popup the arrows step it
           directly (002DF540) and the rest goes to the base widget. */
        int popup = (int)mp_number(w->src, "popup", 0) != 0;
        int vertical = (int)mp_number(w->src, "direction", 0) == 1;
        if (popup && input == MENU_ACCEPT) {
            if (!w->option_open)
                w->option_pending = w->focus_index;
            w->option_open = !w->option_open;
            menu_play_effect(m, "cursor");
            return 1;
        }
        int old = w->focus_index;
        if (input == MENU_BACK) {
            assign_index(w, w->option_pending);
            w->option_open = 0;
            menu_play_effect(m, "cursor");
        } else if (vertical ? input == MENU_UP || input == MENU_DOWN
                            : input == MENU_LEFT || input == MENU_RIGHT)
            assign_index(w, old + (input == MENU_LEFT || input == MENU_UP ? -1 : 1));
        if (old != w->focus_index) {
            ah_val args[2] = {ah_objv(m->context), ah_int(w->focus_index)}, result;
            ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 2, args, &result);
            if (input != MENU_BACK)
                menu_play_effect(m, "param");
        }
        return 1;
    }
    return 0;
}
void menu_send_input(menu *m, menu_input input)
{
    if (attract_left && m && !strcmp(ui_root_name(m->ui, menu_active_root(m)), "MovieRoot"))
        attract_cancelled = 1;
    if (!m || menu_active_root(m) < 0)
        return;
    /* Circle/Square keep their own PS2 script keys before the default action. */
    if (!m->pointer_dispatch) {
        if(m->pointer_hover) pointer_set_hover(m,NULL);
        ui_pointer(m->ui, 0, 0, 0);
        ui_pointer_scroll_clear(m->ui);
    }
    menu_input physical = input;
    if (input == MENU_CIRCLE)
        input = MENU_ACCEPT;
    if (input == MENU_SQUARE || input == MENU_TRIANGLE)
        input = MENU_BACK;
    if (!strcmp(ah_module_name(m->project), "PhotoShootProject") ||
        !strcmp(m->project_label, "photo_save")) {
        const char *page = ui_root_name(m->ui, menu_active_root(m));
        /* P: PC storage boundary. PreviewRoot Enter exports a PNG rather
           than entering the original Memory Card / USB SaveDialog. */
        if (input == MENU_ACCEPT && page && !strcmp(page, "PreviewRoot")) {
            if (m->photo.image_width)
                m->photo.save_requested = 1;
            return;
        }
    }
    m->idle_frames = 0;
    ensure_focus(m);
    int root = menu_active_root(m);
    ui_widget *w = m->focus[root];
    if (!w && (input == MENU_ACCEPT || input == MENU_BACK))
        w = ui_root_widget(m->ui, root);
    if (!w)
        return;
    /* P: these icons can receive focus/tooltips, but cannot finish the menu,
       stop its music, save data or launch an unimplemented mode. */
    if (input == MENU_ACCEPT && quick_pending_icon(m, w))
        return;
    if (input == MENU_ACCEPT && lan_blocked(m, w)) {
        menu_play_effect(m, "disable");
        return;
    }
    /* 00252404: every pad key reaches the scripts first (onKeyPress from
       the focused widget up to the page and the project; menu_send_key),
       the engine's own move or activation only when none takes it:
       labomode.ad onListKeyPress keeps the arrows, photo_shoot.ad's
       ShootRoot grabs a tool on Return (and lets go on its release),
       SlideRoot takes Return with no onActivate, HomeRoot / setting.ad
       their Triangle. Its release comes with the button's (main.c). */
    {
        static const int keysym[] = {
            [MENU_UP] = 65362,       [MENU_DOWN] = 65364,   [MENU_LEFT] = 65361,
            [MENU_RIGHT] = 65363,    [MENU_ACCEPT] = 65293, [MENU_BACK] = 65307,
            [MENU_TRIANGLE] = 65421, [MENU_CIRCLE] = 65307, [MENU_SQUARE] = 65379};
        if (physical >= 0 && physical <= MENU_SQUARE && keysym[physical] &&
            key_dispatch(m, keysym[physical], 0)) {
            apply_pages(m);
            return;
        }
    }
    if (bar_input(m, w, input)) {
    } else if (!strcmp(w->type, "ListBox") && (int)mp_number(w->src, "direction", 0) == 1 &&
               (input == MENU_UP || input == MENU_DOWN) && w->nkids > 1) {
        /* a vertical ListBox (labomode.ad's lists): up and down move along
           it, stopping at its ends; left and right leave it by geometry */
        int next = w->focus_index + (input == MENU_UP ? -1 : 1);
        if (next >= 0 && next < w->nkids) {
            w->focus_index = next;
            ah_val args[2] = {ah_objv(m->context), ah_int(next)}, result;
            ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 2, args, &result);
            ah_val sound = navigate_sound(m, root);
            nat_sound_play(m->vm, NULL, m, 1, &sound, &result);
        }
    } else if (!strcmp(w->type, "ListBox") && (int)mp_number(w->src, "direction", 0) != 1 &&
               input <= MENU_RIGHT && w->nkids) {
        int step = input == MENU_UP || input == MENU_LEFT ? -1 : 1;
        w->focus_index = (w->focus_index + step + w->nkids) % w->nkids;
        ah_val args[2] = {ah_objv(m->context), ah_int(w->focus_index)}, result;
        ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 2, args, &result);
        ah_val sound = navigate_sound(m, root);
        nat_sound_play(m->vm, NULL, m, 1, &sound, &result);
    } else if (!strcmp(w->type, "OptionMenu") && (w->option_open || input == MENU_ACCEPT)) {
        int count = ui_option_count(w), step = input == MENU_LEFT || input == MENU_UP ? -1 : 1;
        if (input == MENU_BACK) {
            w->option_open = 0;
            menu_play_effect(m, "cancel");
            return;
        }
        if (input == MENU_ACCEPT) {
            if (!w->option_open) {
                w->option_pending = w->focus_index;
                w->option_open = 1;
                menu_play_effect(m, "select");
                return;
            }
            w->option_open = 0;
            menu_play_effect(m, "select");
            if (w->option_pending != w->focus_index) {
                w->focus_index = w->option_pending;
                ah_val args[2] = {ah_objv(m->context), ah_int(w->focus_index)}, result;
                ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 2, args, &result);
                apply_pages(m);
            }
            return;
        }
        int old = w->option_open ? w->option_pending : w->focus_index;
        for (int offset = 1; offset <= count; offset++) {
            int next = ((old + step * offset) % count + count) % count;
            ui_widget *item = ui_option_item(w, next);
            if (!available(item))
                continue;
            if (next != old) {
                if (w->option_open) {
                    w->option_pending = next;
                    menu_play_effect(m, "cursor");
                    return;
                }
                w->focus_index = next;
                ah_val args[2] = {ah_objv(m->context), ah_int(next)}, result;
                ah_call_member(m->vm, widget_module(m, w), "onValueChanged", 2, args, &result);
                ah_val sound = ah_str(m->vm, "param");
                nat_sound_play(m->vm, NULL, m, 1, &sound, &result);
            }
            break;
        }
    } else if (input <= MENU_RIGHT) {
        ui_widget *items[512];
        int n = 0, index = -1;
        candidates(w->parent && !strcmp(w->parent->type, "SelectBox") ? w->parent
                                                                      : ui_root_widget(m->ui, root),
                   items, &n);
        for (int i = 0; i < n; i++)
            if (items[i] == w)
                index = i;
        if (n) {
            int step = input == MENU_UP || input == MENU_LEFT ? -1 : 1;
            int next = index < 0 ? 0 : (index + step + n) % n;
            /* SelectBox stops at its ends (CORE 0x002D7468/0x002D7528). */
            if (index >= 0 && w->parent && !strcmp(w->parent->type, "SelectBox") &&
                (index + step < 0 || index + step >= n))
                next = index;
            if (index >= 0 && (!w->parent || strcmp(w->parent->type, "SelectBox"))) {
                /* Directional focus for grids; SelectBox keeps its ordered list. */
                float best = 1e30f;
                next = index;
                for (int i = 0; i < n; i++)
                    if (i != index) {
                        float score = navigation_score(w, items[i], input);
                        if (score < best) {
                            best = score;
                            next = i;
                        }
                    }
            }
            set_focus(m, items[next]);
            if (next != index) {
                ah_val sound = navigate_sound(m, root), result;
                nat_sound_play(m->vm, NULL, m, 1, &sound, &result);
            }
            ui_widget *pa = items[next]->parent;
            if (pa && !strcmp(pa->type, "SelectBox") && next != index) {
                ah_val args[2] = {ah_objv(m->context), ah_int(pa->focus_index)}, result;
                ah_call_member(m->vm, widget_module(m, pa), "onValueChanged", 2, args, &result);
            }
        }
    } else {
        /* Keep event.widget at the original target while bubbling to parents.
           The scripts use return 2 to consume an activation/cancel. */
        const char *method = input == MENU_ACCEPT ? "onActivate" : "onCancel";
        for (ui_widget *h = w; h; h = h->parent) {
            ah_val result;
            int called = event_call(m, h, w, method, 0, 3, &result);
            if (called < 0 || (called && ah_num(result) != 0))
                break;
        }
    }
    apply_pages(m);
}

int menu_take_project(menu *m, char *name, size_t capacity)
{
    if (!m->next_project[0] || !capacity)
        return 0;
    snprintf(name, capacity, "%s", m->next_project);
    m->next_project[0] = 0;
    return 1;
}

void menu_set_origin(menu *m, const char *project)
{
    /* 00109D00 +44..58 clears game+0x3A368 before loading each menu.
       Cached native VMs must also consume the previous sequence here;
       arcade.ad TopRoot.Option L647..649 only changes next_menu_project. */
    ah_set_member(m->vm, game_object(m), "next_sequence", ah_str(m->vm, ""));
    m->race_requested = 0;
    ah_set_member(m->vm, game_object(m), "last_menu_project", ah_str(m->vm, project));
}

int menu_take_photo_film(menu *m, char *path, size_t capacity)
{
    if (!m || !capacity || !film_loaded[0])
        return 0;
    ah_val result;
    if (!ah_get_member(m->vm, game_object(m), "menu_result", &result) || !ah_cstr(result) ||
        strcmp(ah_cstr(result), "develop") || !m->next_project[0])
        return 0;
    /* labomode.ad commandDevelop sets menu_result after file.load OK. */
    snprintf(path, capacity, "%s/films/%s", gt4_save_directory(), film_loaded);
    film_loaded[0] = 0;
    m->next_project[0] = 0;
    ah_set_member(m->vm, game_object(m), "menu_result", ah_nil());
    return 1;
}

int menu_setting_begin(menu *m)
{
    if (!m || strcmp(ah_module_name(m->project), "SettingProject"))
        return 0;
    menu_save_refresh(m);
    ah_val ctx = ah_objv(m->context), result;
    /* Native sequence dispatch consumed setting; authored exit requests menu. */
    ah_set_member(m->vm, game_object(m), "next_sequence", ah_str(m->vm, "menu"));
    /* setting.ad L28..58: bind current car, resolve rows, queue SettingRoot. */
    return ah_call_member(m->vm, m->project, "onLoad", 1, &ctx, &result) >= 0;
}
void menu_setting_context(menu *m, menu *source)
{
    if (!m || !source)
        return;
    const char *keys[] = {"race_code", "course_code", "major_menu_project", "major_menu_page"};
    /* setting.ad L36..45 reads the shared game context. Cached PC VMs
    carry separate objects, so copy the context labels at this boundary. */
    for (int i = 0; i < 4; i++) {
        ah_val value = ah_nil();
        ah_get_member(source->vm, game_object(source), keys[i], &value);
        ah_set_member(m->vm, game_object(m), keys[i],
                      ah_str(m->vm, ah_cstr(value) ? ah_cstr(value) : ""));
    }
}
/* gtmode.ad MusicTheater -> exit_kick_project("slide"): its page
   (next_menu_arg) and the way back (major_menu_*), carried into the
   slide project's own VM */
void menu_jukebox_context(menu *m, menu *source)
{
    if (!m || !source)
        return;
    menu_setting_context(m, source);
    const char *keys[] = {"next_menu_arg", "major_menu_widget"};
    for (int i = 0; i < 2; i++) {
        ah_val value = ah_nil();
        ah_get_member(source->vm, game_object(source), keys[i], &value);
        if (ah_cstr(value))
            ah_set_member(m->vm, game_object(m), keys[i], ah_str(m->vm, ah_cstr(value)));
    }
}
int menu_setting_end(menu *m)
{
    if (!m || strcmp(ah_module_name(m->project), "SettingProject"))
        return 0;
    ah_val ctx = ah_objv(m->context), result;
    /* setting.ad L65..69: synchronize sheets before saving the car. */
    if (ah_call_member(m->vm, m->project, "onUnload", 1, &ctx, &result) < 0)
        return 0;
    menu_save_capture(m);
    return 1;
}
int menu_riding_car_body(menu *m, uint8_t body[0x4b8])
{
    if (!m)
        return 0;
    ah_val garage = ah_nil(), car = ah_nil();
    ah_get_member(m->vm, game_object(m), "garage", &garage);
    ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &car);
    return gt4_game_car_body(m->vm, ah_as_obj(car, AO_MODULE), body);
}

/* Photo Travel's car parts: the riding car's fitted wheel and wing when
   it is the garage's car (no game.car_code), else none */
int menu_photo_parts(menu *m, char wheel[32], char wing[32])
{
    wheel[0] = wing[0] = 0;
    if (!m)
        return 0;
    ah_val vehicle = ah_nil();
    ah_get_member(m->vm, game_object(m), "car_code", &vehicle);
    if (ah_cstr(vehicle) && *ah_cstr(vehicle))
        return 1;
    uint8_t body[0x4b8];
    return menu_riding_car_body(m, body) && gt4_body_part_labels(m->vol, body, wheel, wing);
}
int menu_photo_request(menu *m, char course[32], char car[96], int *color)
{
    if (!m || !course || !car || !color)
        return 0;
    ah_val scene = ah_nil(), vehicle = ah_nil(), paint = ah_nil(), garage = ah_nil();
    ah_obj *game = game_object(m);
    ah_get_member(m->vm, game, "course_code", &scene);
    ah_get_member(m->vm, game, "car_code", &vehicle);
    const char *label = ah_cstr(vehicle);
    if (!label || !*label) {
        ah_get_member(m->vm, game, "garage", &garage);
        ah_get_member(m->vm, ah_as_obj(garage, AO_MODULE), "riding_car", &vehicle);
        label = car_label_of(m->vm, vehicle);
        ah_get_member(m->vm, ah_as_obj(vehicle, AO_MODULE), "color_index", &paint);
    } else
        ah_get_member(m->vm, game, "color_index", &paint);
    const char *location = ah_cstr(scene);
    if (!location || strncmp(location, "ss_", 3) || strlen(location) >= 32 || !label || !*label ||
        strlen(label) >= 96)
        return 0;
    snprintf(course, 32, "%s", location);
    snprintf(car, 96, "%s", label);
    *color = ah_is_num(paint) ? to_int(paint) : 0;
    return 1;
}

void menu_photo_set_selection(menu *m, const char *course, const char *car, int color)
{
    if (!m)
        return;
    ah_obj *game = game_object(m);
    ah_set_member(m->vm, game, "course_code", ah_str(m->vm, course));
    ah_set_member(m->vm, game, "car_code", ah_str(m->vm, car));
    ah_set_member(m->vm, game, "color_index", ah_int(color));
}
gt4_photo_mode *menu_photo_state(menu *m)
{
    return m ? &m->photo : NULL;
}
void menu_photo_set_location(menu *m, const gt4_course *course)
{
    if (!m)
        return;
    m->photo.ready = 0;
    if (!gt4_photo_mode_init(&m->photo, course)) {
        fprintf(stderr, "Photo Travel: invalid scene placement\n");
        return;
    }
    /* A: native map zoom setup is not yet compared; bounds 20..1000
       from 00370240 +60..7C, initial length from the destination. */
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "ShootRoot")) {
            ui_widget *map = ui_widget_child(ui_root_widget(m->ui, i), "Map"),
                      *pm = ui_widget_child(map, "PM"), *zoom = ui_widget_child(pm, "zoom");
            if (zoom) {
                zoom->photo_min = 20;
                zoom->photo_max = 1000;
                zoom->photo_value = fminf(1000, fmaxf(20, m->photo.location.map_length));
            }
            /* Loading a different destination must rerun the authored resets. */
            while (m->depth)
                finalize_page(m, m->pages[--m->depth]);
            m->pending_kind = 0;
            m->entered[i] = 0;
        }
}

const char *menu_selected_course(menu *m)
{
    ah_val root, code;
    if (m && ah_get_member(m->vm, m->project, "CourseRoot", &root) &&
        ah_get_member(m->vm, ah_as_obj(root, AO_MODULE), "current_crs", &code))
        return ah_cstr(code);
    return NULL;
}

int menu_car_color(const menu *m)
{
    return m ? m->car_color : -1;
}
int menu_take_race_request(menu *m)
{
    int r = m && m->race_requested;
    if (r && !strcmp(ah_module_name(m->project), "ArcadeProject") && !menu_race_is_split(m)) {
        /* arcade.ad apply_setting L4689..4694 mutates main.game.option
           without setNeedSave. The PS2 keeps that same object across
           00111B50 (+1B0..1BC -> 00430878) and quick_tune_popup.ad L403.
           Native projects use separate VMs: publish the live preference
           before the pre-race VM restores its option snapshot. */
        options_capture(m);
        gt4_save_capture(m->vm, game_object(m), 1);
    }
    if (m)
        m->race_requested = 0;
    return r;
}
/* tests: the menu stack as a dealer leaves it (game.menuPush each name) */
void menu_test_push(menu *m, const char *names)
{
    if (!m || !names)
        return;
    ah_obj *game = game_object(m);
    ah_val push, result;
    if (!ah_get_member(m->vm, game, "menuPush", &push))
        return;
    char copy[256];
    snprintf(copy, sizeof copy, "%s", names);
    for (char *name = strtok(copy, ","); name; name = strtok(NULL, ",")) {
        ah_val arg = ah_str(m->vm, name);
        ah_call(m->vm, push, 1, &arg, &result);
    }
}
/* tests: credits through the game's own incMoney */
void menu_test_money(menu *m, int64_t amount)
{
    ah_val fn, arg, result;
    if (!m || !ah_get_member(m->vm, game_object(m), "incMoney", &fn) || fn.t == AV_NIL)
        return;
    arg = ah_int(amount);
    ah_call(m->vm, fn, 1, &arg, &result);
}
/* tests: "label" a new car through the garage's own addNewCar, or
   "label:meters" a used one (MCarGarage.makeUsed, addUsedCar), then ridden */
void menu_test_car(menu *m, const char *spec)
{
    ah_val garage, ret, args[3], cars;
    char label[64];
    if (!m || !spec || !ah_get_member(m->vm, game_object(m), "garage", &garage) ||
        garage.t == AV_NIL)
        return;
    snprintf(label, sizeof label, "%s", spec);
    char *meters = strchr(label, ':');
    if (meters)
        *meters++ = 0;
    ah_obj *g = ah_as_obj(garage, AO_MODULE);
    args[0] = ah_str(m->vm, label);
    args[1] = ah_int(0);
    if (meters) {
        ah_val main_, menu_, cls, source;
        if (!ah_get_member(m->vm, ah_root(m->vm), "main", &main_) ||
            !ah_get_member(m->vm, ah_as_obj(main_, AO_MODULE), "menu", &menu_) ||
            !ah_get_member(m->vm, ah_as_obj(menu_, AO_MODULE), "MCarGarage", &cls) ||
            ah_call(m->vm, cls, 1, args, &source) < 0)
            return;
        ah_val used[] = {ah_int(1), ah_int(0), ah_int((int64_t)strtoull(meters, NULL, 10))};
        if (ah_call_member(m->vm, ah_as_obj(source, AO_MODULE), "makeUsed", 3, used, &ret) != 1 ||
            ah_call_member(m->vm, g, "addUsedCar", 1, &source, &ret) != 1)
            return;
    } else if (ah_call_member(m->vm, g, "addNewCar", 2, args, &ret) != 1)
        return;
    if (!ah_get_member(m->vm, g, "cars", &cars) || cars.t == AV_NIL)
        return;
    ah_val index = ah_int((int32_t)ah_array_size(ah_as_obj(cars, AO_ARRAY)) - 1);
    ah_call_member(m->vm, g, "setRidingCar", 1, &index, &ret);
    menu_save_capture(m); /* menu_enter reloads the garage from the save */
}
/* tests: what MissionRoot.onActivate leaves once its checks pass */
void menu_request_mission(menu *m, const char *code)
{
    if (!m || !code)
        return;
    ah_obj *game = game_object(m);
    ah_set_member(m->vm, game, "race_code", ah_str(m->vm, code));
    ah_set_member(m->vm, game, "next_sequence", ah_str(m->vm, "mission"));
    m->race_requested = 1;
}
const char *menu_selected_car(menu *m)
{
    ah_val root, code;
    if (m && ah_get_member(m->vm, m->project, "CarRoot", &root) &&
        ah_get_member(m->vm, ah_as_obj(root, AO_MODULE), "current_car", &code))
        return ah_cstr(code);
    return NULL;
}

void menu_set_music_fader(menu *m, void (*fn)(float))
{
    if (m)
        m->music_fader = fn;
}
void menu_set_audio_preparer(menu *m, void (*fn)(menu *, void *), void *user)
{
    if (m) {
        m->audio_preparer = fn;
        m->audio_preparer_user = user;
    }
}
int menu_import_ps2_media(menu *m, const char *path, gt4_media_import *report)
{
    return m && gt4_media_import_ps2(m->vol, path, report);
}

int menu_championship_context_get(menu *m, uint8_t live[0x90])
{
    return m && gt4_game_championship_context_get(m->vm, game_object(m), live);
}
int menu_championship_context_set(menu *m, const uint8_t live[0x90])
{
    if (!m || !gt4_game_championship_context_set(m->vm, game_object(m), live))
        return 0;
    menu_save_capture(m);
    return 1;
}
