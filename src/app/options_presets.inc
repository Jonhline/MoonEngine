/* Shared implementation, included by options_store.c so every launcher and
   platform links the same interchange rules without a second preference store. */
#include "options_presets.h"
#include <errno.h>
typedef struct { const char *key; int maximum; } preset_rule;
static const preset_rule preset_rules[] = {
    {"graphics.ui_filter",2}, {"graphics.text_sharpness",100},
    {"graphics.upscaling",4}, {"graphics.antialiasing",4},
    {"graphics.render_scale",2}, {"graphics.texture_smoothing",1},
    {"graphics.texture_filtering",4}, {"graphics.night_sky",1}, {"graphics.car_lamps",1}, {"graphics.car_shadows",1},
    {"graphics.material_shading",1}, {"graphics.reflection_quality",2},
    {"graphics.shadow_maps",3}, {"graphics.modern_lighting",1},
    {"graphics.daylight",1}, {"graphics.solar_hour",24},
    {"graphics.solar_cycle",2}, {"graphics.hdr",1}, {"graphics.sun_rays",2}, {"graphics.ambient_occlusion",1}, {"graphics.shadow_filter",1},
    {"graphics.spatial_visibility",1}, {"graphics.resident_reflections",1},
    {"controls.desktop_menu",1}, {"controls.gtmode_edge_pan",1},
    {"interface.input_hints",1}, {"interface.main_menu_overview",1},
    {"interface.smooth_mouse_cursor",1}, {"interface.mouse_hover_feedback",1},
    {"interface.compact_car_strip",1}, {"interface.graphics_swap_key",1},
    {"interface.controller_tiles",1}
};
static int preset_fail(char *error, unsigned size, const char *message)
{
    if (error && size) snprintf(error,size,"%s",message);
    return 0;
}
static int preset_value(const char *key, const char *value)
{
    for (unsigned i=0; i<sizeof preset_rules/sizeof *preset_rules; i++) {
        if (strcmp(key,preset_rules[i].key)) continue;
        if (!*value || strspn(value,"0123456789") != strlen(value)) return 0;
        char *end;
        errno=0;
        long v=strtol(value,&end,10);
        return !errno && !*end && v>=0 && v<=preset_rules[i].maximum;
    }
    return 0;
}
static int preset_name(const char *name)
{
    if (!name || !*name || strlen(name)>=64 || !strcmp(name,"Classic") ||
        !strcmp(name,"Enhanced") || !strcmp(name,"Performance") || !strcmp(name,"Modified")) return 0;
    for (const unsigned char *p=(const unsigned char *)name; *p; p++)
        if (*p<32 || *p==127 || *p=='=') return 0;
    return name[0]!=' ' && name[strlen(name)-1]!=' ';
}
static FILE *preset_open(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t wide[1200], wm[8];
    if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,1200) ||
        !MultiByteToWideChar(CP_UTF8,0,mode,-1,wm,8)) return NULL;
    return _wfopen(wide,wm);
#else
    return fopen(path,mode);
