#include "options_store.h"
#include "platform/switch_scene.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MAKE_DIR(path) mkdir(path, 0700)
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Switch options line %d: %s\n", __LINE__, #x);                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    MAKE_DIR(argv[1]);
    char legacy[1024], canonical[1024];
    snprintf(legacy, sizeof legacy, "%s/config.ini", argv[1]);
    snprintf(canonical, sizeof canonical, "%s/options.ini", argv[1]);
    remove(canonical);
    FILE *f = fopen(legacy, "wb");
    CHECK(f);
    fputs(
        "disc=sdmc:/switch/opengt4/gt4.iso\nsaves=sdmc:/switch/opengt4/\nwidth=1280\nheight=720\nfuture.value=keep\n",
        f);
    fclose(f);
    moon_options s, r;
    CHECK(moon_options_switch_load(&s,legacy));
    CHECK(!strcmp(s.path,canonical));
    CHECK(moon_options_int(&s,MOON_SWITCH_SCENE_KEY,-1,0,2)==MOON_SWITCH_BALANCED);
    CHECK(moon_options_int(&s,"graphics.width",0,1,5000)==1280);
    for (int enhanced = 0; enhanced < 2; enhanced++) {
        moon_options_preset(&s, enhanced);
        CHECK(moon_options_int(&s,"controls.touch_menu",-1,0,1)==enhanced);
        if (argc > 2)
            moon_options_set(&s, MOON_SWITCH_SCENE_KEY, "1");
        CHECK(moon_options_int(&s,MOON_SWITCH_SCENE_KEY,-1,0,2)==(enhanced?2:0));
        CHECK(moon_options_save(&s));
        CHECK(moon_options_switch_load(&r,legacy));
        CHECK(moon_options_int(&r,MOON_SWITCH_SCENE_KEY,-1,0,2)==(enhanced?2:0));
        CHECK(!strcmp(moon_options_get(&r,"future.value",""),"keep"));
        CHECK(!strcmp(moon_options_get(&r,"disc",""),"sdmc:/switch/opengt4/gt4.iso"));
    }
    CHECK(moon_options_set(&r,MOON_SWITCH_SCENE_KEY,"1") && r.modified);
    CHECK(moon_options_save(&r));
    CHECK(moon_options_switch_load(&s,legacy));
    CHECK(s.modified && moon_options_int(&s,MOON_SWITCH_SCENE_KEY,-1,0,2)==1);
    const unsigned expected[3][2][2] = {
        {{640, 448}, {640, 448}}, {{960, 540}, {1280, 720}}, {{640, 360}, {640, 360}}};
    for (int mode = 0; mode < 3; mode++)
        for (int dock = 0; dock < 2; dock++) {
            unsigned w, h;
            moon_switch_scene_size(mode, dock, &w, &h);
            CHECK(w==expected[mode][dock][0] && h==expected[mode][dock][1]);
        }
    f = fopen(legacy, "rb");
    CHECK(f);
    char text[512] = {0};
    fread(text, 1, sizeof text - 1, f);
    fclose(f);
    CHECK(strstr(text,"width=1280") && !strstr(text,"graphics."));
    puts(
        "Switch resolution: Classic/Balanced/Performance, docked/portable, migration/presets/reload PASS");
    return 0;
}
