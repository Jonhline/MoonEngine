/* Car field offsets. CAR_BLOCK_* is relative to car + CAR_POSITION.
 * Unknown meanings stay neutral; suffix _U preserves an unsigned literal.
 */
#ifndef OPENGT4_FIELDS_CAR_H
#define OPENGT4_FIELDS_CAR_H

#define CAR_BLOCK_UNK_4 0x4 /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_UNK_8 0x8 /* car+0x104 relative; chassis.c: jack_hold, func_0036A318 */
#define CAR_BLOCK_ORIENTATION_BASIS_0                                                              \
    0xC /* func_0035B060 block+0xC: first vector output of gt4_rotation_from_angles */
#define CAR_FIELD_CONTROLLER                                                                       \
    0xC /* controller/profile object; driver.c gt4_pad_idle, func_00426EA0 */
#define CAR_FIELD_DRIVER_MODEL 0x14 /* driver model; init.c car_clear, func_00343AB8 / 00463930 */
#define CAR_BLOCK_ORIENTATION_BASIS_1                                                              \
    0x18 /* func_0035B060 block+0x18: second vector output of gt4_rotation_from_angles */
#define CAR_FIELD_UNK_55 0x55        /* aids.c: balance, func_003F71E8 */
#define CAR_FIELD_UNK_83 0x83        /* race_game.c: update_drivers */
#define CAR_BSPEC_STYLE_SIZE 0x40 /* Original: func_003F1E60 +0x94 */
#define CAR_FIELD_DRIVING_STYLE 0x8C /* player style copied from race+0x20; func_003F1E60 */
#define CAR_BLOCK_PLAYER 0x440       /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_VIBRATION_ON                                                                     \
    0x441 /* car+0x104 relative; ai.c: gt4_race_player_styles, func_003F1E60 */
#define CAR_BLOCK_UNK_442 0x442 /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_RATE 0x44C    /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_UNK_454 0x454 /* car+0x104 relative; driver.c: gt4_car_force_feedback */
#define CAR_BLOCK_UNK_465 0x465 /* car+0x104 relative; ai.c: gt4_ai_drive, func_00465078 */
#define CAR_BLOCK_DRIVING_CONTROL_MODE                                                             \
    0x466 /* func_0035BF08 block+0x466: driving control mode (0 player, 1 B-Spec) */
#define CAR_BLOCK_PIT_LANE_STATE 0x467  /* func_0035B060 +0x467: 2 entering, 3 at box, 4 leaving */
#define CAR_BLOCK_UNK_468 0x468         /* car+0x104 relative; ai.c: gt4_ai_apply, func_00464F38 */
#define CAR_BLOCK_UNK_484 0x484         /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_THROTTLE_REQ 0x488    /* car+0x104 relative; ai.c: gt4_ai_apply, func_00464F38 */
#define CAR_BLOCK_HANDBRAKE 0x490       /* car+0x104 relative; ai.c: gt4_ai_drive, func_00465078 */
#define CAR_BLOCK_THROTTLE 0x494        /* car+0x104 relative; ai.c: gt4_ai_apply, func_00464F38 */
#define CAR_BLOCK_COURSE_DISTANCE 0x4A4 /* car+0x104 relative; course distance; func_003447C8 */
#define CAR_BLOCK_UNK_4A8 0x4A8 /* car+0x104 relative; init.c: gt4_car_place, func_003630C0 */
#define CAR_BLOCK_LAP_COUNT                                                                        \
    0x4AC /* car+0x104 relative; driver.c: gt4_controller_event, func_003F19E0 */
#define CAR_BLOCK_NEXT_SECTOR_INDEX                                                                \
    0x4AE /* func_00358608 +0x4AE: advances on sector crossing, resets at start line */
#define CAR_BLOCK_UNK_4AF 0x4AF /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_REVERSE_LAP_DEBT                                                                 \
    0x4B0 /* func_00358608 +0x4B0: reverse crossings add debt; forward crossings repay it */
#define CAR_BLOCK_FLAGS_4B1 0x4B1 /* car+0x104 relative; pit bits 4/8; func_003F19E0 */
#define CAR_BLOCK_FINISH_DRIVE_STARTED                                                             \
    0x4B2                       /* func_00343D78 +0x4B2: set once when finishing or pulling off */
#define CAR_BLOCK_UNK_4B3 0x4B3 /* car+0x104 relative; init.c: gt4_car_init, func_00363B10 */
#define CAR_BLOCK_NEXT_TIMING_POINT_INDEX                                                          \
    0x4B4 /* func_00358608 +0x4B4: advances at timed points, resets each lap */
#define CAR_BLOCK_UNK_4B6 0x4B6 /* car+0x104 relative; init.c: gt4_car_init, func_00363B10 */
#define CAR_BLOCK_PIT_BOX_INDEX                                                                    \
    0x4B7 /* func_0035B060 +0x4B7: signed index passed to pit-box distance/heading */
