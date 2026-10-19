/* P: the PC preparation entry point. Disc fonts/images/scripts are never
   loaded here. The old standalone Win32 "prepare your disc" window is gone: the
   front-end is now the in-engine wizard and the studio room. bootstrap only
   validates the disc and rewrites argv for the game path (see
   docs/vulcan4/LAUNCHER.md and LAUNCHER-V2.md). */
#include "launcher.h"
#include "launcher_ui.h"
#include "moon_pref.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
extern const char *moon_android_startup_disc(void); /* android_host.c */
#endif

/* No-arg / --launcher is the studio front-end (main runs the first-run wizard when
   nothing is saved, else the room). --launcher-v2 is our own SDL options screen,
   which then runs the game. Anything else is the game CLI and stays untouched.
   Returns 1 = run the game, 2 = front-end, 0 = cancel, -1 = failure. No window
   is created here; the suite can drive it with no disc and no display. */
int launcher_bootstrap(int *argc,char ***argv,launcher_startup *start)
{
    if(*argc>1 && strcmp((*argv)[1],"--launcher") && strcmp((*argv)[1],"--launcher-v2"))return 1;
    int v2=0;const char *file=NULL,*disc=NULL;
    for(int i=2;i<*argc;i++){
        const char *arg=(*argv)[i];
        if(!strcmp(arg,"--launcher-config") && i+1<*argc)file=(*argv)[++i];
        else if(!strcmp(arg,"--launcher-v2"))v2=1;
        else if(!strcmp(arg,"--launcher-v1"))v2=0;
        else if(!strcmp(arg,"--launcher-page") && i+1<*argc)snprintf(start->page,sizeof start->page,"%s",(*argv)[++i]);
        else if(!strcmp(arg,"--launcher-shot") && i+1<*argc)snprintf(start->shot,sizeof start->shot,"%s",(*argv)[++i]);
        else if(!strcmp(arg,"--launcher-install"))start->install_zips=1;
        else if(arg[0]!='-' && !disc)disc=arg;else return -1;
    }
    if(file && strlen(file)>=LAUNCHER_PATH-32)return -1;
    char *pref=moon_pref_path();if(!pref)return -1;
    char config[LAUNCHER_PATH];snprintf(config,sizeof config,"%slauncher.ini",pref);
    launcher_preferences_load(&start->preferences,file?file:config);
#ifdef __ANDROID__
    /* The remembered SAF document is pre-filled from the startup preference as
       an already-open descriptor; launcher.ini's old /proc path is stale across
       processes, and a vanished document leaves the room with no disc chosen. */
    if(!disc){
        const char *saved=moon_android_startup_disc();
        if(saved && *saved)snprintf(start->preferences.disc,sizeof start->preferences.disc,"%s",saved);
        else start->preferences.disc[0]=0;
    }
#endif
    snprintf(start->video_file,sizeof start->video_file,file?"%s.video.ini":"%svideo.ini",file?file:pref);
    if(!start->preferences.saves[0])snprintf(start->preferences.saves,sizeof start->preferences.saves,"%ssaves",pref);
    SDL_free(pref);
    if(disc){if(strlen(disc)>=LAUNCHER_PATH || strpbrk(disc,"\r\n"))return -1;strcpy(start->preferences.disc,disc);}
    if(v2){   /* our own SDL options front door (a later home for pads/video/audio) */
        int result=launcher_ui_run(start);
        if(result!=1)return result;
    }
    start->arguments[0]=(*argv)[0];start->arguments[1]=start->preferences.disc;
    start->arguments[2]="--save-dir";start->arguments[3]=start->preferences.saves;
    start->arguments[4]="--config-file";start->arguments[5]=start->video_file;start->arguments[6]=NULL;
    *argc=6;*argv=start->arguments;
    return v2?1:2;
}
