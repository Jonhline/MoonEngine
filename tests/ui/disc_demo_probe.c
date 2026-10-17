/* Integration of original teacher pad tapes with the existing native run.
   This checks consumption/return isolation, not trajectory fidelity. */
#include <SDL.h>
#include "plat_sdl.h"
#include "career_license_disc.h"
#include "career_license.h"
#include "menu.h"
#include "../test_process.h"
#include "../../src/app/race_game.c"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "demo line %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int ready(race_game *g)
{
    int n = 0;
    for (int i = 0; i < 4000 && !n; i++) {
        SDL_PumpEvents();
        SDL_Delay(5);
        n = race_game_poll(g);
    }
    return n;
}
/* The real frontend consumes every command and returns without an award. */
static int teacher_catalog(race_game *g, gt4_career_race_lookup_data *lookup, int negative)
{
    static const char *groups[] = {"l0b", "l0a", "lib", "lia", "l0s"};
    unsigned count = 0, guides = 0;
    for (unsigned group = 0; group < 5; group++)
        for (unsigned number = 1; number <= 16; number++) {
            char code[32], path[80], error[256];
            snprintf(code, sizeof code, "%s%04u", groups[group], number);
            gt4_career_license_exam exam;
            CHECK(gt4_career_license_prepare(&exam, g->disc, lookup, code, error, sizeof error));
            CHECK(race_game_start_prepared(g, &exam.desc, error, sizeof error));
            CHECK(ready(g) == 1);
            CHECK(race_game_license_demo(g, code, error, sizeof error));
            CHECK(ready(g) == 1);
            snprintf(path, sizeof path, "/replay/license/us%s", code);
            gt4_replay_file reference;
            CHECK(race_game_read_replay(g, path, &reference, error, sizeof error));
            size_t steps = reference.steps;
            CHECK(g->demonstrating && g->replaying && !g->legacy_replay_math);
            CHECK(g->tape_n == steps && !memcmp(g->tape, reference.tape, steps * 32));
            float before[3], after[3], rotation[9], wheels[4][3];
            int guide = exam.desc.license == 2 && gt4_race_run_cars(g->run) > 1;
            if (guide)
                CHECK(gt4_race_run_pose(g->run, 1, before, rotation, wheels));
            if (negative)
                g->tape_n--;
            for (size_t tick = 0; tick < steps + 600 && !race_game_over(g); tick++)
                race_game_update(g, 1. / 60);
            CHECK(g->tape_at == steps && race_game_over(g) && !g->result);
            if (guide) {
                CHECK(gt4_race_run_pose(g->run, 1, after, rotation, wheels));
                float travel = 0;
                for (int axis = 0; axis < 3; axis++) {
                    CHECK(isfinite(after[axis]));
                    travel += (after[axis] - before[axis]) * (after[axis] - before[axis]);
                }
                CHECK(travel > 1); /* Guide motion, not a console pose tolerance. */
                guides++;
            }
            CHECK(!g->result && !g->license_log_collecting);
            CHECK(race_game_end_demo(g, error, sizeof error));
            CHECK(ready(g) == 1 && !g->demonstrating && !g->replaying);
            gt4_replay_free(&reference);
            printf(
                "PASS demonstration %s: %zu original commands, guide=%d, no award, fresh exam return\n",
                code, steps, guide);
            fflush(stdout);
            count++;
        }
    CHECK(count == 80 && guides == 12);
    return 0;
}

static int complete_playback(race_game *g, const gt4_replay_file *file, int negative,
                             float final[3])
{
    char error[256];
    CHECK(race_game_replay_import(g, file));
    CHECK(race_game_replay(g, error, sizeof error));
    CHECK(ready(g) == 1 && g->replaying && !g->legacy_replay_math);
    if (negative)
        g->tape_n--;
    float rotation[9], wheels[4][3];
    for (size_t tick = 0; tick < file->steps; tick++) {
        race_game_update(g, 1. / 60);
        CHECK(gt4_race_run_pose(g->run, 0, final, rotation, wheels));
        for (int axis = 0; axis < 3; axis++)
            CHECK(isfinite(final[axis]));
    }
    CHECK(g->tape_at == file->steps);
    race_game_stop(g);
    return 0;
}