#define CAR_BLOCK_PIT_SERVICE_FLAGS                                                                \
    0x4B8 /* func_0035AF18 +0x4B8: tyre/fuel/player-selection service bits */
#define CAR_BLOCK_PIT_SERVICE_COUNTDOWN                                                            \
    0x4BA                         /* func_0035C0B8 +0x4BA: decremented during tyre/fuel service */
#define CAR_BLOCK_UNK_4BC 0x4BC   /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_FUEL_TANK 0x4CC /* car+0x104 relative; race_game_bspec.inc: bspec_frame */
#define CAR_BLOCK_UNK_4D0 0x4D0   /* car+0x104 relative; race_game_bspec.inc: bspec_frame */
#define CAR_BLOCK_ENGINE_BRAKE 0x4EC /* car+0x104 relative; ai.c: throttle_hold, func_003F76E8 */
#define CAR_BLOCK_ENGINE_RPM                                                                       \
    0x4FA /* car+0x104 relative; ai.c: gt4_ai_pit_throttle, func_00463CD8 */
#define CAR_BLOCK_ENGINE_SPEED 0x4FC /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_UNK_504 0x504      /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_CLUTCH_REQ                                                                       \
    0x514                    /* car+0x104 relative; ai.c: gt4_ai_pit_throttle, func_00463CD8 */
#define CAR_BLOCK_GEAR 0x520 /* car+0x104 relative; ai.c: throttle_hold, func_003F76E8 */
#define CAR_BLOCK_CLUTCH_STATE 0x522  /* car+0x104 relative; ai.c: gt4_ai_model, func_00465078 */
#define CAR_BLOCK_UNK_524 0x524       /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_UNK_526 0x526       /* car+0x104 relative; ai.c: gt4_ai_apply, func_00464F38 */
#define CAR_BLOCK_SPEED 0x53C         /* car+0x104 relative; ai.c: pedals, func_00464770 */
#define CAR_BLOCK_LAT_SPEED 0x540     /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_VELOCITY 0x544      /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_BLOCK_UNK_548 0x548       /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_BLOCK_UNK_54C 0x54C       /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_FIELD_REVERSE_ON 0x566    /* driver.c CAR_REVERSE_ON declaration */
#define CAR_FIELD_BUTTON 0x567        /* command byte 2; driver.c CAR_BUTTON declaration */
#define CAR_BLOCK_ANGLES 0x568        /* car+0x104 relative; CAR_ANGLES in car.h */
#define CAR_FIELD_THROTTLE_MODE 0x568 /* driver.c CAR_THROTTLE_MODE declaration */
#define CAR_BLOCK_UNK_56C 0x56C       /* car+0x104 relative; chassis.c: jack_hold, func_0036A318 */
#define CAR_BLOCK_HEADING                                                                          \
    0x570 /* func_0035B060 block+0x570: pit-box heading passed as third rotation angle */
#define CAR_BLOCK_YAW_RATE 0x57C        /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_BLOCK_UNK_580 0x580         /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_BLOCK_UNK_584 0x584         /* car+0x104 relative; ai.c: model_stop, func_00464C80 */
#define CAR_FIELD_COURSE_DISTANCE 0x5A8 /* course distance at step end; func_003447C8 */
#define CAR_FIELD_UNK_5AC 0x5AC         /* ground.c: gt4_car_ground_info, func_00369888 */
#define CAR_FIELD_LAP_COUNT 0x5B0       /* signed lap count; race.c race_over, func_00343C20 */
#define CAR_FIELD_POSITION 0x5B3        /* position starting at 1; func_00365BC0 */
#define CAR_FIELD_FLAGS_5B5 0x5B5 /* steering bit 0 and retire bits 4/8; driver.c / func_00353218 */
#define CAR_FIELD_UNK_5B7 0x5B7   /* license_session.c: gap, func_0035E740 */
#define CAR_FIELD_UNK_5BC 0x5BC   /* race_game_hud.inc: race_game_draw_hud */
#define CAR_BLOCK_UNK_5C1                                                                          \
    0x5C1 /* car+0x104 relative; driver.c: gt4_controller_event, func_003F19E0 */
#define CAR_BLOCK_PIT_WRONG_WAY_SPEED_LEVEL                                                        \
    0x5C3 /* func_0035B060 block+0x5C3: 0, 1 above 10 km/h, 2 above 40 km/h in wrong-way pit branch */
