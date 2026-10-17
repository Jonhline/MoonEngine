#include "options_store.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"options store line %d: %s\n",__LINE__,#x); return 1;}} while(0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    char handoff[]="C:\\fixture\\video.ini", explicit_path[]="C:\\fixture\\video.ini";
    CHECK(!moon_options_config_path(handoff,handoff));
    CHECK(moon_options_config_path(explicit_path,handoff)==explicit_path);
    CHECK(!moon_options_config_path(NULL,handoff));
    char custom[]="C:/fixture/test.ini.video.ini";
    CHECK(moon_options_config_path(custom,custom)==custom);
    char legacy[1100];
    snprintf(legacy,sizeof legacy,"%s.legacy",argv[1]);
    remove(argv[1]);
    FILE *f=fopen(legacy,"wb");
    CHECK(f);
    fputs("width=1920\nheight=1080\nrenderer=vulkan\nfuture.value=keep\n",f);
    fclose(f);
    moon_options s, r;
    CHECK(moon_options_load(&s,argv[1],legacy));
    CHECK(moon_options_int(&s,"graphics.width",0,640,7680)==1920);
    CHECK(!strcmp(moon_options_get(&s,"graphics.renderer",""),"vulkan"));
    moon_options_preset(&s,0);
    if(argc>2 && !strcmp(argv[2],"--negative")) moon_options_set(&s,"controls.desktop_menu","1");
    if(argc>2 && !strcmp(argv[2],"--map-negative")) moon_options_set(&s,"controls.gtmode_edge_pan","1");
    if(argc>2 && !strcmp(argv[2],"--hints-negative")) moon_options_set(&s,"interface.input_hints","1");
    CHECK(moon_options_int(&s,"controls.desktop_menu",1,0,1)==0 && !s.modified);
    CHECK(moon_options_int(&s,"controls.gtmode_edge_pan",1,0,1)==0);
    CHECK(moon_options_int(&s,"interface.input_hints",1,0,1)==0);
    if(argc>2 && !strcmp(argv[2],"--scene-negative")) moon_options_set(&s,"graphics.spatial_visibility","1");
    CHECK(moon_options_int(&s,"graphics.spatial_visibility",1,0,1)==0);
    CHECK(moon_options_int(&s,"graphics.resident_reflections",1,0,1)==0);
    CHECK(moon_options_int(&s,"interface.main_menu_overview",1,0,1)==0);
    CHECK(moon_options_int(&s,"interface.smooth_mouse_cursor",1,0,1)==0);
    CHECK(moon_options_int(&s,"interface.mouse_hover_feedback",1,0,1)==0);
    CHECK(moon_options_int(&s,"interface.compact_car_strip",1,0,1)==0);
    CHECK(moon_options_int(&s,"graphics.width",0,640,7680)==1920);
    CHECK(moon_options_save(&s) && moon_options_load(&r,argv[1],NULL));
    CHECK(!strcmp(r.preset,"Classic") && !r.modified);
    moon_options_preset(&r,1);
    CHECK(moon_options_int(&r,"controls.desktop_menu",0,0,1)==1 && !r.modified);
    CHECK(moon_options_int(&r,"controls.gtmode_edge_pan",0,0,1)==1);
    CHECK(moon_options_int(&r,"interface.input_hints",0,0,1)==1);
    CHECK(moon_options_int(&r,"graphics.spatial_visibility",0,0,1)==1);
    CHECK(moon_options_int(&r,"graphics.resident_reflections",0,0,1)==1);
    CHECK(moon_options_int(&r,"interface.main_menu_overview",0,0,1)==1);
    CHECK(moon_options_int(&r,"interface.smooth_mouse_cursor",0,0,1)==1);
    CHECK(moon_options_int(&r,"interface.mouse_hover_feedback",0,0,1)==1);
    CHECK(moon_options_int(&r,"interface.compact_car_strip",0,0,1)==1);
    CHECK(moon_options_set(&r,"controls.desktop_menu","0") && r.modified);
    CHECK(moon_options_save(&r) && moon_options_load(&s,argv[1],NULL));
    CHECK(s.modified && !strcmp(s.preset,"Enhanced"));
    CHECK(!strcmp(moon_options_get(&s,"future.value",""),"keep"));
    CHECK(!moon_options_set(&s,"bad\nkey","1"));
    CHECK(moon_options_set(&s,"controls.desktop_menu","9000"));
    CHECK(moon_options_int(&s,"controls.desktop_menu",0,0,1)==0);
    /* Migrated source is retained and cannot override an existing store. */
    CHECK(moon_options_load(&r,argv[1],legacy));
    CHECK(!strcmp(moon_options_get(&r,"controls.desktop_menu",""),"0"));
    remove(argv[1]); remove(legacy);
    puts("Options migration, presets, modified state, persistence and invalid values passed.");
    return 0;
}