static int attract_playback(race_game *g, int negative)
{
    char error[256], path[64] = {0};
    menu *m = menu_open(g->vol, "arcade", error, sizeof error);
    CHECK(m);
    ui_project *ui = menu_ui(m);
    int top = -1;
    for (int root = 0; root < ui_root_count(ui); root++)
        if (!strcmp(ui_root_name(ui, root), "TopRoot"))
            top = root;
    CHECK(top >= 0);
    menu_enter(m, top);
    for (unsigned tick = 0; tick < 18000 && !path[0]; tick++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        menu_take_attract_replay(path, sizeof path);
    }
    CHECK(path[0] && menu_attract_active());
    gt4_replay_file file;
    CHECK(race_game_read_replay(g, path, &file, error, sizeof error));
    float final[3];
    CHECK(!complete_playback(g, &file, negative, final));
    menu_attract_finish(m, 1);
    CHECK(!menu_attract_active());
    printf("PASS idle Arcade attract -> %s -> %zu commands -> cancel/menu return\n", path,
           file.steps);
    gt4_replay_free(&file);
    menu_close(m);
    return 0;
}

/* Commands -> native resimulation -> native file -> Logger Load Track. */
static int logger_playback(race_game *g, gt4_career_race_lookup_data *lookup, int negative)
{
    char error[256], folder[256], name[64];
    gt4_replay_file commands;
    CHECK(race_game_read_replay(g, "/replay/license/usl0b0001", &commands, error, sizeof error));
    gt4_career_license_exam exam;
    CHECK(gt4_career_license_prepare(&exam, g->disc, lookup, "l0b0001", error, sizeof error));
    exam.desc.manual = commands.manual;
    exam.desc.assist_asm = commands.assist_asm;
    exam.desc.assist_tcs = commands.assist_tcs;
    CHECK(race_game_start_prepared(g, &exam.desc, error, sizeof error));
    CHECK(ready(g) == 1);
    free(g->tape);
    g->tape = malloc(commands.steps * 32);
    CHECK(g->tape);
    memcpy(g->tape, commands.tape, commands.steps * 32);
    g->tape_n = g->tape_cap = commands.steps;
    gt4_replay_free(&commands);
    CHECK(race_game_logger_analyze(g, 1, error, sizeof error));
    unsigned count = g->license_log.count;
    CHECK(count > 0);
    gt4_logger_point *expected = malloc(count * sizeof *expected);
    CHECK(expected);
    memcpy(expected, g->license_log.points, count * sizeof *expected);
    CHECK(race_game_logger_analyze(g, 1, error, sizeof error));
    if (negative)
        expected[0].brake ^= 1;
    CHECK(g->license_log.count == count && !memcmp(expected, g->license_log.points, count * sizeof *expected));
    snprintf(folder, sizeof folder, "build/logger-resimulation-%lu-%llu", test_process_id(),
             (unsigned long long)SDL_GetTicks64());
    gt4_save_init(folder);
    CHECK(gt4_save_mkdir(folder));
    gt4_replay_file saved, loaded;
    CHECK(race_game_replay_export(g, &saved, GT4_REPLAY_FULL));
    CHECK(gt4_replay_store(&saved, name, sizeof name));
    CHECK(race_game_read_replay(g, name, &loaded, error, sizeof error));
    CHECK(!loaded.legacy_math);
    CHECK(race_game_logger_load(g, &loaded, 1, error, sizeof error));
    CHECK(g->log_b.count == count && !memcmp(expected, g->log_b.points, count * sizeof *expected));
    CHECK(g->license_log.count == count && !memcmp(expected, g->license_log.points, count * sizeof *expected));
    printf(
        "PASS Logger: %u packed points identical after repeated analysis, file round-trip and Load Track B; A preserved\n",
        count);
    gt4_replay_free(&loaded);
    gt4_save_shutdown();
    free(expected);
    return 0;
}

/* A synthetic PSU envelope containing real original disc recordings. This is
   import/playback coverage, not an independently recorded memory-card fixture. */
