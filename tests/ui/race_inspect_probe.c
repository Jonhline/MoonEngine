/* Read-only real-disc race presentation/inspection probe. Local PNG output
   contains user-disc assets and must stay outside Git. Not a PS2 oracle. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "render3d.h"
#include "video_gl.h"
#include "career_license_disc.h"
#include "car_model.h"
#include "vk_render.h"
#include "daylight.h"
static Uint8 fixture_key[SDL_NUM_SCANCODES];
static const Uint8 *fixture_keys(int *n)
{
    if (n)
        *n = SDL_NUM_SCANCODES;
    return fixture_key;
}
static Uint32 fixture_window_flags(SDL_Window *w)
{
    (void)w;
    return SDL_WINDOW_INPUT_FOCUS;
}
/* Exercise the app's actual keyboard code without sending OS input. */
#define SDL_GetKeyboardState fixture_keys
#define SDL_GetWindowFlags fixture_window_flags
#include "../../src/app/race_game.c"
#undef SDL_GetKeyboardState
#undef SDL_GetWindowFlags
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "inspection line %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int shot(race_game *g, const char *prefix, const char *name)
{
    plat_video_begin_frame();
    if(getenv("PROBE_SOLAR_HOUR"))video_begin_scene();
    if (!(getenv("PROBE_PREGRID")?race_game_draw_pregrid(g):race_game_draw(g)))
        return 0;
    if (getenv("PROBE_PREGRID_OVERLAY"))
        race_game_draw_pregrid_overlay(g);
    if(getenv("PROBE_SOLAR_HOUR"))video_end_scene();
    int w, h;
    size_t len;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    void *png = p ? tdefl_write_image_to_png_file_in_memory(p, w, h, 4, &len) : NULL;
    char path[512];
    snprintf(path, sizeof path, "%s-%s.png", prefix, name);
    FILE *o = png ? fopen(path, "wb") : NULL;
    if (o) {
        fwrite(png, 1, len, o);
        fclose(o);
    }
    free(p);
    mz_free(png);
    plat_video_end_frame();
    return o != NULL;
}
static int merged_ranks(gt4_race_run *r, unsigned frame, int verbose)
{
    uint32_t shown[MOON_MAX_CARS] = {0};
    int official = gt4_race_run_cars(r), phantoms = gt4_race_run_phantom_count(r);
    int n = official + phantoms;
    if (n > MOON_MAX_CARS)
        return 0;
    for (int i = 0; i < official; i++) {
        uint32_t car = gt4_race_run_car(r, i);
        int rank = GT4_U8(car + CAR_FIELD_POSITION);
        int lap = (int16_t)GT4_U16(car + CAR_POSITION + CAR_BLOCK_LAP_COUNT);
        float dist = GT4_F32(car + CAR_POSITION + CAR_BLOCK_COURSE_DISTANCE);
        if (verbose)
            fprintf(stderr,
                    "rank frame=%u official[%d] rank=%d lap=%d dist=%.3f fin=%u t614=%08x t718=%08x\n",
                    frame, i, rank, lap, dist, GT4_U8(car + CAR_5B6),
                    GT4_U32(car + CAR_POSITION + 0x614), GT4_U32(car + CAR_FIELD_UNK_718));
        if (rank < 1 || rank > n || shown[rank - 1]++)
            return 0;
    }
    for (int j = 0; j < phantoms; j++) {
        uint32_t car = gt4_race_run_phantom_car(r, j);
        int rank = GT4_U8(car + CAR_FIELD_POSITION);
        int lap = (int16_t)GT4_U16(car + CAR_POSITION + CAR_BLOCK_LAP_COUNT);
        float dist = GT4_F32(car + CAR_POSITION + CAR_BLOCK_COURSE_DISTANCE);
        if (verbose)
            fprintf(stderr,
                    "rank frame=%u phantom[%d] rank=%d lap=%d dist=%.3f fin=%u t614=%08x t718=%08x\n",
                    frame, j, rank, lap, dist, GT4_U8(car + CAR_5B6),
                    GT4_U32(car + CAR_POSITION + 0x614), GT4_U32(car + CAR_FIELD_UNK_718));
        if (rank < 1 || rank > n || shown[rank - 1]++)
            return 0;
    }
    for (int k = 0; k < n; k++)
        if (shown[k] != 1)
            return 0;
    return 1;
}

static int shot_hud(race_game *g, const char *prefix, const char *name, double dt)
{
    plat_video_begin_frame();
    if (!race_game_draw(g))
        return 0;
    race_game_draw_hud(g, dt);
    int w, h;
    size_t len;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    void *png = p ? tdefl_write_image_to_png_file_in_memory(p, w, h, 4, &len) : NULL;
    char path[512];
    snprintf(path, sizeof path, "%s-%s.png", prefix, name);
    FILE *o = png ? fopen(path, "wb") : NULL;
    if (o) {
        fwrite(png, 1, len, o);
        fclose(o);
    }
    free(p);
    mz_free(png);
    plat_video_end_frame();
    return o != NULL;
}

