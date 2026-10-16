/* Conductor field offsets in the original memory layout.
 * _U preserves unsigned literals; UNK fields have no assigned meaning.
 */
#ifndef MOON_FIELDS_CONDUCTOR_H
#define CONDUCTOR_FIELD_BSPEC_COURSE_KIND 0xCBE7 /* Original: func_00468A40 +0x14 */
#define MOON_FIELDS_CONDUCTOR_H

#define CONDUCTOR_FIELD_TYRE_STATE 0x4     /* shared tyre state; init.c race_start, func_00367C90 */
#define CONDUCTOR_FIELD_UNK_7C 0x7C        /* race.c: gt4_race_rubber_band */
#define CONDUCTOR_FIELD_CARS 0x6698        /* car array; func_0034C190, car.h */
#define CONDUCTOR_FIELD_CARS_U 0x6698u     /* car array; func_0034C190, car.h */
#define CONDUCTOR_FIELD_UNK_CA58_U 0xCA58u /* driver.c: gt4_car_force_feedback */
#define CONDUCTOR_FIELD_UNK_CAB8 0xCAB8    /* race_game_license.inc: license_step */
#define CONDUCTOR_FIELD_UNK_CAB8_U 0xCAB8u /* race_run.c: gt4_race_run_camera */
#define CONDUCTOR_FIELD_COURSE 0xCBD8      /* course object; session.c course_of */
#define CONDUCTOR_FIELD_COURSE_U 0xCBD8u   /* course object; session.c course_of */
#define CONDUCTOR_FIELD_DRIVING_MODE 0xCBDC    /* mode set by gt4_race_init (init.c) */
#define CONDUCTOR_FIELD_DRIVING_MODE_U 0xCBDCu /* mode set by gt4_race_init (init.c) */
#define CONDUCTOR_FIELD_COURSE_MODE 0xCBDD     /* COURSE row +0x10; func_0034C7E0 */
#define CONDUCTOR_FIELD_COURSE_MODE_U 0xCBDDu  /* COURSE row +0x10; func_0034C7E0 */
#define CONDUCTOR_FIELD_START_KIND_U                                                               \
    0xCBDEu /* COURSE row +0x12, grid/rolling placement; func_0034C7E0 */
#define CONDUCTOR_FIELD_COURSE_FLAG_14_U 0xCBE0u /* COURSE row +0x14 != 0; func_0034C7E0 */
#define CONDUCTOR_FIELD_CLOCK_STOPPED_U 0xCBE4u  /* nonzero stops race clock; func_00357E80 */
#define CONDUCTOR_FIELD_COURSE_CODE_U 0xCBE6u   /* eight encoded course-code bytes; func_0034C7E0 */
#define CONDUCTOR_FIELD_UNK_CBF4_U 0xCBF4u      /* init.c: gt4_race_init */
#define CONDUCTOR_FIELD_UNK_CBFC_U 0xCBFCu      /* init.c: gt4_race_init */
#define CONDUCTOR_FIELD_PAIR_LAP_GAPS_U 0xCCC8u /* six-by-six lap gaps; func_00366018 */
#define CONDUCTOR_FIELD_PAIR_DISTANCE_GAPS_U 0xCCECu /* six-by-six distance gaps; func_00366018 */
#define CONDUCTOR_FIELD_UNK_CD7C_U 0xCD7Cu           /* init.c: gt4_race_init */
#define CONDUCTOR_FIELD_UNK_D7A4_U 0xD7A4u           /* race_game_bspec.inc: bspec_sectors */
#define CONDUCTOR_FIELD_PIT_STOP_COUNT_U                                                           \
    0xD9E4u /* func_0035AD10 +0xD9E4: per-car uint16 count incremented at pit entry */
#define CONDUCTOR_FIELD_LINES_U 0xDA50u     /* course-line object; init.c gt4_lines_setup */
#define CONDUCTOR_FIELD_UNK_DA78 0xDA78     /* course_objects.c: gt4_course_objects_step */
#define CONDUCTOR_FIELD_UNK_DA78_U 0xDA78u  /* laps.c: pit_lane */
#define CONDUCTOR_FIELD_RULES 0xF818        /* decoded RACE rules; func_0034D418 / 0035E108 */
#define CONDUCTOR_FIELD_RULES_U 0xF818u     /* decoded RACE rules; func_0034D418 / 0035E108 */
#define CONDUCTOR_FIELD_RULE_KIND 0xF81A    /* kind read by licence finish policy; func_003F8818 */
#define CONDUCTOR_FIELD_RULE_KIND_U 0xF81Au /* kind read by licence finish policy; func_003F8818 */
#define CONDUCTOR_FIELD_UNK_F830 0xF830     /* drivetrain.c: gt4_car_drivetrain */
#define CONDUCTOR_FIELD_UNK_F830_U 0xF830u  /* session.c: lap_limits */
#define CONDUCTOR_FIELD_TIME_LIMIT_MINUTES_U                                                       \
    0xF850u /* minutes times 60000; race.c race_over, func_0034C140 */
