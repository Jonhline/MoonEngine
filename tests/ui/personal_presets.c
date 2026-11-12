#include "options_presets.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"personal presets %d: %s\n",__LINE__,#x); return 1;} } while(0)
static int write_text(const char *path,const char *text)
{
    FILE *f=fopen(path,"wb"); if (!f) return 0;
    int ok=fputs(text,f)>=0; return !fclose(f) && ok;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    moon_options original={0}, imported={0}, destination={0}; char error[160];
    CHECK(moon_options_set(&original,"graphics.ui_filter","2"));
    CHECK(moon_options_set(&original,"graphics.texture_filtering","4"));
    CHECK(moon_options_set(&original,"controls.desktop_menu","1"));
    CHECK(moon_options_set(&original,"graphics.width","3840"));
    CHECK(moon_options_set(&original,"disc","private/game.iso"));
    CHECK(moon_options_set(&original,"save.directory","private/save"));
    CHECK(moon_preset_write(&original,argv[1],"Touring",error,sizeof error));
    CHECK(moon_preset_read(&imported,argv[1],error,sizeof error));
    CHECK(imported.count==3 && !imported.modified && !strcmp(imported.preset,"Touring"));
    CHECK(!moon_options_get(&imported,"disc",NULL));
    CHECK(!moon_options_get(&imported,"graphics.width",NULL));
    CHECK(moon_options_set(&destination,"graphics.width","1280"));
    CHECK(moon_options_set(&destination,"future.local","keep"));
    snprintf(destination.path,sizeof destination.path,"private/local.ini");
    if (argc>2) CHECK(moon_options_set(&imported,"graphics.texture_filtering","0"));
    CHECK(moon_preset_merge(&destination,&imported));
    CHECK(!strcmp(moon_options_get(&destination,"graphics.texture_filtering",""),"4"));
    CHECK(!strcmp(moon_options_get(&destination,"graphics.width",""),"1280"));
    CHECK(!strcmp(moon_options_get(&destination,"future.local",""),"keep"));
    CHECK(!strcmp(destination.path,"private/local.ini"));
    CHECK(!moon_preset_write(&original,argv[1],"Classic",error,sizeof error));
    CHECK(!moon_preset_write(&original,argv[1],"Enhanced",error,sizeof error));
    CHECK(!moon_preset_write(&original,argv[1],"Bad\nname",error,sizeof error));
    CHECK(moon_preset_read(&imported,argv[1],error,sizeof error));
    moon_options before=imported;
    const char *invalid[]={
        "MoonEnginePreset=2\nname=X\ngraphics.ui_filter=0\n",
        "MoonEnginePreset=1\nname=X\ngraphics.ui_filter=99\n",
        "MoonEnginePreset=1\nname=X\ngraphics.ui_filter=1\ngraphics.ui_filter=2\n",
        "MoonEnginePreset=1\nname=X\ndisc=private/game.iso\n",
        "MoonEnginePreset=1\nname=X\nname=Y\ngraphics.ui_filter=0\n",
        "MoonEnginePreset=1\nname=Classic\ngraphics.ui_filter=0\n",
        "MoonEnginePreset=1\nname=X\ngraphics.upscaling=1\ngraphics.antialiasing=2\n",
        "MoonEnginePreset=1\nname=X\ngraphics.ui_filter=1garbage\n",
        "MoonEnginePreset=1\nname=X\ngraphics.ui_filter=999999999999999999999999999999\n",
        "MoonEnginePreset=1\nname=X\ngraphics.ui_filter=1\rhidden\n",
        "MoonEnginePreset=1\nname=X\n"
    };
    for (unsigned i=0;i<sizeof invalid/sizeof *invalid;i++) {
        CHECK(write_text(argv[1],invalid[i]));
        CHECK(!moon_preset_read(&imported,argv[1],error,sizeof error));
        CHECK(!memcmp(&imported,&before,sizeof imported));
        CHECK(error[0]);
    }
    remove(argv[1]);
    puts("Personal presets: roundtrip, private-path exclusion, protected defaults, transactional invalid imports passed");
    return 0;
}
