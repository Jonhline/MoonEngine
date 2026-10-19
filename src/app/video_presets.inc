/* Presentation fields shared by the official launcher and original Options.
   Host resolution/API, paths, game saves and physical devices stay local. */
typedef struct { const char *key; size_t offset; int maximum; } video_preset_field;
#define PF(key, field, maximum) {key, offsetof(pc_video_options, field), maximum}
static const video_preset_field video_preset_fields[] = {
    PF("graphics.ui_filter",filter,2), PF("graphics.text_sharpness",text,100),
    PF("graphics.upscaling",video.upscale,4),
    PF("controls.desktop_menu",desktop,1), PF("controls.gtmode_edge_pan",edge_pan,1),
    PF("interface.input_hints",input_hints,1), PF("interface.main_menu_overview",main_menu,1),
    PF("interface.smooth_mouse_cursor",smooth_cursor,1), PF("interface.mouse_hover_feedback",hover_feedback,1),
    PF("interface.compact_car_strip",compact_cars,1), PF("interface.graphics_swap_key",graphics_swap,1),
    PF("graphics.spatial_visibility",spatial_visibility,1), PF("graphics.resident_reflections",resident_reflections,1),
    PF("graphics.antialiasing",antialiasing,4), PF("graphics.render_scale",render_scale,2),
    PF("graphics.texture_smoothing",texture_smoothing,1), PF("graphics.texture_filtering",texture_filtering,4),
    PF("graphics.car_shadows",car_shadows,1), PF("graphics.material_shading",material_shading,1),
    PF("graphics.reflection_quality",reflection_quality,2), PF("graphics.shadow_maps",shadow_maps,3),
    PF("graphics.modern_lighting",modern_lighting,1), PF("graphics.daylight",daylight,1),
    PF("graphics.solar_hour",solar_hour,10), PF("graphics.solar_cycle",solar_cycle,2),
    PF("graphics.hdr",hdr,1), PF("graphics.sun_rays",sun_rays,2), PF("graphics.ambient_occlusion",ambient_occlusion,1)
};
#undef PF
int video_settings_preset_export(video_settings *s, const char *path, const char *name, int draft)
{
    moon_options snapshot={0};
    const pc_video_options *v=draft ? &s->draft : &s->current;
    for (unsigned i=0;i<sizeof video_preset_fields/sizeof *video_preset_fields;i++) {
        const video_preset_field *f=&video_preset_fields[i];
        int value=*(const int *)((const char *)v+f->offset);
        char text_value[16]; snprintf(text_value,sizeof text_value,"%d",value);
        if (!moon_options_set(&snapshot,f->key,text_value)) return 0;
    }
    if (!moon_preset_write(&snapshot,path,name,s->message,sizeof s->message)) return 0;
    snprintf(s->personal_preset,sizeof s->personal_preset,"%s",name);
    s->personal_modified=0;
    s->preset_draft=-1;
    if (!draft) {
        snprintf(s->store.preset,sizeof s->store.preset,"%s",name);
        save(s);
    }
    return 1;
}
int video_settings_preset_import(video_settings *s, const char *path, int draft)
{
    moon_options imported;
    if (!moon_preset_read(&imported,path,s->message,sizeof s->message)) return 0;
    pc_video_options next=draft ? s->draft : s->current;
    /* A partial quality preset resets mutually exclusive sampling controls. */
    if (moon_options_get(&imported,"graphics.antialiasing",NULL) ||
        moon_options_get(&imported,"graphics.render_scale",NULL) ||
        moon_options_get(&imported,"graphics.upscaling",NULL)) {
        next.antialiasing=0; next.render_scale=2; next.video.upscale=0;
    }
    for (unsigned i=0;i<sizeof video_preset_fields/sizeof *video_preset_fields;i++) {
        const video_preset_field *f=&video_preset_fields[i];
        if (moon_options_get(&imported,f->key,NULL))
            *(int *)((char *)&next+f->offset)=moon_options_int(&imported,f->key,0,0,f->maximum);
    }
    snprintf(s->personal_preset,sizeof s->personal_preset,"%s",imported.preset);
    s->personal_modified=0;
    s->preset_draft=-1;
    s->draft=next;
    if (!draft) {
        s->current=next;
        save(s);
    }
    return 1;
}
int video_settings_preset_dialog(video_settings *s, int exporting, int draft)
{
    s->message[0]=0;
    /* Deterministic file-choice injection for the hidden-window UI fixtures.
       Real windows always use the host picker. */
    const char *fixture=getenv("MOON_TEST_PRESET_PATH");
    if (fixture && getenv("MOON_WINDOW_HIDDEN") && !strcmp(getenv("MOON_WINDOW_HIDDEN"),"1"))
        return exporting ? video_settings_preset_export(s,fixture,"Fixture preset",draft)
                         : video_settings_preset_import(s,fixture,draft);
#ifdef _WIN32
    wchar_t selected[1200]=L"", initial[1200]=L"";
    char directory[1200];
    snprintf(directory,sizeof directory,"%s",s->store.path);
    char *slash=strrchr(directory,'/'), *back=strrchr(directory,'\\');
    if (!slash || (back && back>slash)) slash=back;
    if (slash) {
        snprintf(slash+1,sizeof directory-(size_t)(slash+1-directory),"presets");
        if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,directory,-1,initial,1200))
            _wmkdir(initial);
    }
    if (exporting) MultiByteToWideChar(CP_UTF8,0,s->personal_preset[0] ? s->personal_preset : "My preset",-1,selected,1200);
    OPENFILENAMEW dialog={0}; dialog.lStructSize=sizeof dialog;
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    SDL_Window *window=plat_sdl_window();
    if (window && SDL_GetWindowWMInfo(window,&info)) dialog.hwndOwner=info.info.win.window;
    dialog.lpstrFile=selected; dialog.nMaxFile=1200;
    dialog.lpstrFilter=L"Moon Engine preset\0*.moonpreset\0";
    dialog.lpstrDefExt=L"moonpreset"; dialog.lpstrInitialDir=initial[0] ? initial : NULL;
    dialog.lpstrTitle=exporting ? L"Save personal preset - name, duplicate or export" : L"Load personal preset - preview before applying";
    dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(exporting ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (!(exporting ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) return 0;
    char path[1200];
    if (!WideCharToMultiByte(CP_UTF8,0,selected,-1,path,sizeof path,NULL,NULL)) return 0;
    if (!exporting) return video_settings_preset_import(s,path,draft);
    char name[64];
    const char *base; /* Find the UTF-8 separator; dialog offsets count UTF-16 units. */
    const char *last=strrchr(path,'\\');
    base=last ? last+1 : path;
    size_t length=strlen(base);
    if (length>11 && !_stricmp(base+length-11,".moonpreset")) length-=11;
    if (!length || length>=sizeof name) {
        snprintf(s->message,sizeof s->message,"Use a preset name shorter than 64 bytes."); return 0;
    }
    memcpy(name,base,length); name[length]=0;
    return video_settings_preset_export(s,path,name,draft);
#else
    (void)exporting; (void)draft;
    snprintf(s->message,sizeof s->message,"Preset file selection is not available on this platform yet.");
    return 0;
#endif
}