#define CONDUCTOR_FIELD_ALL_CAR_FINISH 0xF858    /* all-car finish flag; func_003F8818 */
#define CONDUCTOR_FIELD_ALL_CAR_FINISH_U 0xF858u /* all-car finish flag; func_003F8818 */
#define CONDUCTOR_FIELD_UNK_F85E 0xF85E          /* race_game_hud.inc: race_game_draw_hud */
#define CONDUCTOR_FIELD_UNK_F860 0xF860          /* license_session.c: gt4_license_collision */
#define CONDUCTOR_FIELD_UNK_F860_U 0xF860u       /* session.c: race_start */
#define CONDUCTOR_FIELD_UNK_F861 0xF861          /* license_session.c: gt4_license_collision */
#define CONDUCTOR_FIELD_UNK_F861_U 0xF861u       /* session.c: race_start */
#define CONDUCTOR_FIELD_UNK_F862_U 0xF862u       /* session.c: race_start */
#define CONDUCTOR_FIELD_CONTACT_PENALTY_LIMIT                                                      \
    0xF864 /* car contact threshold; race_run.c Time Trial setup */
#define CONDUCTOR_FIELD_CONTACT_PENALTY_LIMIT_U                                                    \
    0xF864u                                /* car contact threshold; race_run.c Time Trial setup */
#define CONDUCTOR_FIELD_UNK_F866 0xF866    /* session.c: battle */
#define CONDUCTOR_FIELD_UNK_F866_U 0xF866u /* init.c: gt4_race_init */
#define CONDUCTOR_FIELD_UNK_F86C 0xF86C    /* session.c: license */
#define CONDUCTOR_FIELD_UNK_F86C_U 0xF86Cu /* init.c: gt4_race_init */
#define CONDUCTOR_FIELD_START_DISTANCE_U                                                           \
    0xF884u                                   /* signed start distance; session.c race_start_at */
#define CONDUCTOR_FIELD_START_SPEED_U 0xF886u /* start speed; session.c race_start_at */
#define CONDUCTOR_FIELD_START_GAP_U 0xF887u   /* grid-slot distance step; session.c race_start_at */
#define CONDUCTOR_FIELD_RACE_CLOCK 0xF898     /* 50 units/frame unless stopped; func_00357E80 */
#define CONDUCTOR_FIELD_RACE_CLOCK_U 0xF898u  /* 50 units/frame unless stopped; func_00357E80 */
#define CONDUCTOR_FIELD_RUNNING_CLOCK_U                                                            \
    0xF89Cu /* 50 units/frame even when race clock stopped; func_00357E80 */
#define CONDUCTOR_FIELD_CAR_COUNT 0xF8A0          /* number of cars; step.c ncars */
#define CONDUCTOR_FIELD_CAR_COUNT_U 0xF8A0u       /* number of cars; step.c ncars */
#define CONDUCTOR_FIELD_POSITION_ORDER 0xF8A4     /* car indices by position; func_00365BC0 */
#define CONDUCTOR_FIELD_POSITION_ORDER_U 0xF8A4u  /* car indices by position; func_00365BC0 */
#define CONDUCTOR_FIELD_UNK_10000 0x10000         /* surface.c: gt4_car_surface_feel */
#define CONDUCTOR_FIELD_UNK_10000_U 0x10000u      /* ai.c: gt4_cars_nearby */
#define CONDUCTOR_FIELD_UNK_1003C 0x1003C         /* race_game_hud.inc: race_game_draw_hud */
#define CONDUCTOR_FIELD_UNK_1003C_U 0x1003Cu      /* session.c: race_start */
#define CONDUCTOR_FIELD_UNK_1003E 0x1003E         /* license_session.c: gt4_license_wait */
#define CONDUCTOR_FIELD_SESSION_VTABLE 0x10140    /* session virtual table; phys.h gt4_session */
#define CONDUCTOR_FIELD_SESSION_VTABLE_U 0x10140u /* session virtual table; phys.h gt4_session */

#define CONDUCTOR_FIELD_PIT_LINE_POINT_COUNT_U                                                     \
    0xda5cu /* func_0034C750 conductor+0xDA5C: signed pit-line point count; zero disables pits */

#define CONDUCTOR_FIELD_PIT_LAP_HISTORY_HEAD_U                                                     \
    0xd9f0u /* func_0035AD10 conductor+0xD9F0: latest pit-stop lap, car stride 8 */

#define CONDUCTOR_FIELD_PIT_LAP_SHIFT_SOURCE_U                                                     \
    0xd9f4u /* func_0035AD10 conductor+0xD9F4: third lap-history element before shift */

#define CONDUCTOR_FIELD_PIT_LAP_SHIFT_DEST_U                                                       \
    0xd9f6u /* func_0035AD10 conductor+0xD9F6: fourth lap-history element after shift */

#define CONDUCTOR_FIELD_PIT_FRONT_TYRE_HISTORY_U                                                   \
    0xda20u /* func_0035AD10/0035C0B8 +0xDA20: newest front compound; car stride 8 */

#define CONDUCTOR_FIELD_PIT_REAR_TYRE_HISTORY_U                                                    \
    0xda21u /* func_0035AD10/0035C0B8 +0xDA21: newest rear compound; car stride 8 */

#define CONDUCTOR_FIELD_PIT_FRONT_SHIFT_SOURCE_U                                                   \
    0xda24u /* func_0035AD10 +0xDA24: third front-compound history element before shift */

#define CONDUCTOR_FIELD_PIT_REAR_SHIFT_SOURCE_U                                                    \
    0xda25u /* func_0035AD10 +0xDA25: third rear-compound history element before shift */

#define CONDUCTOR_FIELD_PIT_FRONT_SHIFT_DEST_U                                                     \
    0xda26u /* func_0035AD10 +0xDA26: fourth front-compound history element after shift */

#define CONDUCTOR_FIELD_PIT_REAR_SHIFT_DEST_U                                                      \
    0xda27u /* func_0035AD10 +0xDA27: fourth rear-compound history element after shift */

#endif
