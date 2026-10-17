/* Native GT4R v1 playback compatibility. Live modes share the EE arithmetic. */
#ifndef MOON_REPLAY_RUN_H
#define MOON_REPLAY_RUN_H
#include "race_run.h"
gt4_race_run *legacy_gt4_race_run_open(const char *disc, const gt4_race_desc *desc, char *message,
                                       size_t message_size);
int legacy_gt4_race_run_frame(gt4_race_run *run);
#endif
