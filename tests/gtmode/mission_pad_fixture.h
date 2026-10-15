/* P: test-driver policy, not GT4 AI or a dynamics replacement. Only a pad
   command is produced from read-only course/car geometry. All gains,
   lookahead and speed choices below are explicit fixture input. */
#include <math.h>
#include "fields_conductor.h"
/* Original: func_0034C518 +0x00 */
/* Original: func_0046BB48 +0x00 */
/* Choice of an authored line
   is a test-driver input policy; interpolation is the unchanged native API. */
static int mission_fixture_point(gt4_race_run *run, float distance, float out[3])
{
    const char *line_env = getenv("GT4_MISSION_LINE");
    if (!line_env)
        return gt4_race_run_course_point(run, distance, out);
    int line = atoi(line_env);
    float point[3];
    if (line < 0 || line > 6 ||
        !gt4_line_course_point(gt4_race_run_conductor(run) + CONDUCTOR_FIELD_LINES_U, line,
                               distance, point))
        return 0;
    out[0] = point[0];
    out[1] = point[2];
    out[2] = -point[1];
    return 1;
}

/* P: preview/braking policy of the external pad test driver. The desired
   acceleration and distances are fixture choices, never game parameters. */
static float mission_fixture_target(gt4_race_run *run, float distance, float speed)
{
    float target = 100.f;
    const char *lateral_env = getenv("GT4_MISSION_LATERAL_TARGET");
    float lateral = lateral_env ? (float)atof(lateral_env) : 8.f;
    for (unsigned sample = 0; sample < 12; sample++) {
        float ahead = 10.f + sample * fmaxf(10.f, speed * .2f);
        float a[3], b[3], c[3];
        if (!mission_fixture_point(run, distance + ahead, a) ||
            !mission_fixture_point(run, distance + ahead + 10.f, b) ||
            !mission_fixture_point(run, distance + ahead + 20.f, c))
            continue;
        float ax = b[0] - a[0], az = b[2] - a[2];
        float bx = c[0] - b[0], bz = c[2] - b[2];
        float bend = fabsf(atan2f(ax * bz - az * bx, ax * bx + az * bz)) / 10.f;
        if (bend > .0001f)
            target = fminf(target, sqrtf(lateral / bend + 12.f * ahead));
    }
    return target;
}

/* P: drafting/overtaking decisions of the external pad driver. Only
   read-only geometry is observed; all wake/drag effects remain native. */
static float mission_fixture_lane(gt4_race_run *run, uint32_t car)
{
    float nearest = 600.f, lane = 0;
    const char *gap_env = getenv("GT4_MISSION_PASSING_GAP");
    float passing_gap = gap_env ? (float)atof(gap_env) : 12.f;
    float fx = GT4_F32(car + CAR_MATRIX), fy = GT4_F32(car + CAR_MATRIX + 4);
    for (int i = 1; i < gt4_race_run_cars(run); i++) {
        uint32_t other = gt4_race_run_car(run, i);
        float dx = GT4_F32(other + CAR_POSITION) - GT4_F32(car + CAR_POSITION);
        float dy = GT4_F32(other + CAR_POSITION + 4) - GT4_F32(car + CAR_POSITION + 4);
        float gap = fx * dx + fy * dy;
        if (gap <= 0 || gap >= nearest || fabsf(fx * dy - fy * dx) > 15.f)
            continue;
        float d = GT4_F32(other + CAR_FIELD_COURSE_DISTANCE), a[3], b[3];
        if (!mission_fixture_point(run, d, a) || !mission_fixture_point(run, d + 5, b))
            continue;
        float x = b[0] - a[0], z = b[2] - a[2], length = hypotf(x, z);
        if (length <= 0)
            continue;
        lane = ((GT4_F32(other + CAR_POSITION) - a[0]) * z +
                (GT4_F32(other + CAR_POSITION + 4) + a[2]) * x) /
               length;
        if (gap < passing_gap)
            lane += lane > 0 ? -3.f : 3.f;
        nearest = gap;
    }
    return fmaxf(-5.f, fminf(5.f, lane));
}

static void mission_pad_fixture(gt4_race_run *run, uint32_t car, uint8_t command[32])
{
    float speed = GT4_F32(car + CAR_ABS_SPEED), point[3];
    const char *look_env = getenv("GT4_MISSION_LOOK_FACTOR");
    float look_factor = look_env ? (float)atof(look_env) : .5f;
    float look = 15.f + fabsf(speed) * look_factor;
    float distance = GT4_F32(car + CAR_FIELD_COURSE_DISTANCE);
    if (!mission_fixture_point(run, distance + look, point))
        return;
    const char *lane_env = getenv("GT4_MISSION_LANE");
    if (lane_env || getenv("GT4_MISSION_DRAFT")) {
        float ahead[3], lane = lane_env ? (float)atof(lane_env) : mission_fixture_lane(run, car);
        if (mission_fixture_point(run, distance + look + 5, ahead)) {
            float x = ahead[0] - point[0], z = ahead[2] - point[2], length = sqrtf(x * x + z * z);
            if (length > 0) {
                point[0] += z / length * lane;
                point[2] -= x / length * lane;
            }
        }
    }
    float dx = point[0] - GT4_F32(car + CAR_POSITION);
    float dz = point[2] + GT4_F32(car + CAR_POSITION + 4);
    float fx = GT4_F32(car + CAR_MATRIX), fz = -GT4_F32(car + CAR_MATRIX + 4);
    float angle = atan2f(fz * dx - fx * dz, fx * dx + fz * dz);
    const char *gain_env = getenv("GT4_MISSION_GAIN");
    float gain = gain_env ? (float)atof(gain_env) : .6f;
    float steer = fmaxf(-1.f, fminf(1.f, angle * gain));
    int16_t axis = (int16_t)(steer * 4096), brake = fabsf(angle) > .2f && speed > 20.f,
            throttle = !brake;
    if (getenv("GT4_MISSION_PREDICTIVE")) {
        float target = mission_fixture_target(run, distance, speed);
        float pedal = fmaxf(0.f, fminf(1.f, (speed - target) / 5.f));
        command[0] = 7;
        brake = (int16_t)(pedal * 4096.f);
        throttle = brake ? 0 : 4096;
    } else {
        command[0] = 1;
    }
    command[1] = 2;
    memcpy(command + 4, &axis, 2);
    memcpy(command + 8, &throttle, 2);
    memcpy(command + 10, &brake, 2);
}