static int imported_playback(race_game *g, int negative)
{
    char folder[256], path[300], error[256];
    snprintf(folder, sizeof folder, "build/replay-import-run-%lu-%llu", test_process_id(),
             (unsigned long long)SDL_GetTicks64());
    gt4_save_init(folder);
    CHECK(gt4_save_mkdir(folder));
    const unsigned demos[] = {0, 9};
    for (unsigned k = 0; k < 2; k++) {
        char disc_path[64];
        snprintf(disc_path, sizeof disc_path, "/replay/Rdemo%u/Rdemo%u", demos[k], demos[k]);
        size_t size;
        uint8_t *raw = gt4_vol_read(g->vol, disc_path, &size);
        CHECK(raw && size <= UINT32_MAX);
        size_t archive_size = 2048 + ((size + 1023) & ~(size_t)1023);
        uint8_t *archive = calloc(1, archive_size);
        CHECK(archive);
        archive[0] = archive[512] = archive[1024] = 0x20;
        uint32_t entries = 3, bytes = (uint32_t)size;
        memcpy(archive + 4, &entries, 4);
        archive[1536] = 0x10;
        memcpy(archive + 1540, &bytes, 4);
        memcpy(archive + 1600, "recording", 10);
        memcpy(archive + 2048, raw, size);
        snprintf(path, sizeof path, "%s/import.psu", folder);
        CHECK(plat_file_write(path, archive, archive_size));
        gt4_media_import report;
        CHECK(gt4_media_import_ps2(g->vol, path, &report) && report.replays == 1);
        char names[8][64];
        int count = gt4_replay_list(names, 8), matched = 0;
        gt4_replay_file source;
        CHECK(race_game_read_replay(g, disc_path, &source, error, sizeof error));
        for (int index = 0; index < count; index++) {
            gt4_replay_file file;
            CHECK(race_game_read_replay(g, names[index], &file, error, sizeof error));
            if (strcmp(source.course, file.course)) {
                gt4_replay_free(&file);
                continue;
            }
            CHECK(!file.legacy_math && source.steps == file.steps && !memcmp(source.tape, file.tape, source.steps * 32));
            float expected[3], actual[3];
            CHECK(!complete_playback(g, &source, 0, expected));
            CHECK(!complete_playback(g, &file, negative, actual));
            CHECK(!memcmp(expected, actual, sizeof expected));
            gt4_replay_free(&file);
            matched++;
        }
        CHECK(matched == 1);
        printf(
            "PASS PSU original recording %u: import, full playback, final pose bit exact to disc source\n",
            demos[k]);
        gt4_replay_free(&source);
        free(archive);
        free(raw);
    }
    gt4_save_shutdown();
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc>=2);
    int negative = argc > 2 && !strcmp(argv[2], "--negative");
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(SDL_GetWindowFromID(1));
    gt4_career_race_lookup_data lookup;
    gt4_career_license_exam exam;
    char error[256];
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    race_game *g = race_game_open(argv[1]);
    CHECK(g);
    if (argc > 2 &&
        (!strcmp(argv[2], "--teacher-catalog") || !strcmp(argv[2], "--teacher-catalog-negative") ||
         !strcmp(argv[2], "--attract") || !strcmp(argv[2], "--attract-negative") ||
         !strcmp(argv[2], "--imported") || !strcmp(argv[2], "--imported-negative") ||
         !strcmp(argv[2], "--logger") || !strcmp(argv[2], "--logger-negative"))) {
        int bad = strstr(argv[2], "negative") != NULL;
        int result = strstr(argv[2], "teacher")   ? teacher_catalog(g, &lookup, bad)
                     : strstr(argv[2], "attract") ? attract_playback(g, bad)
                     : strstr(argv[2], "logger")  ? logger_playback(g, &lookup, bad)
                                                  : imported_playback(g, bad);
        race_game_close(g);
        plat_video_shutdown();
        return result;
    }
    int theater_long = argc > 2 && !strcmp(argv[2], "--theater-long");
    int theater_complete_negative = argc > 2 && !strcmp(argv[2], "--theater-complete-negative");
    int theater_complete =
        theater_complete_negative || (argc > 2 && !strcmp(argv[2], "--theater-complete"));
    int theater_info = argc > 2 && !strcmp(argv[2], "--theater-info");
    int bspec_test =
        argc > 2 && (!strcmp(argv[2], "--disc-bspec") || !strcmp(argv[2], "--disc-bspec-negative"));
    int bspec_negative = argc > 2 && !strcmp(argv[2], "--disc-bspec-negative");
    if (bspec_test || theater_info || theater_long || theater_complete ||
        (argc > 2 && !strcmp(argv[2], "--theater"))) {
        char paths[32][64];
        int count = gt4_disc_replay_list(g->vol, paths, 32, 0);
        CHECK(count==13);
        /* Optional diagnosis of the first title replay beyond the two-second
           catalog smoke check. This does not claim original trajectory parity. */
        int ticks = theater_long ? 5400 : bspec_test ? 1200 : 120;
        if (theater_long)
            SDL_setenv("MOON_RACE_REPLAY_LOG", "1", 1);
        int checked_bspec = 0;
        for (int k = 0; k < (theater_long ? 1 : count); k++) {
            gt4_replay_file file;
            CHECK(race_game_read_replay(g,paths[k],&file,error,sizeof error));
            if (theater_info) {
                /* 00388E38: print raw header +C, entry +198/+19C,
                   +1A0..1A5 and decoded commands before interpreting them. */
                uint32_t header, flags, grid;
                memcpy(&header, file.disc_info + 12, 4);
                memcpy(&flags, file.disc_info + 0x960 + 0x198, 4);
                memcpy(&grid, file.disc_info + 0x960 + 0x19c, 4);
                unsigned throttle = 0, steering = 0, gears = 0;
                for (size_t at = 0; at < file.steps; at++) {
                    const uint8_t *cmd = file.tape + at * 32;
                    throttle += cmd[8] != 0 || cmd[9] != 0;
                    steering += cmd[4] != 0 || cmd[5] != 0;
                    gears += cmd[2] != 0;
                }
                const uint8_t *opt = file.disc_info + 0x960 + 0x1a0;
                printf(
                    "INFO %s %s %s class=%s headerC=%u entry198=%x grid=%x opts=%u,%u,%u,%u,%u,%u steps=%zu throttle=%u steer=%u gear=%u segments=%u\n",
                    paths[k], file.course, file.car, file.disc_info + 16, header, flags, grid,
                    opt[0], opt[1], opt[2], opt[3], opt[4], opt[5], file.steps, throttle, steering,
                    gears, file.disc_segments);
                gt4_replay_free(&file);
                continue;
            }
            if (theater_complete)
                ticks = (int)file.steps;
            unsigned recorded_bspec = file.disc_info[12] == 1;
            CHECK(file.disc_bspec == (int)recorded_bspec);
            if (bspec_test && !recorded_bspec) {
                gt4_replay_free(&file);
                continue;
            }
            if (bspec_negative)
                file.disc_bspec = 0; /* Reproduce the old lost mode, keep the tape unchanged. */
            if (bspec_test) {
                checked_bspec++;
                for (size_t at = 0; at < file.steps; at++) {
                    const uint8_t *cmd = file.tape + at * 32;
                    CHECK(!cmd[4] && !cmd[5] && !cmd[8] && !cmd[9]);
                }
            }
            CHECK(race_game_replay_import(g,&file));
            CHECK(race_game_replay(g,error,sizeof error));
            CHECK(ready(g)==1);
            CHECK(g->replaying && !g->demonstrating);
            if (theater_complete_negative)
                g->tape_n--; /* Prove the full-recording guard catches a dropped command. */
            float p[3], r[9], w[4][3], origin[3];
            CHECK(gt4_race_run_pose(g->run,0,origin,r,w));
            CHECK(g->run_bspec == (int)recorded_bspec);
            fprintf(stderr, "Theater trace: %s course=%s car=%s ticks=%d tape=%zu\n", paths[k],
                    file.course, file.car, ticks, g->tape_n);
            for (int i = 0; i < ticks; i++) {
                race_game_update(g, 1. / 60);
                CHECK(gt4_race_run_pose(g->run,0,p,r,w));
                for (int axis = 0; axis < 3; axis++)
                    CHECK(isfinite(p[axis]));
                if (theater_long && (i + 1) % 300 == 0)
                    fprintf(stderr,
                            "Theater pose: tick=%d tape=%zu frame=%u held=%d pos=%.6f %.6f %.6f\n",
                            i + 1, g->tape_at, g->license_test_frame, g->held, p[0], p[1], p[2]);
            }
            CHECK(g->tape_at>0 && g->tape_at<=(size_t)ticks);
            if (theater_complete)
                CHECK(g->tape_at == file.steps);
            if (bspec_test) {
                float dx = p[0] - origin[0], dz = p[2] - origin[2];
                CHECK(dx*dx+dz*dz>100); /* Motion guard, not a PS2 trajectory tolerance. */
            }
            CHECK(g->disc_segment==file.disc_segments);
            CHECK(gt4_race_run_pose(g->run,0,p,r,w));
            for (int i = 0; i < 3; i++)
                CHECK(isfinite(p[i]));
            race_game_key(g, SDLK_ESCAPE);
            CHECK(race_game_quit(g));
            race_game_stop(g);
            printf(
                "PASS native Theater %s, pad playback, finite pose, recorded checkpoints and cancellation.\n",
                paths[k]);
            fflush(stdout);
            gt4_replay_free(&file);
        }
        if (bspec_test)
            CHECK(checked_bspec == 4);
        race_game_close(g);
        plat_video_shutdown();
        return 0;
    }
    CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,"l0b0001",error,sizeof error));
    CHECK(race_game_start_prepared(g,&exam.desc,error,sizeof error));
    CHECK(ready(g)==1);
    /* A player's previous attempt must survive teacher playback. */
    g->tape = malloc(96);
    CHECK(g->tape);
    memset(g->tape, 0x5a, 96);
    g->tape_n = g->tape_cap = 3;
    g->thumb_on = 2;
    uint8_t *old = g->tape;
    g->license_log.count = 2;
    CHECK(race_game_license_demo(g,NULL,error,sizeof error));
    CHECK(ready(g)==1);
    CHECK(race_game_demonstrating(g) && race_game_replaying(g));
    if (argc > 2 && !strcmp(argv[2], "--telemetry-negative"))
        g->license_log_collecting = 1;
    CHECK(!g->concourse.enabled && !g->license_log_collecting);
    /* Test only: 003F8AB0 skips grade writes for DC=1 demonstrations.
       DC=0 evaluates the same original pad tape as a player attempt. */
    if (getenv("GT4_DEMO_TEST_PLAYER_GRADE"))
        GT4_U32(GT4_U32(gt4_race_run_conductor(g->run)) + 0xdc) = 0;
    gt4_replay_file reference;
    CHECK(race_game_read_replay(g,"/replay/license/usl0b0001",&reference,error,sizeof error));
    if (negative)
        g->tape[4] ^= 1;
    CHECK(g->tape_n==reference.steps && !memcmp(g->tape,reference.tape,reference.steps*32));
    size_t steps = reference.steps;
    uint32_t original_time = reference.race_time;
    printf(
        "B-1 original recording metadata: race=%u best=%u; targets gold=%u silver=%u bronze=%u ms\n",
        reference.race_time, reference.best_lap, gt4_career_license_time(g->last_event_row, 0),
        gt4_career_license_time(g->last_event_row, 1),
        gt4_career_license_time(g->last_event_row, 2));
    gt4_replay_free(&reference);
    /* Each command delivered at the pad boundary must be byte exact. */
    uint8_t command_bytes[32], ev[32] = {0};
    for (size_t i = 0; i < steps; i++) {
        CHECK(host_read(NULL,player,command_bytes,ev));
        CHECK(!memcmp(command_bytes,g->tape+i*32,32));
    }
    g->tape_at = 0;
    float before[3], rot[9], wheels[4][3], after[3];
    CHECK(gt4_race_run_pose(g->run,0,before,rot,wheels));
    for (unsigned i = 0; i < 4000 && !race_game_over(g); i++)
        race_game_update(g, 1. / 60);
    CHECK(g->tape_at==steps && race_game_over(g) && !g->result);
    int32_t teacher_state;
    uint32_t teacher_time;
    CHECK(race_game_license_result(g,&teacher_state,&teacher_time));
    gt4_race_hud teacher_hud;
    CHECK(gt4_race_run_hud(g->run,0,&teacher_hud));
    printf("B-1 teacher native: state=%d time=%u ms lap=%u speed=%.6f; no award/result UI\n",
           teacher_state, teacher_time, teacher_hud.lap_time, teacher_hud.speed);
    if (getenv("GT4_DEMO_TEST_PLAYER_GRADE")) {
        CHECK(teacher_state==(getenv("GT4_DEMO_TEST_GRADE_NEGATIVE")?3:2));
        CHECK(teacher_time==original_time && teacher_time<=gt4_career_license_time(g->last_event_row,0));
    }
    CHECK(gt4_race_run_pose(g->run,0,after,rot,wheels));
    CHECK(memcmp(before,after,sizeof before));
    CHECK(race_game_end_demo(g,error,sizeof error));
    CHECK(ready(g)==1);
    CHECK(!g->demonstrating && !g->replaying && g->tape==old && g->tape_n==3 && g->tape_cap==3 && g->thumb_on==2 && g->license_log.count==2);
    for (unsigned i = 0; i < 96; i++)
        CHECK(old[i]==0x5a);
    printf(
        "PASS B-1 teacher: %zu exact pad commands, native motion, no result, prior tape/thumbnail/logger restored.\n",
        steps);
    CHECK(race_game_license_demo(g,"l0b0001",error,sizeof error));
    CHECK(ready(g)==1);
    race_game_key(g, SDLK_ESCAPE);
    CHECK(race_game_quit(g));
    CHECK(race_game_end_demo(g,error,sizeof error));
    CHECK(ready(g)==1 && g->tape==old && g->tape_n==3);
    puts("PASS teacher cancellation and fresh exam return.");
    race_game_close(g);
    plat_video_shutdown();
    return 0;
}