#endif
}
int moon_preset_write(const moon_options *s, const char *path, const char *name,
                      char *error, unsigned size)
{
    if (!s || !path || !*path || strlen(path)>1100 || !preset_name(name))
        return preset_fail(error,size,"Choose a personal name (Classic, Enhanced and Performance are protected).");
    moon_options output={0};
    for (int i=0;i<s->count;i++) {
        const moon_option_entry *e=&s->entries[i];
        int registered=0;
        for (unsigned j=0;j<sizeof preset_rules/sizeof *preset_rules;j++)
            registered |= !strcmp(e->key,preset_rules[j].key);
        if (!registered) continue;
        if (!preset_value(e->key,e->value) || !moon_options_set(&output,e->key,e->value))
            return preset_fail(error,size,"An option contains an invalid value.");
    }
    if (!output.count) return preset_fail(error,size,"There are no portable options to export.");
    char tmp[1200]; snprintf(tmp,sizeof tmp,"%s.tmp",path);
    FILE *f=preset_open(tmp,"wb");
    if (!f) return preset_fail(error,size,"Could not create the preset file.");
    int ok=fprintf(f,"MoonEnginePreset=1\nname=%s\n",name)>0;
    for (int i=0;i<output.count;i++)
        if (fprintf(f,"%s=%s\n",output.entries[i].key,output.entries[i].value)<0) ok=0;
    if (fclose(f)) ok=0;
#ifdef _WIN32
    wchar_t wt[1200], wp[1200];
    if (ok) ok=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,tmp,-1,wt,1200) &&
               MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wp,1200) &&
               MoveFileExW(wt,wp,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if (!ok && MultiByteToWideChar(CP_UTF8,0,tmp,-1,wt,1200)) DeleteFileW(wt);
#elif defined(__SWITCH__)
    if (ok) ok=plat_switch_replace_file(tmp,path);
    if (!ok) remove(tmp);
#else
    if (ok) ok=rename(tmp,path)==0;
    if (!ok) remove(tmp);
#endif
    if (!ok) return preset_fail(error,size,"Could not finish writing the preset.");
    if (error && size) *error=0;
    return 1;
}
int moon_preset_read(moon_options *s, const char *path, char *error, unsigned size)
{
    FILE *f=path ? preset_open(path,"rb") : NULL;
    if (!f) return preset_fail(error,size,"Could not open the preset.");
    moon_options result={0};
    char line[512]; int header=0, named=0, ok=1, lines=0;
    for (;;) {
        long position=ftell(f);
        if (!fgets(line,sizeof line,f)) break;
        if (position<0 || ftell(f)-position!=(long)strlen(line)) {ok=0;break;}
        if (++lines>130 || (!strchr(line,'\n') && !feof(f))) {ok=0;break;}
        size_t length=strlen(line);
        if (length && line[length-1]=='\n') line[--length]=0;
        if (length && line[length-1]=='\r') line[--length]=0;
        if (strpbrk(line,"\r\n")) {ok=0;break;}
        if (!header) {header=!strcmp(line,"MoonEnginePreset=1"); if (!header) {ok=0;break;} continue;}
        char *eq=strchr(line,'=');
        if (!eq) {ok=0;break;} *eq++=0;
        if (!strcmp(line,"name")) {
            if (named || !preset_name(eq)) {ok=0;break;}
            snprintf(result.preset,sizeof result.preset,"%s",eq); named=1;
        } else {
            if (!preset_value(line,eq) || moon_options_get(&result,line,NULL) ||
                !moon_options_set(&result,line,eq)) {ok=0;break;}
        }
    }
    ok=ok && !ferror(f) && header && named && result.count>0;
    fclose(f);
    /* Coupled quality controls must represent a state the menu can produce. */
    int aa=moon_options_int(&result,"graphics.antialiasing",0,0,4);
    int scale=moon_options_int(&result,"graphics.render_scale",2,0,2);
    int fsr=moon_options_int(&result,"graphics.upscaling",0,0,4);
    if ((aa>=2 && (scale!=2 || fsr)) || (scale!=2 && fsr)) ok=0;
    if (!ok) return preset_fail(error,size,"Invalid or unsupported preset; settings were not changed.");
    result.modified=0;
    *s=result;
    if (error && size) *error=0;
    return 1;
}
int moon_preset_merge(moon_options *s, const moon_options *preset)
{
    if (!s || !preset || !preset_name(preset->preset)) return 0;
    moon_options merged=*s;
    for (int i=0;i<preset->count;i++) {
        const moon_option_entry *e=&preset->entries[i];
        if (!preset_value(e->key,e->value) || !moon_options_set(&merged,e->key,e->value)) return 0;
    }
    snprintf(merged.preset,sizeof merged.preset,"%s",preset->preset);
    merged.modified=0;
    *s=merged;
    return 1;
}
