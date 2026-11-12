/* P: the launcher's Scene (GENERAL; Dante: "change the menu room, like Euro Truck Sim 2"):
   where the car stands. The Room (the default) is the generated white sweep; the others are
   the disc's own Photo Travel places and courses, drawn live through the race's renderer
   with the player's car in them (a place's PhotoMode spot, a course's first grid slot). The
   launcher's cameras, written in the car's frame, become cameras of that world; they stay
   above the road and come closer when the ground rises between them and the car. */
#include "photo_mode.h"
typedef struct {
    const char *code, *name; /* code: /crs/<code>, NULL for the room */
} launcher_scene_entry;
static const launcher_scene_entry launcher_scenes[] = {
    {NULL, "Room"},
    {"ss_gion", "Gion, Kyoto"},
    {"ss_nanzenji", "Nanzenji, Kyoto"},
    {"ss_sagano", "Sagano, Kyoto"},
    {"ss_tsumago", "Tsumago"},
    {"ss_tsukiji", "Tsukiji, Tokyo"},
    {"ss_shibuya", "Shibuya, Tokyo"},
    {"ss_times", "Times Square"},
    {"ss_brooklyn", "Brooklyn"},
    {"ss_louisburg", "Louisburg Square"},
    {"ss_fremont", "Fremont Street"},
    {"ss_grandcanyon", "Grand Canyon (Photo)"},
    {"ss_sanmarco", "San Marco, Venice"},
    {"ss_rialt", "Rialto, Venice"},
    {"ss_forum", "Forum"},
    {"ss_snow", "Snow"},
    {"tsukuba", "Tsukuba"},
    {"suzuka", "Suzuka"},
    {"motegi", "Motegi"},
    {"new_fisco", "Fuji Speedway"},
    {"laguna", "Laguna Seca"},
    {"infineon", "Infineon"},
    {"nurburgring", "Nurburgring"},
    {"sarthe", "Le Mans"},
    {"montecarlo", "Cote d'Azur"},
    {"newyork", "New York"},
    {"hongkong", "Hong Kong"},
    {"seattle", "Seattle"},
    {"deepforest", "Deep Forest"},
    {"trialmt", "Trial Mountain"},
    {"grandvalley", "Grand Valley"},
    {"apricot", "Apricot Hill"},
    {"autumn", "Autumn Ring"},
    {"highspeedring", "High Speed Ring"},
    {"yosemite", "El Capitan"},
    {"tahiti_maze", "Tahiti Maze"},
};
#define LAUNCHER_SCENES (int)(sizeof launcher_scenes / sizeof *launcher_scenes)
static struct {
    int index, loading, failed, live;  /* index: the chosen entry; live: frames drawn */
    int photo;                         /* a Photo Travel place (else a course) */
    int mirror;                        /* the cameras on the car's other side */
    char car[48];                      /* the car the course was read with */
    int color;
    race_view *view;
    float pos[3], rot[9];              /* where the car stands in the course's world */
    float turn, reach; /* a place: the cameras turned to its photographer's side, and no
                          further from the car than they stand (0: no limit) */
    plat_texture *image;               /* a live frame, for the Options column */
} launcher_scene = {0};
static int launcher_scene_find(const char *code)
{
    for (int i = 1; code && *code && i < LAUNCHER_SCENES; i++)
        if (!strcmp(launcher_scenes[i].code, code))
            return i;
    return 0;
}
static void launcher_scene_pick(showroom *cars, int index)
{
    (void)cars;
    launcher_scene.index = (index % LAUNCHER_SCENES + LAUNCHER_SCENES) % LAUNCHER_SCENES;
    race_view_close(launcher_scene.view);
    launcher_scene.view = NULL;
    launcher_scene.loading = launcher_scene.failed = launcher_scene.live = 0;
    launcher_scene.turn = launcher_scene.reach = 0;
    launcher_scene.mirror = 0;
    launcher_scene.car[0] = 0;
    studio_scene_shade = 0;
}
/* the car's spot: a place's PhotoMode placement, a course's first grid slot on its road */
static void launcher_scene_spot(race_view *v, int photo)
{
    const gt4_course *course = race_view_course(v);
    if (photo) {
        gt4_photo_mode s;
        float yaw = 0;
        memset(&s, 0, sizeof s);
        int placed = gt4_photo_mode_init(&s, course);
        if (placed) {
            memcpy(launcher_scene.pos, s.car, sizeof launcher_scene.pos);
            yaw = s.car_angle * 3.14159265f / 180;
        }
        float c = cosf(yaw), sn = sinf(yaw);
        const float rot[9] = {c, 0, -sn, 0, 1, 0, sn, 0, c}; /* photo_travel.c pose */
        memcpy(launcher_scene.rot, rot, sizeof rot);
        if (placed) {
            /* the place's own camera is the side known to be clear (no fence, no wall
               between it and the car): the room's framing turns there and keeps its reach */
            float d[3] = {s.eye[0] - s.car[0], s.eye[1] - s.car[1], s.eye[2] - s.car[2]};
            float lx = rot[0] * d[0] + rot[3] * d[1] + rot[6] * d[2],
                  lz = rot[2] * d[0] + rot[5] * d[1] + rot[8] * d[2];
            launcher_scene.turn = atan2f(lx, lz) - atan2f(LCAM_HOME_EYE[0], LCAM_HOME_EYE[2]);
            launcher_scene.reach = sqrtf(lx * lx + lz * lz) * 1.1f;
        }
    } else {
        gt4_grid g = gt4_course_grid(course, 0);
        float y = g.pos[1], yaw = g.heading + 3.14159265f, cs = cosf(yaw), sn = sinf(yaw);
        gt4_course_height(course, g.pos[0], g.pos[2], g.pos[1] + 2.f, &y);
        const float rot[9] = {cs, 0, sn, 0, 1, 0, -sn, 0, cs}; /* race_view.c place() */
        memcpy(launcher_scene.rot, rot, sizeof rot);
        launcher_scene.pos[0] = g.pos[0], launcher_scene.pos[1] = y, launcher_scene.pos[2] = g.pos[2];
        /* walls are not in the road data, but the road is: the cameras look from the side
           with more road between the car and them (the grid's first slot is often by a wall) */
        int road[2] = {0, 0};
        for (int side = 0; side < 2; side++)
            for (int i = 1; i <= 8; i++) {
                float l[3] = {LCAM_HOME_EYE[0] * i / 8.f * (side ? -1 : 1), 0,
                              LCAM_HOME_EYE[2] * i / 8.f * (side ? -1 : 1)},
                      h;
                float wx = g.pos[0] + rot[0] * l[0] + rot[2] * l[2],
                      wz = g.pos[2] + rot[6] * l[0] + rot[8] * l[2];
                if (!gt4_course_height(course, wx, wz, y + 3.f, &h) || fabsf(h - y) > 1.5f)
                    break;
                road[side]++;
            }
        launcher_scene.mirror = road[1] > road[0];
    }
    /* the car follows the road under its wheels (a flat car on a slope sinks on one side) */
    const float *r = launcher_scene.rot;
    float h[4], sum = 0;
    int found = 0;
    for (int k = 0; k < 4; k++) {
        float lx = k & 1 ? .75f : -.75f, lz = k & 2 ? 1.25f : -1.25f;
        float wx = launcher_scene.pos[0] + r[0] * lx + r[2] * lz,
              wz = launcher_scene.pos[2] + r[6] * lx + r[8] * lz;
        if (gt4_course_height(course, wx, wz, launcher_scene.pos[1] + 1.f, &h[k]) &&
            fabsf(h[k] - launcher_scene.pos[1]) < .6f)
            found++, sum += h[k];
    }
    if (found == 4) {
        /* the plane through the four: its slope across (x) and along (z) the car */
        float sx = ((h[1] + h[3]) - (h[0] + h[2])) / (2 * 1.5f),
              sz = ((h[2] + h[3]) - (h[0] + h[1])) / (2 * 2.5f);
        float ax[3] = {r[0], r[3] + sx, r[6]}, az[3] = {r[2], r[5] + sz, r[8]};
        float n[3] = {az[1] * ax[2] - az[2] * ax[1], az[2] * ax[0] - az[0] * ax[2],
                      az[0] * ax[1] - az[1] * ax[0]};
        float ln = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (ln > 1e-4f && n[1] / ln > .85f) {
            for (int c = 0; c < 3; c++)
                n[c] /= ln;
            float d = n[0] * az[0] + n[1] * az[1] + n[2] * az[2];
            float z[3] = {az[0] - n[0] * d, az[1] - n[1] * d, az[2] - n[2] * d};
            float lz = sqrtf(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
            for (int c = 0; c < 3; c++)
                z[c] /= lz;
            float x[3] = {n[1] * z[2] - n[2] * z[1], n[2] * z[0] - n[0] * z[2],
                          n[0] * z[1] - n[1] * z[0]};
            for (int c = 0; c < 3; c++) {
                launcher_scene.rot[c * 3] = x[c];
                launcher_scene.rot[c * 3 + 1] = n[c];
                launcher_scene.rot[c * 3 + 2] = z[c];
            }
            launcher_scene.pos[1] = sum / 4;
        }
    }
    race_view_set_pose(v, launcher_scene.pos, launcher_scene.rot);
    const float wheels[4][3] = {{0}}; /* straight, as photo_travel.c poses a place's car */
    race_view_set_wheels(v, wheels);
}
/* Before the launcher's frame: reads the chosen course with the room's car. */
static void launcher_scene_step(showroom *cars, const char *code)
{
    const launcher_scene_entry *e = &launcher_scenes[launcher_scene.index];
    if (!e->code || launcher_scene.failed || !studio_disc || !code)
        return;
    int photo = !strncmp(e->code, "ss_", 3), color = showroom_color_index(cars);
    if (launcher_scene.view && !launcher_scene.loading &&
        (strcmp(launcher_scene.car, code) || launcher_scene.color != color))
        launcher_scene_pick(cars, launcher_scene.index); /* another car: read it again */
    if (!launcher_scene.view) {
        launcher_scene.view = race_view_open(studio_disc);
        if (launcher_scene.view)
            race_view_set_parts(launcher_scene.view, studio_wheel, studio_wing);
        if (!launcher_scene.view ||
            !(photo ? race_view_request_photo(launcher_scene.view, e->code, code, color)
                    : race_view_request(launcher_scene.view, e->code, code, color))) {
            launcher_scene.failed = 1;
            printf("studio: scene %s is unavailable\n", e->code);
            return;
        }
        snprintf(launcher_scene.car, sizeof launcher_scene.car, "%s", code);
        launcher_scene.color = color;
        launcher_scene.loading = 1;
    }
    if (launcher_scene.loading) {
        int ready = race_view_poll(launcher_scene.view, 0);
        if (ready < 0) {
            launcher_scene.failed = 1;
            printf("studio: scene %s is unavailable\n", e->code);
        }
        if (ready != 1)
            return;
        launcher_scene.loading = 0;
        launcher_scene.photo = photo;
        race_view_daylight_time(launcher_scene.view, 0);
        launcher_scene_spot(launcher_scene.view, photo);
        printf("studio: scene %s, the car at %.2f %.2f %.2f\n", e->code, launcher_scene.pos[0],
               launcher_scene.pos[1], launcher_scene.pos[2]);
    }
}
static int launcher_scene_ready(void)
{
    return launcher_scenes[launcher_scene.index].code && launcher_scene.view &&
           !launcher_scene.loading && !launcher_scene.failed;
}
/* the road height under (x, z) at or below y, or the car's own ground off the road; *off set
   when there is no road there */
static float launcher_scene_ground(float x, float z, float y, int *off)
{
    float h = launcher_scene.pos[1];
    int on = gt4_course_height(race_view_course(launcher_scene.view), x, z, y + .5f, &h);
    if (!on)
        h = launcher_scene.pos[1];
    if (off)
        *off = !on;
    return h;
}
/* The room's camera (eye, target in the room, where the car stands at car, angle) in the
   course: the same view of the car, drawn in where the ground rises across it and kept
   above the road. Inside video_begin_scene, over the whole window. */
static void launcher_scene_draw(const float eye[3], const float target[3], const float car[3],
                                float angle, float fov, int raster)
{
    if (!launcher_scene_ready())
        return;
    /* the room -> the car's frame (lcam_place's inverse) -> the course */
    float out[2][3];
    const float *room[2] = {eye, target};
    float cs = cosf(angle), sn = sinf(angle);
    for (int k = 0; k < 2; k++) {
        float x = room[k][0] - car[0], y = room[k][1] - car[1], z = room[k][2] - car[2];
        float in[3] = {cs * x - sn * z, y, sn * x + cs * z};
        if (launcher_scene.mirror)
            in[0] = -in[0];
        float tc = cosf(launcher_scene.turn), ts = sinf(launcher_scene.turn), ix = in[0];
        in[0] = ix * tc + in[2] * ts, in[2] = in[2] * tc - ix * ts;
        float flat = sqrtf(in[0] * in[0] + in[2] * in[2]);
        if (launcher_scene.reach > 0 && flat > launcher_scene.reach)
            in[0] *= launcher_scene.reach / flat, in[2] *= launcher_scene.reach / flat;
        const float *r = launcher_scene.rot;
        for (int c = 0; c < 3; c++)
            out[k][c] = launcher_scene.pos[c] + r[c * 3] * in[0] + r[c * 3 + 1] * in[1] +
                        r[c * 3 + 2] * in[2];
    }
    float *e = out[0], *t = out[1];
    for (int i = 1; i <= 12; i++) {
        float f = i / 12.f, p[3] = {t[0] + (e[0] - t[0]) * f, t[1] + (e[1] - t[1]) * f,
                                    t[2] + (e[2] - t[2]) * f};
        /* on a course the camera also stays over the road: off it stand the walls, the
           barriers and the stands, which the road data does not have */
        int off = 0;
        if (launcher_scene_ground(p[0], p[2], p[1] + 3.f, &off) > p[1] - .25f ||
            (off && !launcher_scene.photo)) {
            float keep = fmaxf((i - 1) / 12.f, .35f);
            for (int c = 0; c < 3; c++)
                e[c] = t[c] + (e[c] - t[c]) * keep;
            break;
        }
    }
    float floor = launcher_scene_ground(e[0], e[2], e[1] + 3.f, NULL) + .3f;
    if (e[1] < floor)
        e[1] = floor;
    float f[3] = {t[0] - e[0], t[1] - e[1], t[2] - e[2]};
    float lf = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (lf < 1e-4f)
        return;
    for (int c = 0; c < 3; c++)
        f[c] /= lf;
    float r[3] = {-f[2], 0, f[0]}, lr = sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (lr < 1e-4f)
        r[0] = 1, r[2] = 0, lr = 1;
    r[0] /= lr, r[2] /= lr;
    float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
    const float rows[9] = {r[0], r[1], r[2], u[0], u[1], u[2], -f[0], -f[1], -f[2]};
    float sy = 1.f / tanf((fov > 1 ? fov : 32.f) * 3.14159265f / 360.f);
    race_view_set_camera(launcher_scene.view, e, rows, sy * 448.f / (float)raster, sy, .1f);
    plat_video_set_view(640.f, 448.f);
    race_view_draw(launcher_scene.view);
    if (++launcher_scene.live == 3) {
        launcher_scene.image = video_grab(launcher_scene.image, 0, 0, 640.f, 448.f);
        printf("studio: scene %s live\n", launcher_scenes[launcher_scene.index].code);
    }
    studio_scene_shade = 1;
}
static void launcher_scene_close(showroom *cars)
{
    (void)cars;
    studio_scene_shade = 0;
    race_view_close(launcher_scene.view);
    plat_texture_destroy(launcher_scene.image);
    memset(&launcher_scene, 0, sizeof launcher_scene);
}
static const char *studio_scene_name(void)
{
    return launcher_scenes[launcher_scene.index].name;
}
/* a shot waits while the chosen scene is still being read */
static int launcher_scene_busy(void)
{
    return launcher_scenes[launcher_scene.index].code && !launcher_scene.failed &&
           launcher_scene.live < 4;
}