#define CAR_BLOCK_VIEW_YAW_LAG 0x5FC /* car+0x104 relative; race_game_license.inc: license_step */
#define CAR_BLOCK_VIBRATION 0x604    /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_UNK_605 0x605      /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_UNK_606 0x606      /* car+0x104 relative; init.c: gt4_car_rest, func_00363560 */
#define CAR_BLOCK_LAP_TIMING_REFERENCE                                                             \
    0x614 /* func_00358608 +0x614: elapsed-lap subtraction; func_003659E8 finish ordering */
#define CAR_FIELD_UNK_62B 0x62B /* gearbox.c: shift_bias, func_0036C098 */
#define CAR_FIELD_UNK_62C 0x62C /* gearbox.c: shift_bias, func_0036C098 */
#define CAR_FIELD_UNK_62D 0x62D /* gearbox.c: shift_bias, func_0036C098 */
#define CAR_BLOCK_PIT_STOP_REQUEST_STATE                                                           \
    0x62E /* func_0035B060 block+0x62E: race-requested stop, states 0/1/2; physics notes Laps */
#define CAR_FIELD_UNK_62E 0x62E /* gearbox.c: auto_gear, func_0036C160 */
#define CAR_BLOCK_UNK_62F 0x62F /* car+0x104 relative; init.c: gt4_car_init, func_00363B10 */
#define CAR_BLOCK_UNK_632 0x632 /* car+0x104 relative; init.c: gt4_car_init, func_00363B10 */
#define CAR_BLOCK_FFB_RAMP_COUNTDOWN                                                               \
    0x633 /* func_00353ED8 block+0x633: starts at 30; driver force-feedback output decrements/ramp-scales */
#define CAR_BLOCK_PIT_MENU_COUNTDOWN                                                               \
    0x64C /* func_0035BCC8/0035BCE8 block+0x64C: timeout, zero or -1 after player touch */
#define CAR_BLOCK_UNK_64E 0x64E /* car+0x104 relative; car.c: pit_service, func_0035C0B8 */
#define CAR_BLOCK_UNK_64F 0x64F /* car+0x104 relative; car.c: pit_service, func_0035C0B8 */
#define CAR_BLOCK_COURSE_PROGRESS_RATE                                                             \
    0x650 /* func_00358608 +0x650: signed distance change per frame times 60 */
#define CAR_BLOCK_TIMED_START_STATE                                                                \
    0x65B /* func_00358608 block+0x65B: 1 awaiting timed start crossing, then 2 */
#define CAR_FIELD_UNK_694 0x694       /* session.c: tt_start, func_00363528 */
#define CAR_FIELD_UNK_6C4 0x6C4       /* init.c: gt4_car_init, func_00363B10 */
#define CAR_FIELD_UNK_6C6 0x6C6       /* race.c: gt4_car_race_end, func_0034C158 */
#define CAR_FIELD_UNK_712 0x712       /* race_game_bspec.inc: bspec_frame */
#define CAR_FIELD_UNK_718 0x718       /* race_run.c: gt4_race_run_hud, func_00343B50 */
#define CAR_FIELD_BEST_LAP_TIME 0x71C /* best lap time; func_0035E3C8 / 0035E550 */
#define CAR_FIELD_UNK_72C 0x72C       /* driver.c: reference_ground, func_00350C20 */
#define CAR_FIELD_UNK_733 0x733       /* ground.c: gt4_car_ground_info, func_00369888 */
#define CAR_FIELD_UNK_734 0x734       /* ground.c: gt4_car_ground_info, func_00369888 */
#define CAR_FIELD_UNK_736 0x736       /* race.c: gt4_race_finish_handover, func_00366588 */
#define CAR_FIELD_UNK_768 0x768       /* race_game.c: update_drivers */
#define CAR_FIELD_UNK_788 0x788       /* init.c: cvt_init, func_00368260 */
#define CAR_FIELD_UNK_78C 0x78C       /* init.c: gt4_car_init, func_00363B10 */
#define CAR_FIELD_UNK_78D 0x78D       /* init.c: gt4_car_init, func_00363B10 */
#define CAR_FIELD_PREVIOUS_COURSE_DISTANCE 0x99C /* course distance at step start; func_003447C8 */
#define CAR_FIELD_UNK_1080 0x1080                /* race_game.c: update_drivers */

#define CAR_BLOCK_SIGNAL_GUARD_TAG                                                                 \
    0x5fb /* func_0035ACD8 block+0x5FB: table tag suppresses zero-tag signals */

#define CAR_BLOCK_SIGNAL_KIND 0x5fa /* func_0035ACD8 block+0x5FA: selected signal kind */

#define CAR_BLOCK_ORIENTATION_BASIS_2                                                              \
    0x24 /* func_0035B060 block+0x24: third vector output of gt4_rotation_from_angles */

#define CAR_FIELD_DRIVING_CONTROL_MODE                                                             \
    0x56a /* func_0035ADF0 car+0x56A: same control-mode byte as block+0x466 */

#endif