static uint64_t celestial_frame(race_game *g)
{
    plat_video_begin_frame();
    if (!race_game_draw_scene(g))
        return 0;
    int w, h;
    uint8_t *pixels = plat_sdl_read_pixels(&w, &h);
    uint64_t hash = 1469598103934665603ull;
    if (pixels)
        for (int i = 0; i < w * h * 4; i++)
            hash = (hash ^ pixels[i]) * 1099511628211ull;
    free(pixels);
    plat_video_end_frame();
    return hash;
}
static int benchmark_race(race_game *g, const char *prefix)
{
    video_set_menu_pacing(0);
    video_set_vsync(0);
    fixture_key[SDL_SCANCODE_UP] = 1;
    if (getenv("PROBE_SCENE_MODE")) {
        int mode = atoi(getenv("PROBE_SCENE_MODE"));
        CHECK(mode >= 0 && mode <= 3);
        r3d_scene_options(mode & 1, (mode >> 1) & 1);
    }
    const int frames = getenv("PROBE_FRAMES") ? atoi(getenv("PROBE_FRAMES")) : 120;
    const int warmup = getenv("PROBE_WARMUP") ? atoi(getenv("PROBE_WARMUP")) : 20;
    CHECK(frames >= 60 && frames <= 3600 && warmup >= 0 && warmup <= 120);
    int preparation_ticks = getenv("PROBE_REPLAY") ? frames + warmup + 1200
                            : getenv("PROBE_GRID") ? 0
                                                   : 600;
    for (int i = 0; i < preparation_ticks; i++)
        race_game_update(g, 1. / 60);
    if (getenv("PROBE_REPLAY")) {
        char message[256];
        CHECK(race_game_can_replay(g));
        CHECK(race_game_replay(g, message, sizeof message));
        int ready = 0;
        for (int i = 0; i < 2000 && !ready; i++) {
            SDL_Delay(5);
            ready = race_game_poll(g);
        }
        CHECK(ready == 1 && race_game_replaying(g));
    }
    Uint64 update = 0, render = 0, present = 0;
    moon_vk_render_stats before = {0};
    r3d_totals work_before = {0};
    double *frame_ms = malloc((size_t)frames * sizeof *frame_ms);
    CHECK(frame_ms);
    moon_vk_render_stats loading = moon_vk_render_statistics();
    for (int i = -warmup; i < frames; i++) {
        Uint64 start = SDL_GetPerformanceCounter();
        if (i == 0) {
            moon_vk_render_drain();
            if (!moon_vk_render_active())
                glFinish();
            before = moon_vk_render_statistics();
            work_before = r3d_work_totals();
            start = SDL_GetPerformanceCounter();
        }
        if (getenv("PROBE_SWEEP_CAMERA") && i >= 0 && !(i % 240)) {
            const int cameras[] = {1, 0, 6};
            g->camera = cameras[(i / 240) % 3];
        }
        if (getenv("PROBE_REFLECTION_CYCLE") && i >= 0)
            r3d_reflection_quality(i < frames / 3 ? 0 : i < frames * 2 / 3 ? 1 : 2);
        if (getenv("PROBE_SCENE_SWITCH") && i == frames / 2)
            r3d_scene_options(0, 0);
        race_game_update(g, 1. / 60);
        Uint64 updated = SDL_GetPerformanceCounter();
        plat_video_begin_frame();
        if(getenv("PROBE_SOLAR_HOUR"))video_begin_scene();
        CHECK(race_game_draw(g));
        if(getenv("PROBE_SOLAR_HOUR"))video_end_scene();
    if(getenv("PROBE_SOLAR_HOUR"))video_end_scene();
        Uint64 drawn = SDL_GetPerformanceCounter();
        plat_video_end_frame();
        if (i >= 0) {
            update += updated - start;
            render += drawn - updated;
            Uint64 end = SDL_GetPerformanceCounter();
            present += end - drawn;
            frame_ms[i] = (end - start) * 1000. / SDL_GetPerformanceFrequency();
        }
    }
    Uint64 drain_start = SDL_GetPerformanceCounter();
    moon_vk_render_drain();
    if (!moon_vk_render_active())
        glFinish();
    present += SDL_GetPerformanceCounter() - drain_start;
    for (int i = 0; i < frames; i++)
        for (int j = i + 1; j < frames; j++)
            if (frame_ms[j] < frame_ms[i]) {
                double swap = frame_ms[i];
                frame_ms[i] = frame_ms[j];
                frame_ms[j] = swap;
            }
    fprintf(stderr, "frame pacing: median=%.3f p95=%.3f p99=%.3f worst=%.3f ms\n",
            frame_ms[frames / 2], frame_ms[(frames * 95 + 99) / 100 - 1],
            frame_ms[(frames * 99 + 99) / 100 - 1], frame_ms[frames - 1]);
    int spikes = 0;
    for (int i = 0; i < frames; i++)
        spikes += frame_ms[i] > 1000. / 60;
    free(frame_ms);
    fprintf(stderr, "frame budget: %d/%d over 16.667 ms\n", spikes, frames);
    double scale = 1000. / SDL_GetPerformanceFrequency() / frames;
    fprintf(stderr,
            "race benchmark: %d frames update=%.3f render=%.3f present=%.3f total=%.3f ms/frame\n",
            frames, update * scale, render * scale, present * scale,
            (update + render + present) * scale);
    CHECK(!g->pause && !g->held && g->run);
    moon_vk_render_stats after = moon_vk_render_statistics();
    r3d_totals work_after = r3d_work_totals();
    fprintf(stderr, "shared work: draws=%llu triangles=%llu merged=%llu culled=%llu\n",
            (unsigned long long)(work_after.draws - work_before.draws),
            (unsigned long long)(work_after.triangles - work_before.triangles),
            (unsigned long long)(work_after.merged_groups - work_before.merged_groups),
            (unsigned long long)(work_after.culled_groups - work_before.culled_groups));
    fprintf(stderr,
            "shared materials: textures=%llu texture-hits=%llu states=%llu state-hits=%llu\n",
            (unsigned long long)(work_after.texture_updates - work_before.texture_updates),
            (unsigned long long)(work_after.texture_hits - work_before.texture_hits),
            (unsigned long long)(work_after.state_updates - work_before.state_updates),
            (unsigned long long)(work_after.state_hits - work_before.state_hits));
    fprintf(stderr, "visibility work: tests=%llu spatial-rejected=%llu readbacks=%llu\n",
            (unsigned long long)(work_after.visibility_tests - work_before.visibility_tests),
            (unsigned long long)(work_after.spatial_rejected - work_before.spatial_rejected),
            (unsigned long long)(work_after.readbacks - work_before.readbacks));
    fprintf(stderr, "race benchmark: uploaded=%llu state=%08lx\n",
            (unsigned long long)(after.vertex_upload_bytes - before.vertex_upload_bytes),
            (unsigned long)mz_crc32(0, gt4_phys_mem, 0x2000000));
    fprintf(stderr, "race benchmark: descriptors=%llu draws=%llu submissions=%llu\n",
            (unsigned long long)(after.descriptor_updates - before.descriptor_updates),
            (unsigned long long)(after.draws - before.draws),
            (unsigned long long)(after.submissions - before.submissions));
    fprintf(stderr, "race benchmark: framebuffers=%llu\n",
            (unsigned long long)(after.framebuffer_creations - before.framebuffer_creations));
    if (moon_vk_render_active()) {
        fprintf(stderr, "GPU timing: %.3f ms/frame samples=%llu fence-wait=%.3f ms/frame\n",
                (after.gpu_ns - before.gpu_ns) / 1e6 / frames,
                (unsigned long long)(after.gpu_samples - before.gpu_samples),
                (after.fence_wait_ns - before.fence_wait_ns) / 1e6 / frames);
        fprintf(stderr, "queue rotations: %llu\n",
                (unsigned long long)(after.frame_rotations - before.frame_rotations));
        fprintf(stderr, "draw commands: pipeline=%llu dynamic=%llu vertex=%llu\n",
                (unsigned long long)(after.pipeline_binds - before.pipeline_binds),
                (unsigned long long)(after.dynamic_sets - before.dynamic_sets),
                (unsigned long long)(after.vertex_binds - before.vertex_binds));
        fprintf(stderr,
                "draw CPU: prepare=%.3f descriptors=%.3f pipeline=%.3f commands=%.3f ms/frame\n",
                (after.cpu_prepare_ns - before.cpu_prepare_ns) / 1e6 / frames,
                (after.cpu_descriptors_ns - before.cpu_descriptors_ns) / 1e6 / frames,
                (after.cpu_pipeline_ns - before.cpu_pipeline_ns) / 1e6 / frames,
                (after.cpu_commands_ns - before.cpu_commands_ns) / 1e6 / frames);
        fprintf(stderr, "pipeline preparation: warmup=%llu measured=%llu create=%.3f ms\n",
                (unsigned long long)(before.pipeline_creations - loading.pipeline_creations),
                (unsigned long long)(after.pipeline_creations - before.pipeline_creations),
                (after.pipeline_ns - before.pipeline_ns) / 1e6);
    } else {
        fprintf(stderr, "GPU timing: unavailable for OpenGL in this probe\n");
    }
    char state_path[1024];
    snprintf(state_path, sizeof state_path, "%s.state", prefix);
    FILE *state = fopen(state_path, "wb");
    CHECK(state);
    CHECK(fwrite(gt4_phys_mem, 1, 0x2000000, state) == 0x2000000);
    CHECK(!fclose(state));
    if (getenv("PROBE_REFLECTION_CYCLE"))
        r3d_reflection_quality(0);
    if (getenv("PROBE_SUN_VIEW")) {
        /* Deterministic presentation-only camera facing the low sun. */
        moon_daylight sun=moon_daylight_at(getenv("PROBE_SOLAR_HOUR")?atoi(getenv("PROBE_SOLAR_HOUR")):7);
        float pos[3],rot[9],wheels[4][3];
        CHECK(gt4_race_run_pose(g->run,0,pos,rot,wheels));
        float f[3]={sun.sun[0],.25f,sun.sun[2]};
        float norm=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);for(int k=0;k<3;k++)f[k]/=norm;
        float r[3]={-f[2],0,f[0]};norm=sqrtf(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]);for(int k=0;k<3;k++)r[k]/=norm;
        float u[3]={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
        float rows[9]={r[0],r[1],r[2],u[0],u[1],u[2],-f[0],-f[1],-f[2]};
        pos[1]+=1.2f;race_view_set_camera(g->view,pos,rows,1.3f,1.69f,.1f);
        race_view_show_car(g->view,0);
    }
    CHECK(shot(g, prefix, "benchmark"));
    race_game_close(g);
    plat_video_shutdown();
    return 0;
}
int main(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    plat_video_desc desc = {1280, 896, 0, 0, 0};
    if (getenv("PROBE_WIDTH"))
        desc.width = atoi(getenv("PROBE_WIDTH"));
    if (getenv("PROBE_HEIGHT"))
        desc.height = atoi(getenv("PROBE_HEIGHT"));
    CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GetWindowFromID(1));
    plat_video_set_view(640, 448);
    if(getenv("PROBE_WIDE"))video_set_aspect(1);
    if(getenv("PROBE_SOLAR_HOUR")){r3d_daylight_options(1,atoi(getenv("PROBE_SOLAR_HOUR")),0);video_hdr_options(!getenv("PROBE_NO_HDR"),!getenv("PROBE_NO_RAYS"));}
    if (getenv("PROBE_REALISTIC_SKY"))r3d_night_sky(1);
    if (getenv("PROBE_MODERN_LIGHTING"))
        r3d_modern_lighting(atoi(getenv("PROBE_MODERN_LIGHTING")));
    if (getenv("PROBE_SHADOW_MAPS"))
        r3d_shadow_map_quality(atoi(getenv("PROBE_SHADOW_MAPS")));
    if (getenv("PROBE_REFLECTION_QUALITY"))
        r3d_reflection_quality(atoi(getenv("PROBE_REFLECTION_QUALITY")));
    if (getenv("PROBE_MATERIAL_SHADING"))
        r3d_material_shading(atoi(getenv("PROBE_MATERIAL_SHADING")));
    if(getenv("PROBE_VOLUME"))video_hdr_options(1,2);
    video_ambient_occlusion(getenv("PROBE_AO")!=NULL);
    if(getenv("PROBE_AA"))video_scene_quality(atoi(getenv("PROBE_AA")),2);
    if (getenv("PROBE_CAR_SHADOWS"))
        r3d_car_shadows(atoi(getenv("PROBE_CAR_SHADOWS")));
    if (getenv("PROBE_TEXTURE_FILTER"))
        r3d_texture_filtering(atoi(getenv("PROBE_TEXTURE_FILTER")));
    race_game *g = race_game_open(argv[1]);
    char msg[256];
    CHECK(g);
    if (getenv("PROBE_FIELD_SIZE"))
        race_game_set_field_size(atoi(getenv("PROBE_FIELD_SIZE")));
    const char *others[] = {getenv("PROBE_RIVAL") ? getenv("PROBE_RIVAL") : "ka_01", "z_act_70",
                            "today_g_85", "midget2_d_type_98", "s500_63"};
    if (getenv("PROBE_FULL_GRID"))
        for (int i = 0; i < 5; i++)
            others[i] = getenv("PROBE_RIVAL") ? getenv("PROBE_RIVAL") : "ford_gt_05";
    if (getenv("PROBE_LICENSE")) {
        gt4_career_race_lookup_data lookup;
        gt4_career_license_exam exam;
        CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
        CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,getenv("PROBE_LICENSE"),msg,sizeof msg));
        CHECK(race_game_start_prepared(g,&exam.desc,msg,sizeof msg));
    } else
        CHECK(race_game_start(g,getenv("PROBE_COURSE")?getenv("PROBE_COURSE"):"tsukuba",getenv("PROBE_CAR")?getenv("PROBE_CAR"):"ka_01",0,others,NULL,5,msg,sizeof msg)); /* PROBE_CAR: another car */
    int ready = 0;
    for (int i = 0; i < 2000 && !ready; i++) {
        SDL_PumpEvents();
        SDL_Delay(5);
        ready = race_game_poll(g);
    }
    CHECK(ready==1);
    if (getenv("PROBE_CELESTIAL_CLOCK")) {
        r3d_night_sky(1);
        r3d_daylight_options(1, 23, 2);
        double live_seconds = 0;
        for (int mode = 0; mode < 2; mode++) {
            if (mode) {
                CHECK(race_game_can_replay(g));
                CHECK(race_game_replay(g,msg,sizeof msg));
                int loaded = 0;
                for (int i = 0; i < 4000 && !loaded; i++) {
                    SDL_Delay(5);
                    loaded = race_game_poll(g);
                }
                CHECK(loaded==1 && race_game_replaying(g));
            }
            for (int frame = 0; frame < 240; frame++) {
                memset(fixture_key, 0, sizeof fixture_key);
                fixture_key[SDL_SCANCODE_UP] = !mode;
                race_game_update(g, 1. / 60);
            }
            uint64_t image = celestial_frame(g);
            CHECK(image);
            double seconds = race_view_daylight_seconds(g->view);
            if (getenv("PROBE_CELESTIAL_NEGATIVE"))
                seconds += 1;
            double expected = gt4_race_run_frames(g->run) / 60.;
            printf("celestial %s frame=%u seconds=%.9g hour=%.9g\n", mode ? "replay" : "live",
                   gt4_race_run_frames(g->run), seconds, r3d_solar_hour(seconds, 1));
            CHECK(fabs(seconds-expected)<1e-9);
            if (!mode)
                live_seconds = seconds;
            else
                CHECK(seconds==live_seconds);
            race_game_key(g, mode ? SDLK_p : SDLK_ESCAPE);
            CHECK(mode?g->held:race_game_is_paused(g));
            for (int i = 0; i < 30; i++)
                race_game_update(g, 1. / 60);
            uint64_t paused = celestial_frame(g);
            CHECK(paused==image);
            CHECK(race_view_daylight_seconds(g->view)==seconds);
            race_game_key(g, mode ? SDLK_p : SDLK_ESCAPE);
        }
        CHECK(shot(g,argv[2],"celestial-clock"));
        race_game_close(g);
        plat_video_shutdown();
        puts("PASS live/replay solar frame clock and byte-exact paused scene");
        return 0;
    }
    if (getenv("PROBE_BRAKE_LAMPS")) {
        unsigned changes[6]={0}; int previous[6]={0};
        for(int mode=0;mode<2;mode++) {
            if(mode) {
                CHECK(race_game_can_replay(g));
                CHECK(race_game_replay(g,msg,sizeof msg));
                int loaded=0;
                for(int i=0;i<4000 && !loaded;i++){SDL_Delay(5);loaded=race_game_poll(g);}
                CHECK(loaded==1 && race_game_replaying(g));
            }
            for(int frame=0;frame<2400;frame++) {
                memset(fixture_key,0,sizeof fixture_key);
                fixture_key[SDL_SCANCODE_UP]=frame%240<180;
                fixture_key[SDL_SCANCODE_DOWN]=frame%240>=180;
                race_game_update(g,1./60);
                for(unsigned entry=0;entry<6;entry++) {
                    uint32_t car=gt4_race_run_car(g->run,entry);CHECK(car);
                    int expected=GT4_F32(car+CAR_BRAKE_PEDAL)>0;
                    int actual=race_view_brake_lamp(g->view,entry);
                    if(getenv("PROBE_BRAKE_NEGATIVE")) actual=!actual;
                    CHECK(actual==expected);
                    changes[entry]+=actual!=previous[entry];previous[entry]=actual;
                }
            }
            CHECK(changes[0]>3);
        }
        for(int entry=0;entry<6;entry++) {printf("brake transitions entry%d=%u\n",entry,changes[entry]);CHECK(changes[entry]>0);}
        CHECK(shot(g,argv[2],"brake-lamps"));
        race_game_close(g);plat_video_shutdown();
        puts("PASS live and replay brake state follows every car pedal on each tick");return 0;
    }
    if (getenv("PROBE_BENCHMARK"))
        return benchmark_race(g, argv[2]);
    if (getenv("PROBE_FIELD100")) {
        int frames = getenv("PROBE_FIELD100") ? atoi(getenv("PROBE_FIELD100")) : 1200;
        for (int f = 0; f < frames; f++)
            race_game_update(g, 1. / 60);
        gt4_race_entrant ents[MOON_MAX_CARS];
        int n = gt4_race_run_classification(g->run, ents, MOON_MAX_CARS);
        int poses = 0;
        for (int j = 0; j < gt4_race_run_phantom_count(g->run); j++) {
            float pos[3], rot[9], wheels[4][3];
            if (gt4_race_run_phantom_pose(g->run, j, pos, rot, wheels))
                poses++;
        }
        fprintf(stderr, "field100: frames=%d official=%d phantoms=%d class=%d poses=%d\n", frames,
                gt4_race_run_cars(g->run), gt4_race_run_phantom_count(g->run), n, poses);
        for (int i = 0; i < n; i++)
            fprintf(stderr, "field100: place=%d id=%d official=%d name=%s lap=%d dist=%.1f fin=%d\n",
                    ents[i].place, ents[i].id, ents[i].official, ents[i].name, ents[i].laps,
                    ents[i].distance, ents[i].finished);
        CHECK(shot(g, argv[2], "field100"));
        fprintf(stderr, "field100: drawn_phantoms=%d\n", race_view_phantom_draws());
        {
            float p0[3], r0[9], w0[4][3];
            if (gt4_race_run_pose(g->run, 0, p0, r0, w0))
                fprintf(stderr, "field100: player pos=%.1f,%.1f,%.1f\n", p0[0], p0[1], p0[2]);
        }
        for (int j = 0; j < 10 && j < gt4_race_run_phantom_count(g->run); j++) {
            float p0[3], r0[9], w0[4][3];
            if (gt4_race_run_phantom_pose(g->run, j, p0, r0, w0))
                fprintf(stderr, "field100: phantom[%d] pos=%.1f,%.1f,%.1f rot0=%.2f\n", j, p0[0],
                        p0[1], p0[2], r0[0]);
            else
                fprintf(stderr, "field100: phantom[%d] pose=0\n", j);
        }
        race_game_close(g);
        plat_video_shutdown();
        puts("field100: captured");
        return 0;
    }
    if (getenv("PROBE_RESULT")) {
        int frames = getenv("PROBE_RESULT_FRAMES") ? atoi(getenv("PROBE_RESULT_FRAMES")) : 18000;
        int finished = 0;
        for (int f = 0; f < frames; f++) {
            memset(fixture_key, 0, sizeof fixture_key);
            race_game_update(g, 1. / 60);
            if (race_game_finished(g) >= 0) {
                finished = 1;
                fprintf(stderr, "result run: player finished frame=%d\n", f + 1);
                break;
            }
        }
        if (!finished) {
            fprintf(stderr, "result run: player did not finish\n");
            race_game_close(g);
            plat_video_shutdown();
            return 1;
        }
        if (gt4_race_run_phantom_count(g->run)) {
            int n = 0, all = 0, f = 0;
            for (; f < 20000; f++) {
                race_game_update(g, 1. / 60);
                gt4_race_entrant ents[MOON_MAX_CARS];
                n = gt4_race_run_classification(g->run, ents, MOON_MAX_CARS);
                all = n > 0;
                for (int i = 0; i < n; i++)
                    if (!ents[i].finished)
                        all = 0;
                if (all && n > 6)
                    break;
            }
            fprintf(stderr, "result run: classify frames=%d n=%d all=%d\n", f, n, all);
            {
                gt4_race_entrant ents[MOON_MAX_CARS];
                n = gt4_race_run_classification(g->run, ents, MOON_MAX_CARS);
                uint32_t leader = n ? ents[0].finish_time : 0, prev = leader;
                for (int i = 0; i < n; i++) {
                    uint32_t gap = ents[i].finish_time - leader;
                    fprintf(stderr,
                            "result run: place=%d id=%d official=%d fin=%d time=%u gap=%u name=%s\n",
                            ents[i].place, ents[i].id, ents[i].official, ents[i].finished,
                            ents[i].finish_time, gap, ents[i].name);
                    if (all && i) {
                        CHECK(ents[i].finish_time >= prev);
                        if (ents[i].finish_time == prev)
                            fprintf(stderr,
                                    "result run: same finish time at places %d and %d: %u\n",
                                    ents[i - 1].place, ents[i].place, ents[i].finish_time);
                    }
                    prev = ents[i].finish_time;
                }
            }
        }
        int draw_frames = getenv("PROBE_RESULT_DRAW") ? atoi(getenv("PROBE_RESULT_DRAW")) : 900;
        for (int f = 0; f < draw_frames; f++) {
            plat_video_begin_frame();
            if (!race_game_draw(g))
                break;
            race_game_draw_hud(g, 1. / 60);
            plat_video_end_frame();
        }
        CHECK(shot_hud(g, argv[2], "result-top", 0));
        if (getenv("PROBE_RESULT_SCROLL")) {
            int steps = atoi(getenv("PROBE_RESULT_SCROLL"));
            for (int k = 0; k < steps; k++) {
                fixture_key[SDL_SCANCODE_DOWN] = 1;
                plat_video_begin_frame();
                race_game_draw(g);
                race_game_draw_hud(g, 1. / 60);
                plat_video_end_frame();
                fixture_key[SDL_SCANCODE_DOWN] = 0;
                plat_video_begin_frame();
                race_game_draw(g);
                race_game_draw_hud(g, 1. / 60);
                plat_video_end_frame();
            }
            CHECK(shot_hud(g, argv[2], "result-bottom", 0));
        }
        race_game_close(g);
        plat_video_shutdown();
        puts("result run: board drawn");
        return 0;
    }
    if (getenv("PROBE_POOL")) {
        gt4_vol *v = gt4_vol_open(argv[1], NULL);
        size_t n = 0;
        uint8_t *d = v ? gt4_vol_read(v, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n) : NULL;
        printf("pool: generic_size=%zu\n", n);
        for (unsigned id = 0; id < 512 && d; id++) {
            const char *l = gt4_specdb_label_for_id(d, n, id);
            if (l && *l)
                printf("pool: %u %s\n", id, l);
        }
        free(d);
        if (v) gt4_vol_close(v);
        race_game_close(g);
        plat_video_shutdown();
        return 0;
    }
    if (getenv("PROBE_TAIL_SCAN")) {
        uint8_t *m = gt4_phys_mem;
        size_t begin = 0x1d10000, end = 0x2000000, run = 0;
        size_t best = 0, best_at = 0, first = 0;
        for (size_t a = begin; a < end; a++) {
            if (!m[a]) {
                if (!run)
                    first = a;
                run++;
                if (run > best) {
                    best = run;
                    best_at = first;
                }
            } else
                run = 0;
        }
        printf("tail scan: begin=%zx end=%zx best_zero=%zu at=%zx first_nonzero_after_best=%zx\n",
               begin, end, best, best_at, best_at + best < end ? best_at + best : end);
        run = 0;
        first = 0;
        for (size_t a = begin; a < end; a++) {
            if (!m[a]) {
                if (!run)
                    first = a;
                run++;
            } else if (run) {
                if (run >= 0x8000)
                    printf("tail run: %zx..%zx len=%zu\n", first, a, run);
                run = 0;
            }
        }
        if (run >= 0x8000)
            printf("tail run: %zx..%zx len=%zu\n", first, end, run);
        race_game_close(g);
        plat_video_shutdown();
        return 0;
    }
    if (getenv("PROBE_HUD_RANK")) {
        int rank = atoi(getenv("PROBE_HUD_RANK"));
        race_hud *h = race_hud_open(g->vol);
        CHECK(h);
        race_hud_prepare_fonts(h);
        race_hud_state st = {0};
        st.rank = rank;
        st.cars = 12;
        st.lap = 1;
        st.laps = 3;
        st.speed = 80;
        st.rpm = 4000;
        st.gear = 3;
        st.start_frames = 0;
        plat_video_begin_frame();
        race_hud_draw(h, &st, 0);
        int w, hh;
        size_t len;
        uint8_t *p = plat_sdl_read_pixels(&w, &hh);
        void *png = p ? tdefl_write_image_to_png_file_in_memory(p, w, hh, 4, &len) : NULL;
        char path[512];
        snprintf(path, sizeof path, "%s-hud-rank-%d.png", argv[2], rank);
        FILE *o = png ? fopen(path, "wb") : NULL;
        if (o) {
            fwrite(png, 1, len, o);
            fclose(o);
        }
        free(p);
        mz_free(png);
        plat_video_end_frame();
        race_hud_close(h);
        race_game_close(g);
        plat_video_shutdown();
        printf("hud rank: rank=%d two_digit=%d path=%s\n", rank, rank > 9, path);
        return o ? 0 : 1;
    }
    if (getenv("PROBE_RANK_HUD")) {
        unsigned seen = 0;
        int last = 0;
        int field = gt4_race_run_cars(g->run) + gt4_race_run_phantom_count(g->run);
        for (int f = 0; f < 3600; f++) {
            memset(fixture_key, 0, sizeof fixture_key);
            race_game_update(g, 1. / 60);
            int rank = GT4_U8(gt4_race_run_car(g->run, 0) + CAR_FIELD_POSITION);
            if (rank != last) {
                fprintf(stderr, "rank hud: frame=%d player_rank=%d field=%d\n", f + 1, rank, field);
                last = rank;
            }
            if (rank >= 7 && rank <= 12 && !(seen & (1u << rank))) {
                char name[32];
                snprintf(name, sizeof name, "rank-%d", rank);
                CHECK(shot_hud(g, argv[2], name, 1. / 60));
                seen |= 1u << rank;
                fprintf(stderr, "rank hud: shot=%s-%s.png\n", argv[2], name);
            }
            if ((field == 8 && rank >= 8) || (field >= 12 && rank >= 12))
                break;
        }
        race_game_close(g);
        plat_video_shutdown();
        puts("rank hud: live rank screenshots saved");
        return 0;
    }
    if (getenv("PROBE_PHANTOM_RUN")) {
        uint32_t cond = gt4_race_run_conductor(g->run);
        size_t nb = (size_t)MOON_CONDUCTOR_CARS * 0x1100u,
               ns = (size_t)MOON_CONDUCTOR_CARS * CAR_SIZE,
               nt = (size_t)MOON_CONDUCTOR_CARS * 16u;
        uint8_t *before = malloc(nb + ns + nt);
        CHECK(before);
        memcpy(before, gt4_ptr(cond + 0x98), nb);
        memcpy(before + nb, gt4_ptr(cond + 0x6698), ns);
        memcpy(before + nb + ns, gt4_ptr(cond + 0xCA58), nt);
        uint32_t cb = mz_crc32(0, before, nb), cs = mz_crc32(0, before + nb, ns),
                 ct = mz_crc32(0, before + nb + ns, nt);
        fprintf(stderr, "phantom run: start cars=%u count=%u phantoms=%d crc=%08x/%08x/%08x\n",
                gt4_race_run_cars(g->run), GT4_U32(cond + CONDUCTOR_FIELD_CAR_COUNT),
                gt4_race_run_phantom_count(g->run), cb, cs, ct);
        unsigned first_over = 0;
        if (getenv("PROBE_GRID_TRACE")) {
            int no = gt4_race_run_cars(g->run), np = gt4_race_run_phantom_count(g->run);
            for (int k = 0; k < no + np; k++) {
                uint32_t car = k < no ? gt4_race_run_car(g->run, k)
                                      : gt4_race_run_phantom_car(g->run, k - no);
                fprintf(stderr,
                        "grid trace id=%d off=%d x=%.3f y=%.3f z=%.3f dist=%.3f heading=%.3f "
                        "speed=%.3f\n",
                        k, k < no, GT4_F32(car + CAR_POSITION),
                        GT4_F32(car + CAR_POSITION + 4), GT4_F32(car + CAR_POSITION + 8),
                        GT4_F32(car + CAR_FIELD_COURSE_DISTANCE),
                        GT4_F32(car + CAR_BLOCK_HEADING), GT4_F32(car + CAR_ABS_SPEED));
            }
            fprintf(stderr, "contact restitution=%.6f\n", GT4_F32(0x00846630u));
            {
                float pos[MOON_MAX_CARS][3];
                for (int k = 0; k < no + np; k++) {
                    uint32_t car = k < no ? gt4_race_run_car(g->run, k)
                                          : gt4_race_run_phantom_car(g->run, k - no);
                    for (int a = 0; a < 3; a++)
                        pos[k][a] = GT4_F32(car + CAR_POSITION + (uint32_t)a * 4);
                }
                float minimum = 1e30f;
                for (int a = 0; a < no + np; a++)
                    for (int b = a + 1; b < no + np; b++) {
                        float dx = pos[a][0] - pos[b][0], dy = pos[a][1] - pos[b][1],
                              dz = pos[a][2] - pos[b][2];
                        float d = sqrtf(dx * dx + dy * dy + dz * dz);
                        if (d < minimum)
                            minimum = d;
                    }
                fprintf(stderr, "grid distance: min_pairwise=%.3f\n", minimum);
                for (int j = 0; j < np; j++) {
                    uint32_t car = gt4_race_run_phantom_car(g->run, j);
                    float behind = GT4_F32(gt4_race_run_car(g->run, 0) +
                                           CAR_FIELD_COURSE_DISTANCE) -
                                   GT4_F32(car + CAR_FIELD_COURSE_DISTANCE);
                    float dx = GT4_F32(car + CAR_POSITION) -
                               GT4_F32(gt4_race_run_car(g->run, 0) + CAR_POSITION);
                    float dy = GT4_F32(car + CAR_POSITION + 4) -
                               GT4_F32(gt4_race_run_car(g->run, 0) + CAR_POSITION + 4);
                    float dz = GT4_F32(car + CAR_POSITION + 8) -
                               GT4_F32(gt4_race_run_car(g->run, 0) + CAR_POSITION + 8);
                    fprintf(stderr, "grid distance: phantom=%d behind_player=%.3f "
                                    "from_player=%.3f\n",
                            j, behind, sqrtf(dx * dx + dy * dy + dz * dz));
                }
            }
        }
        for (int f = 0; f < 18000; f++) {
            if (getenv("PROBE_START_TRACE") && f < 701) {
                uint32_t c0 = gt4_race_run_car(g->run, 0);
                uint32_t c1 = gt4_race_run_car(g->run, 1);
                uint32_t p0 = gt4_race_run_phantom_car(g->run, 0);
                uint32_t pc = p0 ? GT4_U32(p0 + CAR_SESSION) : 0;
                fprintf(stderr,
                        "start trace f=%d official_countdown=%d official_ai_speed=%.4f "
                        "phantom_speed=%.4f official_dist=%.3f phantom_dist=%.3f "
                        "phantom_own_countdown=%d\n",
                        f, gt4_race_run_countdown(g->run, 0), GT4_F32(c1 + CAR_ABS_SPEED),
                        p0 ? GT4_F32(p0 + CAR_ABS_SPEED) : 0,
                        GT4_F32(c0 + CAR_FIELD_COURSE_DISTANCE),
                        p0 ? GT4_F32(p0 + CAR_FIELD_COURSE_DISTANCE) : 0,
                        pc ? (int32_t)GT4_U32(GT4_U32(pc) + 0xbc) +
                                 (int32_t)GT4_U32(pc + CONDUCTOR_FIELD_UNK_CBFC_U)
                           : 0);
            }
            CHECK(gt4_race_run_frame(g->run));
            if (getenv("PROBE_RANK_TRACE") && f % 60 == 0) {
                int no = gt4_race_run_cars(g->run), np = gt4_race_run_phantom_count(g->run);
                gt4_race_entrant ce[MOON_MAX_CARS];
                int cn = gt4_race_run_classification(g->run, ce, MOON_MAX_CARS);
                fprintf(stderr, "rank trace f=%d\n", f);
                for (int k = 0; k < no + np; k++) {
                    uint32_t car = k < no ? gt4_race_run_car(g->run, k)
                                          : gt4_race_run_phantom_car(g->run, k - no);
                    int place = -1;
                    for (int q = 0; q < cn; q++)
                        if (ce[q].id == k)
                            place = ce[q].place;
                    int lap = (int16_t)GT4_U16(car + CAR_POSITION + CAR_BLOCK_LAP_COUNT);
                    int done = lap;
                    if (!GT4_U8(car + CAR_POSITION + CAR_BLOCK_FINISH_DRIVE_STARTED))
                        done -= GT4_U8(car + CAR_POSITION + CAR_BLOCK_REVERSE_LAP_DEBT);
                    fprintf(stderr,
                            "rank trace f=%d id=%d off=%d lap=%d done=%d fin=%u dist=%.3f raw=%d place=%d\n",
                            f, k, k < no, lap, done, GT4_U8(car + CAR_5B6),
                            GT4_F32(car + CAR_FIELD_COURSE_DISTANCE),
                            GT4_U8(car + CAR_FIELD_POSITION), place);
                }
            }
            if (getenv("PROBE_RANK_TRACE") && f >= 4000)
                break;
            if (getenv("PROBE_START_TRACE") && f >= 700)
                break;
            if (!first_over && gt4_race_run_phantom_count(g->run)) {
                int rank = GT4_U8(gt4_race_run_car(g->run, 0) + CAR_FIELD_POSITION);
                if (rank > MOON_CONDUCTOR_CARS) {
                    first_over = (unsigned)f + 1;
                    fprintf(stderr,
                            "phantom run: frame=%u player_rank=%d field=%d phantoms=%d\n",
                            first_over, rank,
                            gt4_race_run_cars(g->run) + gt4_race_run_phantom_count(g->run),
                            gt4_race_run_phantom_count(g->run));
                }
            }
        }
        uint32_t ab = mz_crc32(0, gt4_ptr(cond + 0x98), nb),
                 as = mz_crc32(0, gt4_ptr(cond + 0x6698), ns),
                 at = mz_crc32(0, gt4_ptr(cond + 0xCA58), nt);
        int good = GT4_U32(cond + CONDUCTOR_FIELD_CAR_COUNT) == MOON_CONDUCTOR_CARS;
        for (int i = 0; i < MOON_CONDUCTOR_CARS; i++)
            good &= GT4_U32(cond + 0x6698 + (uint32_t)i * CAR_SIZE + 4) == cond;
        for (int j = 0; j < gt4_race_run_phantom_count(g->run); j++) {
            uint32_t pc = gt4_race_run_phantom_car(g->run, j);
            good &= pc && GT4_U32(pc + 4) != cond;
        }
        int ranks = merged_ranks(g->run, 18000, 1);
        if (gt4_race_run_phantom_count(g->run) && !first_over)
            good = 0;
        good &= ranks;
        for (int j = 0; j < gt4_race_run_phantom_count(g->run); j++) {
            uint32_t pc = gt4_race_run_phantom_car(g->run, j);
            uint32_t pcond = GT4_U32(pc + CAR_SESSION);
            fprintf(stderr, "phantom clock[%d]: cond=%08x clock=%u ms=%u\n", j, pcond,
                    GT4_U32(pcond + CONDUCTOR_FIELD_RACE_CLOCK_U),
                    GT4_U32(pcond + CONDUCTOR_FIELD_RACE_CLOCK_U) / 3u);
        }
        fprintf(stderr,
                "phantom run: end cars=%u count=%u phantoms=%d crc=%08x/%08x/%08x ranks=%d good=%d\n",
                gt4_race_run_cars(g->run), GT4_U32(cond + CONDUCTOR_FIELD_CAR_COUNT),
                gt4_race_run_phantom_count(g->run), ab, as, at, ranks, good);
        for (int i = 0; i < MOON_CONDUCTOR_CARS; i++)
            fprintf(stderr, "phantom run: ten[%d]=%08x\n", i,
                    GT4_U32(cond + CONDUCTOR_FIELD_UNK_CA58_U + (uint32_t)i * 16u));
        char path[600];
        snprintf(path, sizeof path, "%s-phantom-before.bin", argv[2]);
        FILE *o = fopen(path, "wb");
        if (o) { fwrite(before, 1, nb + ns + nt, o); fclose(o); }
        uint8_t *after = malloc(nb + ns + nt);
        CHECK(after);
        memcpy(after, gt4_ptr(cond + 0x98), nb);
        memcpy(after + nb, gt4_ptr(cond + 0x6698), ns);
        memcpy(after + nb + ns, gt4_ptr(cond + 0xCA58), nt);
        snprintf(path, sizeof path, "%s-phantom-after.bin", argv[2]);
        o = fopen(path, "wb");
        if (o) { fwrite(after, 1, nb + ns + nt, o); fclose(o); }
        free(before);
        free(after);
        race_game_close(g);
        plat_video_shutdown();
        return good ? 0 : 1;
    }
    if (getenv("PROBE_CINEMA_TEST")) {
        race_game_update(g, 0);
        uint8_t *before = malloc(0x2000000);
        CHECK(before);
        memcpy(before, gt4_phys_mem, 0x2000000);
        float eye[3], rows[9], pr[3], seed[3];
        CHECK(race_view_get_camera(g->view,seed,rows,pr));
        race_game_cinematic_toggle(g);
        CHECK(race_game_cinematic(g));
        cinematic_camera(g, 0);
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        for (int k = 0; k < 3; k++)
            CHECK(fabsf(eye[k]-seed[k])<1e-5f);
        fixture_key[SDL_SCANCODE_UP] = fixture_key[SDL_SCANCODE_A] = 1;
        read_controls(g);
        CHECK(!command.throttle && !command.brake && !command.steer && !command.shift);
        memset(fixture_key, 0, sizeof fixture_key);
        fixture_key[SDL_SCANCODE_W] = 1;
        for (int i = 0; i < 60; i++)
            cinematic_camera(g, 1. / 60);
        memset(fixture_key, 0, sizeof fixture_key);
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        float distance = 0;
        for (int k = 0; k < 3; k++)
            distance += (eye[k] - seed[k]) * (eye[k] - seed[k]);
        CHECK(distance>16 && distance<25);
        float old = pr[0];
        race_game_cinematic_mouse(g, 80, -30, 3);
        cinematic_camera(g, 1. / 60);
        CHECK(g->cinema.projection[0]>old);
        CHECK(!g->cinema.aim);
        race_game_cinematic_key(g, SDLK_f);
        cinematic_camera(g, 0);
        CHECK(g->cinema.mode==1 && g->cinema.aim);
        float pos[3], rot[9], wheels[4][3];
        CHECK(gt4_race_run_pose(g->run,0,pos,rot,wheels));
        float len = 0, f[3];
        for (int k = 0; k < 3; k++) {
            f[k] = pos[k] + rot[k * 3 + 1] * .65f - g->cinema.eye[k];
            len += f[k] * f[k];
        }
        len = sqrtf(len);
        for (int k = 0; k < 3; k++)
            CHECK(fabsf(-g->cinema.rows[6+k]-f[k]/len)<1e-5f);
        memcpy(seed, g->cinema.eye, sizeof seed);
        g->cinema.anchor[0] -= 3;
        cinematic_camera(g, 0);
        CHECK(fabsf(g->cinema.eye[0]-seed[0]-3)<1e-5f);
        race_game_cinematic_mouse(g, 150, 40, 0);
        cinematic_camera(g, 1. / 60);
        CHECK(g->cinema.aim);
        CHECK(fabsf(g->cinema.eye[2]-seed[2])>.001f);
        race_game_cinematic_key(g, SDLK_TAB);
        cinematic_camera(g, 0);
        CHECK(g->cinema.target==1);
        memcpy(seed, g->cinema.eye, sizeof seed);
        race_game_cinematic_key(g, SDLK_g);
        cinematic_camera(g, 0);
        CHECK(g->cinema.mode==2 && !g->cinema.aim);
        for (int k = 0; k < 3; k++)
            CHECK(fabsf(g->cinema.eye[k]-seed[k])<1e-4f);
        CHECK(gt4_race_run_pose(g->run,1,pos,rot,wheels));
        for (int k = 0; k < 3; k++) {
            float expected = pos[k];
            for (int j = 0; j < 3; j++)
                expected += rot[k * 3 + j] * g->cinema.mount[j];
            CHECK(fabsf(expected-g->cinema.eye[k])<1e-5f);
        }
        for (int a = 0; a < 3; a++) {
            float norm = 0;
            for (int k = 0; k < 3; k++)
                norm += g->cinema.rows[a * 3 + k] * g->cinema.rows[a * 3 + k];
            CHECK(fabsf(norm-1)<1e-5f);
        }
        int original_hold = g->held;
        race_game_cinematic_key(g, SDLK_p);
        CHECK(g->held);
        fixture_key[SDL_SCANCODE_E] = 1;
        cinematic_camera(g, 1. / 60);
        memset(fixture_key, 0, sizeof fixture_key);
        float focus = g->cinema.focus;
        race_game_cinematic_key(g, SDLK_LEFTBRACKET);
        cinematic_camera(g, 1. / 60);
        CHECK(!g->cinema.focus_auto && g->cinema.focus<=focus);
        race_game_cinematic_key(g, SDLK_r);
        CHECK(g->cinema.focus_auto);
        /* Visible focus fixture, framing the selected car from outside. */
        float target[3], local[3] = {3, 1.5f, -5};
        CHECK(cinematic_pose(g,pos,rot));
        for (int k = 0; k < 3; k++) {
            eye[k] = pos[k];
            for (int j = 0; j < 3; j++)
                eye[k] += rot[k * 3 + j] * local[j];
            target[k] = pos[k] + rot[k * 3 + 1] * .65f;
        }
        look(g, eye, target, 0, 3, 3 * 1.30122f);
        CHECK(race_view_get_camera(g->view,g->cinema.eye,g->cinema.rows,g->cinema.projection));
        cinematic_mode(g, 1);
        cinematic_camera(g, 0);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        if (getenv("PROBE_CINEMA_NEGATIVE"))
            GT4_U8(gt4_race_run_car(g->run, 0) + 0x554) ^= 1;
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        CHECK(shot(g,argv[2],"cinema-sharp"));
        int w, h, w2, h2;
        uint8_t *sharp = plat_sdl_read_pixels(&w, &h);
        CHECK(sharp);
        race_game_cinematic_key(g, SDLK_b);
        CHECK(g->cinema.aperture==1);
        CHECK(shot(g,argv[2],"cinema-focus"));
        uint8_t *blur = plat_sdl_read_pixels(&w2, &h2);
        CHECK(blur && w==w2 && h==h2);
        unsigned changed = 0;
        for (int i = 0; i < w * h; i++)
            if (memcmp(sharp + i * 4, blur + i * 4, 3))
                changed++;
        free(sharp);
        free(blur);
        CHECK(changed>100);
        float samples[8][4];
        gt4_photo_samples(samples, 8);
        for (int i = 0; i < 8; i++) {
            float shift[2], jitter[2];
            cinematic_lens(g->cinema.projection, 2.8f, g->cinema.focus, samples[i], shift, jitter);
            float sx = g->cinema.projection[0] * (448.f * 4.f / 3.f) / 640 * video_projection_x();
            CHECK(fabsf(-shift[0]/g->cinema.focus*sx+jitter[0])<1e-6f);
            CHECK(fabsf(-shift[1]/g->cinema.focus*g->cinema.projection[1]+jitter[1])<1e-6f);
        }
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        for (int k = 0; k < 3; k++)
            CHECK(fabsf(eye[k]-g->cinema.eye[k])<1e-5f);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        race_game_cinematic_key(g, SDLK_HOME);
        cinematic_camera(g, 0);
        CHECK(g->cinema.seeded && !g->cinema.mode);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        race_game_cinematic_key(g, SDLK_ESCAPE);
        CHECK(!race_game_cinematic(g) && g->held==original_hold && !SDL_GetRelativeMouseMode());
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        CHECK(!memcmp(eye,g->cinema.original_eye,sizeof eye) && !memcmp(rows,g->cinema.original_rows,sizeof rows));
        race_game_cinematic_toggle(g);
        race_game_stop(g);
        CHECK(!race_game_cinematic(g) && !SDL_GetRelativeMouseMode());
        free(before);
        race_game_close(g);
        plat_video_shutdown();
        puts(
            "cinema: seed, controls, lens/focus, follow/aim, target, mount, hold, cleanup and unchanged 32 MiB physics passed");
        return 0;
    }
    if (getenv("PROBE_FILM_TEST")) {
        /* Filming must remain a read-only consumer of the complete physics
           address space, including all AI cars. This is a PC rig, not an
           original-camera fidelity comparison. */
        uint8_t *before = malloc(0x2000000);
        CHECK(before);
        memcpy(before, gt4_phys_mem, 0x2000000);
        const char *modes[] = {"low", "orbit", "crane"};
        for (int mode = 0; mode < 3; mode++)
            for (int step = 0; step < 3; step++) {
#ifdef _WIN32
                _putenv_s("MOON_RACE_FILM", modes[mode]);
#else
                setenv("MOON_RACE_FILM", modes[mode], 1);
#endif
                g->film_clock = 10 + step * 12;
                film_camera(g, 1. / 60);
                float eye[3], rows[9], projection[3];
                CHECK(race_view_get_camera(g->view,eye,rows,projection));
                for (int a = 0; a < 3; a++) {
                    float length = 0;
                    for (int k = 0; k < 3; k++) {
                        CHECK(isfinite(eye[k]));
                        length += rows[a * 3 + k] * rows[a * 3 + k];
                    }
                    CHECK(fabsf(length-1)<1e-5f);
                }
                CHECK(projection[0]>0 && projection[1]>0 && projection[2]>0);
                if (getenv("PROBE_FILM_NEGATIVE"))
                    GT4_U8(gt4_race_run_car(g->run, 0) + 0x554) ^= 1;
                CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
            }
        free(before);
        race_game_close(g);
        plat_video_shutdown();
        puts("film camera: orthonormal, finite, physics unchanged (32 MiB)");
        return 0;
    }
    if (getenv("PROBE_ALL_ENTRIES"))
        for (int entry = 0; entry < 6; entry++) {
            const car_model *car = race_view_car_model(g->view, entry);
            CHECK(car);
            for (int wheel = 0; wheel < 4; wheel++) {
                const gt4_model *model = car->wheel[wheel];
                CHECK(model && model->count);
                for (int triangle = 0; triangle < model->count; triangle++)
                    CHECK(model->triangles[triangle].source_model==0);
                r3d_mesh *mesh = r3d_mesh_create(model);
                CHECK(mesh);
                r3d_mesh_destroy(mesh);
            }
        }
    race_game_update(g, 0);
    float eye[3], rows[9], projection[3], start[3];
    CHECK(race_view_get_camera(g->view,eye,rows,projection));
    memcpy(start, eye, sizeof eye);
    race_game_inspect_toggle(g);
    inspect_camera(g, 0);
    CHECK(g->inspect_seeded);
    fixture_key[SDL_SCANCODE_UP] = fixture_key[SDL_SCANCODE_A] = 1;
    read_controls(g);
    CHECK(!command.throttle && !command.brake && !command.steer && !command.shift);
    memset(fixture_key, 0, sizeof fixture_key);
    CHECK(race_view_get_camera(g->view,eye,rows,projection));
    for (int k = 0; k < 3; k++)
        CHECK(fabsf(eye[k]-start[k])<1e-5f);
    fixture_key[SDL_SCANCODE_W] = 1;
    for (int i = 0; i < 60; i++)
        inspect_camera(g, 1. / 60);
    fixture_key[SDL_SCANCODE_W] = 0;
    CHECK(race_view_get_camera(g->view,eye,rows,projection));
    for (int k = 0; k < 3; k++)
        CHECK(fabsf(eye[k]-start[k]+rows[6+k]*3)<1e-3f);
    float expected = 1;
    if (argc > 3 && !strcmp(argv[3], "--negative"))
        expected = 2;
    for (int a = 0; a < 3; a++) {
        float length = 0;
        for (int k = 0; k < 3; k++)
            length += rows[a * 3 + k] * rows[a * 3 + k];
        CHECK(fabsf(length-expected)<1e-5f);
    }
    fixture_key[SDL_SCANCODE_Q] = fixture_key[SDL_SCANCODE_LSHIFT] = 1;
    for (int i = 0; i < 60; i++)
        inspect_camera(g, 1. / 60);
    memset(fixture_key, 0, sizeof fixture_key);
    CHECK(race_view_get_camera(g->view,eye,rows,projection));
    CHECK(fabsf(eye[1]-(start[1]-rows[7]*3-15))<1e-3f);
    race_game_inspect_toggle(g);
    race_game_update(g, 0);
    CHECK(!g->inspect_on);
    /* Diagnostic only: inspect live wheel transforms after native ticks. */
    if (getenv("PROBE_STEPS"))
        for (int i = 0; i < atoi(getenv("PROBE_STEPS")); i++) {
            race_game_update(g, 1. / 60);
            if (getenv("PROBE_DRAW_STEPS")) {
                plat_video_begin_frame();
                CHECK(race_game_draw(g));
                plat_video_end_frame();
            }
        }
    if (getenv("PROBE_STEPS"))
        CHECK(shot(g,argv[2],"live"));
    /* Diagnostic views use native choices to inspect every visible wheel and
       the bonnet/lamps. The car pose itself is the real simulation's pose. */
    float pos[3], rot[9], wheels[4][3];
    CHECK(gt4_race_run_pose(g->run,getenv("PROBE_ENTRY")?atoi(getenv("PROBE_ENTRY")):0,pos,rot,wheels));
    if (getenv("PROBE_ISOLATE")) {
        int entry = getenv("PROBE_ENTRY") ? atoi(getenv("PROBE_ENTRY")) : 0;
        for (int i = 0; i < 5; i++)
            if (i != entry - 1)
                race_view_set_opponent(g->view, i, NULL, NULL, NULL);
    }
    static const float camera[4][3] = {{3, 2, -5}, {-3, 2, 5}, {4, 1.2f, 0}, {-4, 1.2f, 0}};
    static const char *names[] = {"front", "rear", "side", "side-left"};
    float camera_scale =
        getenv("PROBE_CAMERA_SCALE") ? (float)atof(getenv("PROBE_CAMERA_SCALE")) : 1;
    int all_entries = getenv("PROBE_ALL_ENTRIES") != NULL;
    for (int entry = 0; entry < (all_entries ? 6 : 1); entry++)
        for (int a = 0; a < 4; a++) {
            if (all_entries)
                CHECK(gt4_race_run_pose(g->run,entry,pos,rot,wheels));
            float target[3];
            for (int k = 0; k < 3; k++) {
                eye[k] = target[k] = pos[k];
                for (int j = 0; j < 3; j++) {
                    float component=camera[a][j];
                    if(getenv("PROBE_PHOTO") && j==1)component=.95f;
                    if(getenv("PROBE_PHOTO_FLIP") && j==0)component=-component;
                    eye[k] += rot[k * 3 + j] * component * camera_scale;
                }
                target[k] += rot[k * 3 + 1] * .6f;
            }
            float f[3], r[3], u[3], length = 0;
            for (int k = 0; k < 3; k++) {
                f[k] = target[k] - eye[k];
                length += f[k] * f[k];
            }
            length = sqrtf(length);
            for (int k = 0; k < 3; k++)
                f[k] /= length;
            length = sqrtf(f[0] * f[0] + f[2] * f[2]);
            r[0] = -f[2] / length;
            r[1] = 0;
            r[2] = f[0] / length;
            u[0] = -f[1] * r[2];
            u[1] = f[0] * r[2] - f[2] * r[0];
            u[2] = f[1] * r[0];
            for (int k = 0; k < 3; k++) {
                rows[k] = r[k];
                rows[3 + k] = u[k];
                rows[6 + k] = -f[k];
            }
            char label[64];
            if (all_entries)
                snprintf(label, sizeof label, "car-%d-%s", entry, names[a]);
            if (!all_entries)
                snprintf(label, sizeof label, "%s", names[a]);
            race_view_set_camera(g->view, eye, rows, 1.73205f, 2.25378f, .1f);
            race_view_show_car(g->view, !getenv("PROBE_ISOLATE") || !getenv("PROBE_ENTRY") ||
                                            !atoi(getenv("PROBE_ENTRY")));
            CHECK(shot(g,argv[2],label));
        }
    if(getenv("PROBE_PHOTO")){race_game_close(g);plat_video_shutdown();return 0;}
    if (getenv(
            "PROBE_DIST")) { /* tests: the course seen from 3 m over its line at that distance, 20 m ahead */
        float d = (float)atof(getenv("PROBE_DIST")), p0[3], p1[3];
        if (gt4_race_run_course_point(g->run, d, p0) &&
            gt4_race_run_course_point(g->run, d + 20, p1)) {
            float f[3], r[3], u[3], length = 0;
            eye[0] = p0[0];
            eye[1] = p0[1] + 3;
            eye[2] = p0[2];
            for (int k = 0; k < 3; k++) {
                f[k] = p1[k] - eye[k];
                length += f[k] * f[k];
            }
            length = sqrtf(length);
            for (int k = 0; k < 3; k++)
                f[k] /= length;
            length = sqrtf(f[0] * f[0] + f[2] * f[2]);
            r[0] = -f[2] / length;
            r[1] = 0;
            r[2] = f[0] / length;
            u[0] = -f[1] * r[2];
            u[1] = f[0] * r[2] - f[2] * r[0];
            u[2] = f[1] * r[0];
            for (int k = 0; k < 3; k++) {
                rows[k] = r[k];
                rows[3 + k] = u[k];
                rows[6 + k] = -f[k];
            }
            race_view_set_camera(g->view, eye, rows, 1.73205f, 2.25378f, .1f);
            CHECK(shot(g,argv[2],"course"));
        }
    }
    if (argc > 3 && (!strcmp(argv[3], "--wheel-test") || !strcmp(argv[3], "--wheel-negative"))) {
        /* Same camera/body/pose, synthetic angular exposure only. The player
           rim uses GPU accumulation; no physics state is written. */
        uint8_t *wheel_state = malloc(0x2000000);
        CHECK(wheel_state);
        memcpy(wheel_state, gt4_phys_mem, 0x2000000);
        race_view_set_wheel_motion(g->view, 0, NULL, NULL, 0);
        CHECK(shot(g,argv[2],"sharp"));
        int w, h, w2, h2;
        uint8_t *sharp = plat_sdl_read_pixels(&w, &h);
        CHECK(sharp);
        r3d_stats before = r3d_frame_stats();
        if (strcmp(argv[3], "--wheel-negative")) {
            float current[4] = {1.5f, 1.5f, 1.5f, 1.5f}, previous[4] = {0};
            race_view_set_wheel_motion(g->view, 0, current, previous, 0);
        }
        CHECK(shot(g,argv[2],"blurred"));
        uint8_t *blur = plat_sdl_read_pixels(&w2, &h2);
        CHECK(blur && w==w2 && h==h2);
        r3d_stats after = r3d_frame_stats();
        CHECK(before.draws==after.draws && before.triangles==after.triangles);
        unsigned changed = 0;
        for (int p = 0; p < w * h; p++)
            if (memcmp(sharp + p * 4, blur + p * 4, 4))
                changed++;
        free(sharp);
        CHECK(changed>0);
        CHECK(after.wheel_passes>0);
        CHECK(shot(g,argv[2],"cached"));
        uint8_t *cached = plat_sdl_read_pixels(&w, &h);
        CHECK(cached);
        CHECK(!memcmp(blur,cached,(size_t)w*h*4));
        free(blur);
        free(cached);
        r3d_stats reused = r3d_frame_stats();
        CHECK(!reused.wheel_passes && reused.wheel_cache_hits>0);
        CHECK(glGetError()==GL_NO_ERROR);
        CHECK(!memcmp(wheel_state, gt4_phys_mem, 0x2000000));
        free(wheel_state);
        printf(
            "wheel accumulation: %u changed pixels, %u GPU passes, cached redraw 0 passes, unchanged scene draws/triangles.\n",
            changed, after.wheel_passes);
    }
    race_game_stop(g);
    CHECK(!g->inspect_on && !g->inspect_seeded);
    race_game_close(g);
    plat_video_shutdown();
    puts("race inspection: toggle, pose seed, movement, axes, no clipping and reset passed");
    return 0;
}
