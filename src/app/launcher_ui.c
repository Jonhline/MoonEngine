/* P: the MOON ENGINE front door. SDL, no game artwork, no disc access at startup.
   The captain's layout (2026-10-15): the title centred at the top, a dock of
   icon+name buttons centred along the bottom with EXIT pinned to the right, and
   his own picture behind everything. Pages open as a panel over the dock.
   Prototype scope: DISC and MODS work; OPTIONS names its limitation. */
#include "launcher_ui.h"
#include "launcher_update.h"
#include "moon_http.h"
#include "miniz.h"
#ifndef _WIN32
#include <pthread.h>
#endif
#include "launcher_text.h"
#include "launcher_fonts_data.h"
#include "mod_manifest.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../platform/common/moon_dirent.h"
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir((p),0777)
#endif
#include "miniz.h"
#include "plat_sdl.h"
#include "platform/platform.h"
/* The project's vendored stb_image is JPEG-only and stdio-less (see
   jpeg_storage.c), so the launcher compiles its own PNG reader into this unit.
   STB_IMAGE_STATIC keeps it private: no symbol clash with that copy. */
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../third_party/stb/stb_image.h"
#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#define UI_W 1280
#define UI_H 800
#define MAX_ENTRIES 256
#define MAX_MODS 32
#define DOCK_ITEMS 7              /* DISC MODS OPTIONS MUSIC START ... EXIT */
#define DOCK_ICON 44
#define DOCK_SLOT 132
#define DOCK_Y_FROM_BOTTOM 156
#define PANEL_W 560
#define PANEL_ROWS 9
#define PANEL_ROW_H 34

/* Modern, restrained: near-black ground, one warm accent, hairline rules. */
#define C_TEXT_R 236
#define C_TEXT_G 238
#define C_TEXT_B 241
#define C_DIM_R 172
#define C_DIM_G 177
#define C_DIM_B 186
#define C_FAINT_R 126
#define C_FAINT_G 131
#define C_FAINT_B 142
#define C_ACCENT_R 232
#define C_ACCENT_G 180
#define C_ACCENT_B 94
#define C_PANEL_R 20
#define C_PANEL_G 22
#define C_PANEL_B 28
#define C_RULE_R 44
#define C_RULE_G 47
#define C_RULE_B 56

enum { PAGE_DISC=0, PAGE_MODS, PAGE_OPTIONS, PAGE_MUSIC, PAGE_START, PAGE_INFO, PAGE_COUNT };
static const char *const page_names[PAGE_COUNT]={"DISC","MODS","OPTIONS","MUSIC","START","INFO"};

typedef struct { char name[512]; int is_dir; } entry;

struct ui {
    SDL_Window *window; SDL_Renderer *ren;
    launcher_startup *start;
    lt_face *face_body,*face_head;
    int w,h,fullscreen;
    SDL_Texture *background; int bg_w,bg_h;
    SDL_Texture *mod_image; int mod_image_for,mod_image_w,mod_image_h;   /* the card's picture */
    int dock;                 /* which dock button is focused */
    int open;                 /* PAGE_* or -1 when nothing is open */
    int cursor[PAGE_COUNT], scroll[PAGE_COUNT];
    char base_path[1024];
    char browse[1024];
    char mods_dir[1400];      /* where mods live: the user's Moon Engine folder */
    char pref_dir[1400];      /* that folder itself (saves, config, mods.ini) */
    entry entries[MAX_ENTRIES]; int nentries;
    char mods[MAX_MODS][256]; int nmods, mod_tick[MAX_MODS];
    char mod_full[MAX_MODS][1400];   /* each mod's own path, so a tick can be handed over */
    char status[256];
    char info_status[160];
    char info_version[32];
    int info_checking, info_downloading, info_ready;
    int quit, result;
};

/* ---- drawing primitives ---- */
static void fill(struct ui *u,int x,int y,int w,int h,int r,int g,int b)
{
    SDL_SetRenderDrawColor(u->ren,(Uint8)r,(Uint8)g,(Uint8)b,255);
    SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(u->ren,&rc);
}
static void rect_a(struct ui *u,int x,int y,int w,int h,int r,int g,int b,int a)
{
    SDL_SetRenderDrawBlendMode(u->ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(u->ren,(Uint8)r,(Uint8)g,(Uint8)b,(Uint8)a);
    SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(u->ren,&rc);
    SDL_SetRenderDrawBlendMode(u->ren,SDL_BLENDMODE_NONE);
}
static void rule(struct ui *u,int x,int y,int w)
{
    fill(u,x,y,w,1,C_RULE_R,C_RULE_G,C_RULE_B);
}
static void text(struct ui *u,int x,int y,const char *s,int size,int r,int g,int b);

/* The game's own selection cursor, taken from the engine screen's drawing code
   (src/app/launcher.c:493): a four-point yellow arrow with a dark outline. The
   dock points with the same hand the game does. */
static void poly_fill(struct ui *u,const int *xs,const int *ys,int n,int r,int g,int b)
{
    int top=ys[0],bottom=ys[0];
    for(int i=1;i<n;i++){if(ys[i]<top)top=ys[i];if(ys[i]>bottom)bottom=ys[i];}
    for(int y=top;y<=bottom;y++){
        int x0=1<<30,x1=-(1<<30);
        for(int i=0;i<n;i++){
            int j=(i+1)%n,yi=ys[i],yj=ys[j];
            if((yi<=y && yj>y)||(yj<=y && yi>y)){
                int x=xs[i]+(int)((long)(y-yi)*(xs[j]-xs[i])/(yj-yi));
                if(x<x0)x0=x;if(x>x1)x1=x;
            }
        }
        if(x1>=x0)fill(u,x0,y,x1-x0+1,1,r,g,b);
    }
}
static void gt4_arrow(struct ui *u,int x,int y,int scale)
{
    static const int px[4]={0,24,14,12},py[4]={0,12,14,24};
    int xs[4],ys[4],ox[4],oy[4];
    for(int i=0;i<4;i++){xs[i]=x+px[i]*scale/2;ys[i]=y+py[i]*scale/2;}
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
        for(int i=0;i<4;i++){ox[i]=xs[i]+dx;oy[i]=ys[i]+dy;}
        poly_fill(u,ox,oy,4,96,62,0);
    }
    poly_fill(u,xs,ys,4,246,196,40);
}
/* Text over a picture needs no band behind it if it carries its own shadow. */
static void text_shadow(struct ui *u,int x,int y,const char *s,int size,int r,int g,int b)
{
    text(u,x+2,y+2,s,size,0,0,0);
    text(u,x,y,s,size,r,g,b);
}
/* Icons are drawn, not shipped: no artwork is needed before the captain's own
   files arrive, and nothing is borrowed. */
static void disc_ring(struct ui *u,int cx,int cy,int radius,int thickness,int r,int g,int b)
{
    for(int dy=-radius;dy<=radius;dy++){
        int span=(int)(sqrt((double)(radius*radius-dy*dy))+0.5);
        int inner=(radius-thickness);
        int ispan=dy>-inner && dy<inner ? (int)(sqrt((double)(inner*inner-dy*dy))+0.5) : 0;
        if(span>0)fill(u,cx-span,cy+dy,span-ispan,1,r,g,b);
        if(span>0)fill(u,cx+ispan,cy+dy,span-ispan,1,r,g,b);
    }
}
static void disc_disc(struct ui *u,int cx,int cy,int radius,int r,int g,int b)
{
    for(int dy=-radius;dy<=radius;dy++){
        int span=(int)(sqrt((double)(radius*radius-dy*dy))+0.5);
        fill(u,cx-span,cy+dy,span*2,1,r,g,b);
    }
}
static void icon_disc(struct ui *u,int cx,int cy,int r,int g,int b)
{
    disc_ring(u,cx,cy,DOCK_ICON/2,4,r,g,b);
    disc_disc(u,cx,cy,4,r,g,b);
}
static void icon_play(struct ui *u,int cx,int cy,int r,int g,int b)
{
    int size=DOCK_ICON/2;
    for(int row=-size;row<=size;row++){
        int span=(int)((size-abs(row))*0.9f);
        if(span>0)fill(u,cx-size/2,cy+row,span*2,1,r,g,b);
    }
}
static void icon_gear(struct ui *u,int cx,int cy,int r,int g,int b)
{
    disc_ring(u,cx,cy,DOCK_ICON/2,5,r,g,b);
    for(int k=0;k<8;k++){
        float a=k*3.14159265f/4.0f;
        int x=cx+(int)(cosf(a)*(DOCK_ICON/2+4)),y=cy+(int)(sinf(a)*(DOCK_ICON/2+4));
        fill(u,x-3,y-3,6,6,r,g,b);
    }
}
static void icon_grid(struct ui *u,int cx,int cy,int r,int g,int b)
{
    int s=9,gap=4;
    for(int row=0;row<2;row++)for(int col=0;col<2;col++)
        fill(u,cx-s-gap/2+col*(s+gap/2),cy-s-gap/2+row*(s+gap/2),s,s,r,g,b);
}
static void icon_power(struct ui *u,int cx,int cy,int r,int g,int b)
{
    disc_ring(u,cx,cy+2,DOCK_ICON/2,4,r,g,b);
    fill(u,cx-2,cy-DOCK_ICON/2-4,4,DOCK_ICON/2+2,r,g,b);
}

/* ---- text ---- */
static lt_font *face_for(struct ui *u,int size)
{
    if(size>=4)return lt_font_size(u->face_head,54);
    if(size==3)return lt_font_size(u->face_head,24);
    if(size==2)return lt_font_size(u->face_body,22);
    if(size==1)return lt_font_size(u->face_body,21);
    return lt_font_size(u->face_body,17);
}
static void text(struct ui *u,int x,int y,const char *s,int size,int r,int g,int b)
{
    SDL_Color colour={(Uint8)r,(Uint8)g,(Uint8)b,255};
    lt_draw(u->ren,face_for(u,size),x,y,s,size==3?2:0,colour);
}
static int text_w(struct ui *u,const char *s,int size)
{
    return lt_width(face_for(u,size),s,size==3?2:0);
}
static void text_fit(struct ui *u,int x,int y,const char *s,int size,int maxw,int r,int g,int b)
{
    lt_font *font=face_for(u,size);
    int tracking=size==3?2:0;
    if(lt_width(font,s,tracking)<=maxw){text(u,x,y,s,size,r,g,b);return;}
    char buf[512];size_t n=0;
    while(s[n] && n<sizeof buf-4){
        buf[n]=s[n];buf[n+1]=0;
        if(lt_width(font,buf,tracking)>maxw){buf[n]=0;break;}
        n++;
    }
    if(n>=2){buf[n-1]='.';buf[n-2]='.';}
    text(u,x,y,buf,size,r,g,b);
}

/* ---- the captain's picture, behind everything ---- */
static int has_suffix(const char *s,const char *suffix)
{
    size_t n=strlen(s),m=strlen(suffix);return n>m && !strcasecmp(s+n-m,suffix);
}
static void load_background(struct ui *u)
{
    char path[1200];int w=0,h=0,channels=0;unsigned char *pixels=NULL;
    snprintf(path,sizeof path,"%sbackground.png",u->base_path);
    pixels=stbi_load(path,&w,&h,&channels,4);
    if(!pixels){
        char dir[1200];snprintf(dir,sizeof dir,"%sbackgrounds",u->base_path);
        DIR *d=opendir(dir);
        if(d){
            struct dirent *e;
            while((e=readdir(d))){
                if(!has_suffix(e->d_name,".png"))continue;
                snprintf(path,sizeof path,"%s/%s",dir,e->d_name);
                pixels=stbi_load(path,&w,&h,&channels,4);
                if(pixels)break;
            }
            closedir(d);
        }
    }
    if(!pixels)return;
    /* stb hands back RGBA byte order, which is SDL's ABGR8888 on little-endian */
    u->background=SDL_CreateTexture(u->ren,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STATIC,w,h);
    if(u->background){
        SDL_UpdateTexture(u->background,NULL,pixels,w*4);
        u->bg_w=w;u->bg_h=h;
        printf("launcher: background %dx%d from %s\n",w,h,path);
    }
    stbi_image_free(pixels);
}
/* The mod's own picture, from the manifest's `image` key: a file name inside the
   mod folder and nothing else. Kept for the selected row so moving up and down
   the list does not reload it every frame. */
static void load_mod_image(struct ui *u,int idx)
{
    if(u->mod_image && u->mod_image_for==idx)return;
    if(u->mod_image){SDL_DestroyTexture(u->mod_image);u->mod_image=NULL;}
    u->mod_image_for=-1;u->mod_image_w=u->mod_image_h=0;
    const char *folder=u->mod_full[idx][0]?u->mod_full[idx]:u->mods[idx];
    char name[256],path[1700];
    if(!moon_manifest_field(folder,"image",name,sizeof name))return;
    if(!name[0] || strchr(name,'/') || strchr(name,'\\') || strstr(name,".."))return;
    snprintf(path,sizeof path,"%s/%s",folder,name);
    int w=0,h=0,ch=0;
    unsigned char *pixels=stbi_load(path,&w,&h,&ch,4);
    if(!pixels)return;
    u->mod_image=SDL_CreateTexture(u->ren,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STATIC,w,h);
    if(u->mod_image){
        SDL_UpdateTexture(u->mod_image,NULL,pixels,w*4);
        u->mod_image_w=w;u->mod_image_h=h;u->mod_image_for=idx;
    }
    stbi_image_free(pixels);
}
static void draw_background(struct ui *u)
{
    if(!u->background){
        /* no picture yet: a quiet vertical gradient, never a flat void */
        for(int y=0;y<u->h;y++){
            int shade=18-(y*10)/u->h;
            fill(u,0,y,u->w,1,shade,shade+1,shade+4);
        }
        return;
    }
    float sx=(float)u->w/(float)u->bg_w,sy=(float)u->h/(float)u->bg_h;
    float s=sx>sy?sx:sy;                        /* cover, never stretch */
    int dw=(int)(u->bg_w*s),dh=(int)(u->bg_h*s);
    SDL_Rect dst={(u->w-dw)/2,(u->h-dh)/2,dw,dh};
    SDL_RenderCopy(u->ren,u->background,NULL,&dst);
    /* The captain does not want bands behind the title or the dock, so there are
       none: one uniform, barely-there dim instead (no visible edge anywhere),
       and every string carries its own shadow. */
    rect_a(u,0,0,u->w,u->h,8,9,12,48);
}

/* ---- the disc browser ---- */
static void browse_scan(struct ui *u)
{
    u->nentries=0;
    DIR *d=opendir(u->browse[0]?u->browse:".");if(!d)return;
    struct dirent *e;
    while((e=readdir(d)) && u->nentries<MAX_ENTRIES){
        if(!strcmp(e->d_name,"."))continue;
        int is_dir=0;
#ifdef DT_DIR
        is_dir=e->d_type==DT_DIR;
#endif
        if(!is_dir && !strcasecmp(e->d_name,".."))is_dir=1;
        if(!is_dir && !has_suffix(e->d_name,".iso"))continue;
        snprintf(u->entries[u->nentries].name,sizeof u->entries[0].name,"%s",e->d_name);
        u->entries[u->nentries].is_dir=is_dir;u->nentries++;
    }
    closedir(d);
    for(int i=0;i<u->nentries;i++)for(int j=i+1;j<u->nentries;j++){
        int swap=(u->entries[j].is_dir!=u->entries[i].is_dir)?u->entries[j].is_dir
                 :strcmp(u->entries[j].name,u->entries[i].name)<0;
        if(swap){entry t=u->entries[i];u->entries[i]=u->entries[j];u->entries[j]=t;}
    }
}
static void browse_enter(struct ui *u)
{
    if(u->cursor[PAGE_DISC]<0 || u->cursor[PAGE_DISC]>=u->nentries)return;
    entry *e=&u->entries[u->cursor[PAGE_DISC]];
    if(!strcmp(e->name,"..")){
        char *slash=strrchr(u->browse,'/');
        if(slash && slash!=u->browse)*slash=0;else if(slash==u->browse)u->browse[1]=0;
        browse_scan(u);u->cursor[PAGE_DISC]=u->scroll[PAGE_DISC]=0;return;
    }
    if(e->is_dir){
        size_t n=strlen(u->browse);
        snprintf(u->browse+n,sizeof u->browse-n,"%s%s",n && u->browse[n-1]=='/'?"":"/",e->name);
        browse_scan(u);u->cursor[PAGE_DISC]=u->scroll[PAGE_DISC]=0;return;
    }
    snprintf(u->start->preferences.disc,sizeof u->start->preferences.disc,"%s/%s",u->browse,e->name);
    char message[256];
    if(launcher_validate_disc(u->start->preferences.disc,message,sizeof message))
        snprintf(u->status,sizeof u->status,"Disc: %s",e->name);
    else
        snprintf(u->status,sizeof u->status,"Not a PlayStation 2 disc: %s",message);
}
/* Mods live in the user's own Moon Engine folder (<prefs>/mods), so everything
   the engine needs from the user -- saves, config, mods -- is in one place and
   the program's folder stays clean. A `mods` folder beside the exe is still
   read, so a portable install keeps working; the user's folder comes first. */
static void mods_scan_root(struct ui *u, const char *root)
{
    DIR *d=opendir(root);if(!d)return;
    struct dirent *e;
    while((e=readdir(d)) && u->nmods<MAX_MODS){
        if(e->d_name[0]=='.')continue;
        int archive=has_suffix(e->d_name,".zip");       /* a mod that is still a zip */
#ifdef DT_DIR
        if(e->d_type!=DT_DIR && !archive)continue;
#endif
        int seen=0;
        for(int i=0;i<u->nmods;i++)if(!strcasecmp(u->mods[i],e->d_name))seen=1;
        if(seen)continue;
        snprintf(u->mods[u->nmods],sizeof u->mods[0],"%s",e->d_name);
        snprintf(u->mod_full[u->nmods],sizeof u->mod_full[0],"%s/%s",root,e->d_name);
        u->nmods++;
    }
    closedir(d);
}
static void mods_scan(struct ui *u)
{
    u->nmods=0;
    if(u->mods_dir[0])mods_scan_root(u,u->mods_dir);
    if(u->base_path[0]){char beside[1400];snprintf(beside,sizeof beside,"%smods",u->base_path);mods_scan_root(u,beside);}
    for(int i=0;i<u->nmods;i++)for(int j=i+1;j<u->nmods;j++)
        if(strcasecmp(u->mods[j],u->mods[i])<0){
            char t[256];snprintf(t,sizeof t,"%s",u->mods[i]);
            snprintf(u->mods[i],sizeof u->mods[0],"%s",u->mods[j]);
            snprintf(u->mods[j],sizeof u->mods[0],"%s",t);
            char f[1400];snprintf(f,sizeof f,"%s",u->mod_full[i]);
            snprintf(u->mod_full[i],sizeof u->mod_full[0],"%s",u->mod_full[j]);
            snprintf(u->mod_full[j],sizeof u->mod_full[0],"%s",f);
            int tk=u->mod_tick[i];u->mod_tick[i]=u->mod_tick[j];u->mod_tick[j]=tk;
        }
    /* What was ticked last time is ticked again: the tick list is the user's,
       and it is the same list the engine reads when it is started on its own. */
    if(!u->pref_dir[0])return;
    char ini[1500];snprintf(ini,sizeof ini,"%smods.ini",u->pref_dir);
    FILE *f=fopen(ini,"rb");if(!f)return;
    char line[256];
    while(fgets(line,sizeof line,f)){
        size_t n=strlen(line);
        while(n && (line[n-1]=='\n'||line[n-1]=='\r'||line[n-1]==' '||line[n-1]=='\t'))line[--n]=0;
        if(!line[0]||line[0]=='#')continue;
        for(int i=0;i<u->nmods;i++)if(!strcmp(u->mods[i],line))u->mod_tick[i]=1;
    }
    fclose(f);
}
static void clamp(struct ui *u)
{
    int page=u->open;
    if(page<0)return;
    int *c=&u->cursor[page],*s=&u->scroll[page];
    int n=page==PAGE_DISC?u->nentries:page==PAGE_MODS?u->nmods:0;
    if(n<=0){*c=0;*s=0;return;}
    if(*c<0)*c=0;if(*c>=n)*c=n-1;
    if(*c<*s)*s=*c;if(*c>=*s+PANEL_ROWS)*s=*c-PANEL_ROWS+1;if(*s<0)*s=0;
}

/* ---- handing over to the engine ---- */
static void hand_over(struct ui *u)
{
    char joined[4096];joined[0]=0;int enabled=0;
    char ini[1500];ini[0]=0;
    if(u->pref_dir[0])snprintf(ini,sizeof ini,"%smods.ini",u->pref_dir);
    FILE *tick=ini[0]?fopen(ini,"wb"):NULL;
    if(tick)fprintf(tick,"# MOON ENGINE - the mods that are on. Written by the manager, read by the engine.\n");
    for(int i=0;i<u->nmods;i++)if(u->mod_tick[i]){
        if(has_suffix(u->mods[i],".zip"))continue;       /* an archive never reaches the engine */
        const char *full=u->mod_full[i][0]?u->mod_full[i]:u->mods[i];
        if(enabled)strncat(joined,";",sizeof joined-strlen(joined)-1);
        strncat(joined,full,sizeof joined-strlen(joined)-1);
        if(tick)fprintf(tick,"%s\n",u->mods[i]);
        enabled++;
    }
    if(tick)fclose(tick);
    if(enabled){
#ifdef _WIN32
        _putenv_s("MOON_OVERLAY",joined);
#else
        setenv("MOON_OVERLAY",joined,1);
#endif
        printf("launcher: overlay %s\n",joined);
    }
    char message[256];
    if(!u->start->preferences.disc[0]){snprintf(u->status,sizeof u->status,"Choose a disc first.");return;}
    if(!launcher_validate_disc(u->start->preferences.disc,message,sizeof message)){
        snprintf(u->status,sizeof u->status,"Not a PlayStation 2 disc: %s",message);return;
    }
    printf("launcher: disc %s\n",u->start->preferences.disc);
    u->result=1;u->quit=1;
}
/* Say out loud what the mods page is about to show: the same list and the same
   card fields the user is looking at, so a picture taken on a machine nobody can
   sit at can be checked without opening it. */
static void announce_page(struct ui *u)
{
    if(u->open==PAGE_MUSIC){
        char folder[1500];snprintf(folder,sizeof folder,"%sMusic",u->pref_dir);
        int tracks=0;DIR *d=opendir(folder);
        if(d){struct dirent *e;while((e=readdir(d))){if(e->d_name[0]=='.')continue;
            size_t n=strlen(e->d_name);if(n>4&&(!strcasecmp(e->d_name+n-4,".mp3")||!strcasecmp(e->d_name+n-4,".m4a")||
               !strcasecmp(e->d_name+n-4,".ogg")||!strcasecmp(e->d_name+n-4,".flac")))tracks++;}closedir(d);}
        printf("launcher: page MUSIC folder=%s tracks=%d\n",folder,tracks);
        return;
    }
    if(u->open!=PAGE_MODS)return;
    printf("launcher: mods page: %d mod(s) in %s\n",u->nmods,u->mods_dir[0]?u->mods_dir:"(no user folder)");
    for(int i=0;i<u->nmods;i++){
        const char *folder=u->mod_full[i][0]?u->mod_full[i]:u->mods[i];
        char name[256],version[128],image[256],desc[512];
        if(!moon_manifest_field(folder,"name",name,sizeof name))snprintf(name,sizeof name,"%s",u->mods[i]);
        moon_manifest_field(folder,"version",version,sizeof version);
        moon_manifest_field(folder,"image",image,sizeof image);
        moon_manifest_field(folder,"description",desc,sizeof desc);
        printf("launcher:   %s%s%s%s%s%s  [%s]\n",u->mod_tick[i]?"[x] ":"[ ] ",name,
               version[0]?" v":"",version,image[0]?" card ":"",image,folder);
        if(desc[0])printf("launcher:       %s\n",desc);
    }
}
static int install_zip(struct ui *u,int idx);     /* defined with the other actions */
static void activate(struct ui *u)
{
    if(u->dock==DOCK_ITEMS-1){u->result=0;u->quit=1;return;}          /* EXIT */
    if(u->open>=0){
        if(u->open==PAGE_DISC)browse_enter(u);
        else if(u->open==PAGE_MODS){
            int i=u->cursor[PAGE_MODS];
            if(i>=0 && i<u->nmods){
                if(has_suffix(u->mods[i],".zip"))install_zip(u,i);   /* a zip installs itself */
                else u->mod_tick[i]=!u->mod_tick[i];
            }
        }
        else if(u->open==PAGE_START)hand_over(u);
        clamp(u);
        return;
    }
    if(u->dock==PAGE_START)hand_over(u);
    else u->open=u->dock;
    announce_page(u);
    clamp(u);
}

/* ---- the open page, drawn as a panel over the dock ---- */
static void *info_check_worker(void *user)
{
    struct ui *u = user;
    const char *ch = u->start->preferences.channel;
    int nightly = !strcasecmp(ch, "nightly");
    const char *url = nightly ? "https://api.github.com/repos/Jonhline/MoonEngine/releases"
                              : "https://api.github.com/repos/Jonhline/MoonEngine/releases/latest";
    size_t n = 0;
    unsigned char *json = moon_http_get(url, 1 << 20, &n);
    if (!json) {
        snprintf(u->info_status, sizeof u->info_status, "Check failed: network");
        u->info_checking = 0;
        return NULL;
    }
    char tag[64] = "";
    char *p = (char *)json;
    while ((p = strstr(p, "\"tag_name\":\"")) != NULL) {
        p += 12;
        char *end = strchr(p, '"');
        if (!end) break;
        size_t k = (size_t)(end - p);
        if (k >= sizeof tag) k = sizeof tag - 1;
        memcpy(tag, p, k); tag[k] = 0;
        const char *next = strstr(end, "\"tag_name\":\"");
        const char *pre = strstr(end, "\"prerelease\":true");
        if (!nightly || (pre && (!next || pre < next))) break;
        p = end;
    }
    free(json);
    if (!tag[0]) {
        snprintf(u->info_status, sizeof u->info_status, "Check failed: no release");
        u->info_checking = 0;
        return NULL;
    }
    const char *ver = tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag;
    if (launcher_update_newer(ver, MOON_VERSION)) {
        snprintf(u->info_version, sizeof u->info_version, "%s", ver);
        snprintf(u->info_status, sizeof u->info_status, "Download %s (Enter)", ver);
    } else
        snprintf(u->info_status, sizeof u->info_status, "Up to date (%s)", MOON_VERSION);
    u->info_checking = 0;
    return NULL;
}
#ifndef _WIN32
static unsigned long long tar_octal(const unsigned char *p, int n)
{
    unsigned long long v = 0;
    while (n-- && *p >= '0' && *p <= '7') v = v * 8 + (unsigned)(*p++ - '0');
    return v;
}
static int tar_header_ok(const unsigned char *h)
{
    unsigned sum = 0;
    for (int i = 0; i < 512; i++) sum += i >= 148 && i < 156 ? ' ' : h[i];
    return sum == tar_octal(h + 148, 8);
}
static int write_member(const char *base, const char *name, const unsigned char *data, size_t n,
                        char written[][1400], int *count)
{
    char rel[512];
    snprintf(rel, sizeof rel, "%s", name);
    if (!strncmp(rel, "MoonEngine-", 11)) { char *sl = strchr(rel, '/'); if (sl) memmove(rel, sl + 1, strlen(sl + 1) + 1); }
    while (rel[0] == '.' && rel[1] == '/') memmove(rel, rel + 2, strlen(rel + 2) + 1);
    if (!rel[0] || rel[0] == '/' || strstr(rel, "..")) return 0;
    if (!strcmp(rel, "MoonEngine")) snprintf(rel, sizeof rel, "MoonEngine");
    char out[1600];
    snprintf(out, sizeof out, "%s%s.new", base, rel);
    FILE *f = fopen(out, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    if (ok && *count < 64) snprintf(written[(*count)++], 1400, "%s", out);
    return ok;
}
static int extract_targz(const unsigned char *tgz, size_t n, const char *base)
{
    if (n < 18 || tgz[0] != 0x1f || tgz[1] != 0x8b) return 0;
    size_t hdr = 10; int flags = tgz[3];
    if (flags & 4) hdr += 2 + (tgz[hdr] | (tgz[hdr + 1] << 8));
    if (flags & 8) while (hdr < n && tgz[hdr++]) {}
    if (flags & 16) while (hdr < n && tgz[hdr++]) {}
    if (flags & 2) hdr += 2;
    size_t raw_n = 0;
    void *raw = tinfl_decompress_mem_to_heap(tgz + hdr, n - hdr - 8, &raw_n, 0);
    if (!raw) return 0;
    const unsigned char *t = raw;
    char written[64][1400]; int count = 0, ok = 1;
    char longname[512] = "";
    for (size_t at = 0; at + 512 <= raw_n && ok;) {
        const unsigned char *h = t + at;
        if (!h[0]) break;
        if (!tar_header_ok(h)) { ok = 0; break; }
        char name[512];
        snprintf(name, sizeof name, "%s", longname[0] ? longname : (const char *)h);
        unsigned long long sz = tar_octal(h + 124, 12);
        char type = (char)h[156];
        const unsigned char *data = h + 512;
        if (type == 'L') {
            size_t k = sz < sizeof longname - 1 ? (size_t)sz : sizeof longname - 1;
            memcpy(longname, data, k); longname[k] = 0;
        } else if (type == '0' || type == 0) {
            ok = write_member(base, name, data, (size_t)sz, written, &count);
        }
        longname[0] = 0;
        at += 512 + ((sz + 511) & ~511ULL);
    }
    free(raw);
    if (!ok) for (int i = 0; i < count; i++) remove(written[i]);
    return ok && count;
}
#endif
static void *info_download_worker(void *user)
{
    struct ui *u = user;
    const char *ch = u->start->preferences.channel;
    int nightly = !strcasecmp(ch, "nightly");
    const char *url = nightly ? "https://api.github.com/repos/Jonhline/MoonEngine/releases"
                              : "https://api.github.com/repos/Jonhline/MoonEngine/releases/latest";
    size_t n = 0;
    unsigned char *json = moon_http_get(url, 1 << 20, &n);
    if (!json) { snprintf(u->info_status,sizeof u->info_status,"Download failed"); u->info_downloading=0; return NULL; }
    char want[128];
    snprintf(want, sizeof want, "MoonEngine-%s-linux-x86_64.tar.gz", u->info_version);
    char *p = strstr((char *)json, want);
    if (!p) { free(json); snprintf(u->info_status,sizeof u->info_status,"No Linux asset"); u->info_downloading=0; return NULL; }
    char *uurl = strstr(p, "\"browser_download_url\":\"");
    char *size = strstr(p, "\"size\":");
    char *digest = strstr(p, "\"digest\":\"sha256:");
    if (!uurl || !size) { free(json); snprintf(u->info_status,sizeof u->info_status,"Asset parse failed"); u->info_downloading=0; return NULL; }
    uurl += 24; char *ue = strchr(uurl, '"');
    char asset[1024]; size_t k = ue ? (size_t)(ue - uurl) : 0;
    if (!k || k >= sizeof asset) { free(json); snprintf(u->info_status,sizeof u->info_status,"Asset URL bad"); u->info_downloading=0; return NULL; }
    memcpy(asset, uurl, k); asset[k] = 0;
    unsigned long long asize = strtoull(size + 7, NULL, 10);
    char sha[65] = "";
    if (digest) { digest += 17; memcpy(sha, digest, 64); sha[64] = 0; }
    free(json);
    size_t got = 0;
    unsigned char *data = moon_http_get(asset, (size_t)(asize ? asize : (256u << 20)), &got);
    if (!data) { snprintf(u->info_status,sizeof u->info_status,"Download failed"); u->info_downloading=0; return NULL; }
    if (asize && got != asize) { free(data); snprintf(u->info_status,sizeof u->info_status,"Download size mismatch"); u->info_downloading=0; return NULL; }
    if (sha[0]) {
        char tmp[] = "/tmp/moon-dl-XXXXXX";
        int fd = mkstemp(tmp);
        int ok = 0;
        if (fd >= 0) {
            size_t w = write(fd, data, got);
            close(fd);
            ok = w == got;
            if (ok) {
                char cmd[1600], out[256] = "";
                snprintf(cmd, sizeof cmd, "sha256sum '%s' 2>/dev/null", tmp);
                FILE *h = popen(cmd, "r");
                if (h) { if (!fgets(out, sizeof out, h)) out[0] = 0; pclose(h); }
                ok = !strncasecmp(out, sha, 64);
            }
            remove(tmp);
        }
        if (!ok) { free(data); snprintf(u->info_status,sizeof u->info_status,"Download failed"); u->info_downloading=0; return NULL; }
    }
#ifndef _WIN32
    int ok = extract_targz(data, got, u->base_path);
#else
    int ok = 0;
#endif
    free(data);
    if (!ok) { snprintf(u->info_status,sizeof u->info_status,"Download failed"); u->info_downloading=0; return NULL; }
    u->info_ready = 1;
    snprintf(u->info_status,sizeof u->info_status,"Restart to finish (%llu bytes)", asize);
    u->info_downloading = 0;
    return NULL;
}
static void draw_page(struct ui *u)
{
    if(u->open<0)return;
    int w=PANEL_W,h=u->h-260;
    int x=(u->w-w)/2,y=150;
    rect_a(u,x,y,w,h,C_PANEL_R,C_PANEL_G,C_PANEL_B,232);
    rule(u,x,y,w);
    int tx=x+28,ty=y+24;
    text(u,tx,ty,page_names[u->open],3,C_DIM_R,C_DIM_G,C_DIM_B);
    rule(u,tx,ty+38,w-56);
    int list_y=ty+58;
    if(u->open==PAGE_DISC){
        text_fit(u,tx,list_y,u->browse[0]?u->browse:"(no folder)",0,w-56,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        list_y+=30;
        for(int i=0;i<PANEL_ROWS && i+u->scroll[PAGE_DISC]<u->nentries;i++){
            int idx=i+u->scroll[PAGE_DISC];entry *e=&u->entries[idx];
            int sel=idx==u->cursor[PAGE_DISC],ry=list_y+i*PANEL_ROW_H;
            if(sel){
                rect_a(u,tx-12,ry-5,w-32,PANEL_ROW_H-4,255,255,255,18);
                fill(u,tx-12,ry-5,3,PANEL_ROW_H-4,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);
            }
            text_fit(u,tx,ry,e->name,1,w-90-(e->is_dir?24:0),
                     sel?C_TEXT_R:C_DIM_R,sel?C_TEXT_G:C_DIM_G,sel?C_TEXT_B:C_DIM_B);
            if(e->is_dir)text(u,tx+w-72,ry,">",1,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        }
        if(!u->nentries)text(u,tx,list_y,"No .iso files in this folder.",1,C_FAINT_R,C_FAINT_G,C_FAINT_B);
    }else if(u->open==PAGE_MODS){
        char dir[1500];snprintf(dir,sizeof dir,"%s",u->mods_dir[0]?u->mods_dir:"(no user folder)");
        text_fit(u,tx,list_y,dir,0,w-56,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        list_y+=30;
        int list_w=w/2-30;                       /* the list keeps the left half */
        for(int i=0;i<PANEL_ROWS && i+u->scroll[PAGE_MODS]<u->nmods;i++){
            int idx=i+u->scroll[PAGE_MODS],sel=idx==u->cursor[PAGE_MODS],ry=list_y+i*PANEL_ROW_H;
            if(sel){
                rect_a(u,tx-12,ry-5,list_w+4,PANEL_ROW_H-4,255,255,255,18);
                fill(u,tx-12,ry-5,3,PANEL_ROW_H-4,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);
            }
            if(u->mod_tick[idx])text(u,tx,ry,"[x]",1,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);
            else text(u,tx,ry,"[ ]",1,C_FAINT_R,C_FAINT_G,C_FAINT_B);
            text_fit(u,tx+52,ry,u->mods[idx],1,list_w-60,
                     sel?C_TEXT_R:C_DIM_R,sel?C_TEXT_G:C_DIM_G,sel?C_TEXT_B:C_DIM_B);
        }
        if(!u->nmods){
            text(u,tx,list_y,"No mods installed yet.",1,C_FAINT_R,C_FAINT_G,C_FAINT_B);
            list_y+=30;
            text_fit(u,tx,list_y,"A mod is a folder here holding mod.txt and its files.",0,list_w,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        }else{
            /* The card: what the mod says about itself, in the mod's own words.
               Nothing is invented -- a missing key shows as a dash. */
            int cx=x+w/2+16,cy=list_y,cw=w/2-46;
            int idx=u->cursor[PAGE_MODS]; if(idx<0 || idx>=u->nmods)idx=0;
            const char *folder=u->mod_full[idx][0]?u->mod_full[idx]:u->mods[idx];
            if(has_suffix(u->mods[idx],".zip")){       /* still an archive: install, then it is a mod */
                char zline[512];
                text_fit(u,cx,cy,u->mods[idx],2,cw,C_TEXT_R,C_TEXT_G,C_TEXT_B);cy+=34;
                text_fit(u,cx,cy,"a mod archive, not installed yet",1,cw,C_DIM_R,C_DIM_G,C_DIM_B);cy+=26;
                text_fit(u,cx,cy,"ENTER unpacks it here and it becomes a mod",0,cw,C_DIM_R,C_DIM_G,C_DIM_B);cy+=30;
                snprintf(zline,sizeof zline,"archive %s",folder);
                text_fit(u,cx,cy,zline,0,cw,C_FAINT_R,C_FAINT_G,C_FAINT_B);
                goto mod_card_done;
            }
            char name[256],version[128],author[128],price[64],desc[512],adds[256];
            if(!moon_manifest_field(folder,"name",name,sizeof name))snprintf(name,sizeof name,"%s",u->mods[idx]);
            moon_manifest_field(folder,"version",version,sizeof version);
            moon_manifest_field(folder,"author",author,sizeof author);
            moon_manifest_field(folder,"price",price,sizeof price);
            moon_manifest_field(folder,"description",desc,sizeof desc);
            if(!moon_manifest_field(folder,"car",adds,sizeof adds))moon_manifest_field(folder,"course",adds,sizeof adds);

            text_fit(u,cx,cy,name,2,cw,C_TEXT_R,C_TEXT_G,C_TEXT_B);
            cy+=34;
            char line[512];
            snprintf(line,sizeof line,"%s%s%s",version[0]?version:"1.0",author[0]?" - by ":"",author);
            text_fit(u,cx,cy,line,1,cw,C_DIM_R,C_DIM_G,C_DIM_B);
            cy+=26;
            if(price[0]){snprintf(line,sizeof line,"price %s credits",price);text_fit(u,cx,cy,line,1,cw,C_DIM_R,C_DIM_G,C_DIM_B);cy+=26;}
            if(desc[0]){                                   /* the mod's own words, up to three lines */
                const char *p=desc;
                for(int k=0;k<3 && *p;k++){
                    size_t take=strlen(p);
                    if(take>52){take=52;while(take && p[take]!=' ')take--;if(!take)take=52;}
                    char chunk[64];size_t n=take<sizeof chunk-1?take:sizeof chunk-1;
                    memcpy(chunk,p,n);chunk[n]=0;
                    text_fit(u,cx,cy,chunk,0,cw,C_DIM_R,C_DIM_G,C_DIM_B);
                    cy+=22; p+=take; while(*p==' ')p++;
                }
                cy+=10;
            }
            if(adds[0]){snprintf(line,sizeof line,"adds %s",adds);text_fit(u,cx,cy,line,1,cw,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);cy+=26;}
            snprintf(line,sizeof line,"folder %s",folder);
            text_fit(u,cx,cy,line,0,cw,C_FAINT_R,C_FAINT_G,C_FAINT_B);cy+=26;
            load_mod_image(u,idx);
            if(u->mod_image && u->mod_image_w>0 && u->mod_image_h>0){
                float sx=(float)cw/(float)u->mod_image_w,sy=150.0f/(float)u->mod_image_h;
                float s=sx<sy?sx:sy; if(s>1.0f)s=1.0f;
                int iw=(int)(u->mod_image_w*s),ih=(int)(u->mod_image_h*s);
                SDL_Rect dst={cx,cy,iw,ih};
                SDL_RenderCopy(u->ren,u->mod_image,NULL,&dst);
                cy+=ih+16;
            }
            rule(u,cx-16,cy-12,cw+16);
            text_fit(u,cx,cy,"SPACE tick on/off   D show in folder   DEL delete",0,cw,C_FAINT_R,C_FAINT_G,C_FAINT_B);
            mod_card_done: ;
        }
    }else if(u->open==PAGE_OPTIONS){
        text_fit(u,tx,list_y,"MOON ENGINE LIMITATION: video, audio and pad options still",1,w-56,C_DIM_R,C_DIM_G,C_DIM_B);
        list_y+=30;
        text_fit(u,tx,list_y,"live in the engine's own screen (--launcher-v1). They move",1,w-56,C_DIM_R,C_DIM_G,C_DIM_B);
        list_y+=30;
        text_fit(u,tx,list_y,"here next; nothing is silently dropped.",1,w-56,C_DIM_R,C_DIM_G,C_DIM_B);
        list_y+=44;
        text(u,tx,list_y,"SETTINGS FILE",3,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        list_y+=30;
        text_fit(u,tx,list_y,u->start->video_file,0,w-56,C_DIM_R,C_DIM_G,C_DIM_B);
    }else if(u->open==PAGE_MUSIC){
        char folder[1500];snprintf(folder,sizeof folder,"%sMusic",u->pref_dir);
        text_fit(u,tx,list_y,folder[0]?folder:"(no user folder)",0,w-56,C_FAINT_R,C_FAINT_G,C_FAINT_B);
        list_y+=30;
        int tracks=0;DIR *d=opendir(folder);
        if(d){struct dirent *e;while((e=readdir(d))){if(e->d_name[0]=='.')continue;
            size_t n=strlen(e->d_name);if(n>4&&(!strcasecmp(e->d_name+n-4,".mp3")||!strcasecmp(e->d_name+n-4,".m4a")||
               !strcasecmp(e->d_name+n-4,".ogg")||!strcasecmp(e->d_name+n-4,".flac")))tracks++;}closedir(d);}
        char line[128];snprintf(line,sizeof line,"%d tracks found",tracks);
        text(u,tx,list_y,line,1,C_DIM_R,C_DIM_G,C_DIM_B);list_y+=38;
        rule(u,tx,list_y-10,w-56);
        text(u,tx,list_y,"Download music",2,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);list_y+=34;
        text_fit(u,tx,list_y,"The download popup is not wired on the launcher yet.",0,w-56,C_FAINT_R,C_FAINT_G,C_FAINT_B);
    }else if(u->open==PAGE_INFO){
        char line[256];
        snprintf(line,sizeof line,"Version: Moon Engine %s",MOON_VERSION);
        text(u,tx,list_y,line,1,C_TEXT_R,C_TEXT_G,C_TEXT_B);list_y+=34;
        const char *ch=u->start->preferences.channel[0]?u->start->preferences.channel:"stable";
        snprintf(line,sizeof line,"Channel: %s",!strcasecmp(ch,"nightly")?"Nightly":"Stable");
        text(u,tx,list_y,line,1,C_DIM_R,C_DIM_G,C_DIM_B);list_y+=34;
        text(u,tx,list_y,"Left/Right changes the channel",0,C_FAINT_R,C_FAINT_G,C_FAINT_B);list_y+=34;
        text(u,tx,list_y,u->info_checking ? "Checking..." : (u->info_status[0] ? u->info_status : "Check for update (Enter)"),0,C_FAINT_R,C_FAINT_G,C_FAINT_B);
    }else if(u->open==PAGE_START){
        text_fit(u,tx,list_y,u->start->preferences.disc[0]?u->start->preferences.disc:"NO DISC CHOSEN",
                 1,w-56,C_TEXT_R,C_TEXT_G,C_TEXT_B);
        list_y+=40;
        char line[256];int enabled=0;
        for(int i=0;i<u->nmods;i++)enabled+=u->mod_tick[i]?1:0;
        snprintf(line,sizeof line,"mods enabled: %d",enabled);
        text(u,tx,list_y,line,1,C_DIM_R,C_DIM_G,C_DIM_B);
        list_y+=30;
        snprintf(line,sizeof line,"saves: %s",u->start->preferences.saves);
        text_fit(u,tx,list_y,line,0,w-56,C_FAINT_R,C_FAINT_G,C_FAINT_B);
    }
}

/* ---- the dock ---- */
static void dock_slot(struct ui *u,int index,int *cx,int *cy)
{
    int n=DOCK_ITEMS-1;                     /* EXIT is pinned right, not in the row */
    int slot=DOCK_SLOT;
    int total=n*slot;
    int start=(u->w-total)/2+slot/2;
    *cx=index<n?start+index*slot:u->w-slot/2-24;
    *cy=u->h-DOCK_Y_FROM_BOTTOM;
}
static void draw_dock(struct ui *u)
{
    for(int i=0;i<DOCK_ITEMS;i++){
        int cx,cy;dock_slot(u,i,&cx,&cy);
        int on=i==u->dock;
        const char *name=i<PAGE_COUNT?page_names[i]:"EXIT";
        int r=on?C_TEXT_R:C_DIM_R,g=on?C_TEXT_G:C_DIM_G,b=on?C_TEXT_B:C_DIM_B;
        switch(i){
            case 0:icon_disc(u,cx,cy,r,g,b);break;
            case 1:icon_grid(u,cx,cy,r,g,b);break;
            case 2:icon_gear(u,cx,cy,r,g,b);break;
            case 3:icon_gear(u,cx,cy,r,g,b);break;
            case 4:icon_play(u,cx,cy,r,g,b);break;
            case 5:icon_gear(u,cx,cy,r,g,b);break;
            default:icon_power(u,cx,cy,r,g,b);break;
        }
        int tw=text_w(u,name,1);
        text_shadow(u,cx-tw/2,cy+DOCK_ICON/2+14,name,1,
                    on?C_TEXT_R:C_DIM_R,on?C_TEXT_G:C_DIM_G,on?C_TEXT_B:C_DIM_B);
        if(on)gt4_arrow(u,cx+DOCK_ICON/2-6,cy-DOCK_ICON/2-26,2);
    }
}
static void draw(struct ui *u)
{
    draw_background(u);
    int tw=text_w(u,"MOON ENGINE",4);
    text_shadow(u,(u->w-tw)/2,52,"MOON ENGINE",4,C_TEXT_R,C_TEXT_G,C_TEXT_B);
    int sw=text_w(u,"your own disc - no game files",0);
    text_shadow(u,(u->w-sw)/2,124,"your own disc - no game files",0,196,200,208);
    draw_page(u);
    int sy=u->h-40;
    if(u->status[0])text_fit(u,32,sy,u->status,0,u->w-380,C_ACCENT_R,C_ACCENT_G,C_ACCENT_B);
    else text_fit(u,32,sy,"Arrows pick - Enter opens - Esc closes or quits - F11 fullscreen",0,u->w-380,204,208,214);
    draw_dock(u);
    SDL_RenderPresent(u->ren);
}

/* ---- input ---- */
/* Delete a mod: the whole folder, deliberately, after the user says yes. A mod
   is a folder and nothing else, so removing it is unticking it for good. */
static int remove_tree(const char *path)
{
    DIR *d=opendir(path);
    if(!d)return remove(path)==0;
    struct dirent *e;
    int ok=1;
    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        char child[1600];
        if((size_t)snprintf(child,sizeof child,"%s/%s",path,e->d_name)>=(size_t)sizeof child){ok=0;continue;}
        if(!remove_tree(child))ok=0;
    }
    closedir(d);
    if(!ok)return 0;
#ifdef _WIN32
    return _rmdir(path)==0;
#else
    return rmdir(path)==0;
#endif
}
/* ---- installing a mod that is still a .zip -------------------------------
   The captain's format: a mod is a folder OR a zip. Dropping the zip in the mods
   folder is the whole install; the manager unpacks it beside itself and the
   folder becomes the mod. Entry paths are checked: an archive is not an
   invitation to write outside its own folder. */
static void mkdir_p(const char *path)
{
    char buf[1700];
    snprintf(buf,sizeof buf,"%s",path);
    for(char *p=buf+1;*p;p++)
        if(*p=='/'||*p=='\\'){*p=0;MKDIR(buf);*p='/';}
    MKDIR(buf);
}
static int zip_entry_ok(const char *name)
{
    if(!name || !*name)return 0;
    if(name[0]=='/'||name[0]=='\\')return 0;
    if(strchr(name,':'))return 0;                       /* a drive letter, or a stream */
    for(const char *p=name;*p;p++)
        if(p[0]=='.'&&p[1]=='.'&&(p[2]=='/'||p[2]=='\\'||!p[2]))return 0;
    return 1;
}
static int install_zip(struct ui *u,int idx)
{
    const char *zip=u->mod_full[idx][0]?u->mod_full[idx]:u->mods[idx];
    mz_zip_archive za;memset(&za,0,sizeof za);
    if(!mz_zip_reader_init_file(&za,zip,0)){snprintf(u->status,sizeof u->status,"cannot open %s",u->mods[idx]);return 0;}
    char target[1500];
    snprintf(target,sizeof target,"%s",zip);
    size_t n=strlen(target);
    if(n>4)target[n-4]=0;                                /* drop ".zip": the folder is the mod */
    mkdir_p(target);
    int files=0,ok=1;
    mz_uint count=mz_zip_reader_get_num_files(&za);
    for(mz_uint i=0;i<count && ok;i++){
        mz_zip_archive_file_stat st;
        char name[1024];
        if(!mz_zip_reader_file_stat(&za,i,&st) || !mz_zip_reader_get_filename(&za,i,name,sizeof name)){ok=0;break;}
        if(!zip_entry_ok(name))continue;
        char path[1700];
        if((size_t)snprintf(path,sizeof path,"%s/%s",target,name)>=(size_t)sizeof path)continue;
        if(mz_zip_reader_is_file_a_directory(&za,i) || st.m_is_directory){
            mkdir_p(path);
            continue;
        }
        char parent[1700];snprintf(parent,sizeof parent,"%s",path);
        char *slash=strrchr(parent,'/');if(slash){*slash=0;mkdir_p(parent);}
        if(!mz_zip_reader_extract_to_file(&za,i,path,0)){ok=0;break;}
        files++;
    }
    mz_zip_reader_end(&za);
    if(!ok){snprintf(u->status,sizeof u->status,"%s is not a mod archive I can unpack",u->mods[idx]);return 0;}
    printf("launcher: installed %s -> %s (%d file(s))\n",u->mods[idx],target,files);
    snprintf(u->status,sizeof u->status,"installed %s",u->mods[idx]);
    u->mod_tick[idx]=0;                                  /* an archive is not a mod; the folder is */
    mods_scan(u);                                        /* the zip is a folder now */
    return 1;
}
static void show_in_folder(struct ui *u,int idx)
{
    const char *folder=u->mod_full[idx][0]?u->mod_full[idx]:u->mods[idx];
#ifdef _WIN32
    char cmd[1700];snprintf(cmd,sizeof cmd,"explorer \"%s\"",folder);
#else
    char cmd[1700];snprintf(cmd,sizeof cmd,"xdg-open \"%s\" >/dev/null 2>&1 &",folder);
#endif
    if(system(cmd)!=0)snprintf(u->status,sizeof u->status,"could not open %s",folder);
    else snprintf(u->status,sizeof u->status,"opened %s",folder);
}
static void key(struct ui *u,SDL_Keycode k)
{
    if(k==SDLK_ESCAPE){
        if(u->open>=0){u->open=-1;return;}
        u->status[0]=0;u->result=0;u->quit=1;return;
    }
    if((k==SDLK_RIGHT||k==SDLK_LEFT) && u->open==PAGE_INFO){
        const char *cur=u->start->preferences.channel;
        snprintf(u->start->preferences.channel,sizeof u->start->preferences.channel,"%s",
                 !strcasecmp(cur,"nightly")?"stable":"nightly");
        launcher_preferences_save(&u->start->preferences);
        printf("launcher: channel %s\n",u->start->preferences.channel);
        return;
    }
    if(k==SDLK_RIGHT||k==SDLK_LEFT){
        if(u->open>=0)return;                       /* the page holds the arrows */
        u->dock=(u->dock+(k==SDLK_RIGHT?1:DOCK_ITEMS-1))%DOCK_ITEMS;
        u->status[0]=0;return;
    }
    if(k==SDLK_DOWN){if(u->open>=0){u->cursor[u->open]++;clamp(u);}return;}
    if(k==SDLK_UP){if(u->open>=0){u->cursor[u->open]--;clamp(u);}return;}
    if(u->open==PAGE_MODS && u->nmods){
        int i=u->cursor[PAGE_MODS];
        if(i>=0 && i<u->nmods){
            if(k==SDLK_SPACE){u->mod_tick[i]=!u->mod_tick[i];return;}
            if(k==SDLK_d || k==SDLK_F2){show_in_folder(u,i);return;}
            if(k==SDLK_DELETE){
                const char *folder=u->mod_full[i][0]?u->mod_full[i]:u->mods[i];
                char message[1500];
                snprintf(message,sizeof message,
                         "Delete the mod \"%s\"?\n\nThe folder and everything in it goes:\n%s\n\n"
                         "The game and your disc are not touched.",u->mods[i],folder);
                if(plat_dialog("MOON ENGINE - delete a mod",message,1)==1){
                    if(remove_tree(folder)){
                        snprintf(u->status,sizeof u->status,"deleted %s",u->mods[i]);
                        /* read the list again: the row is gone, not merely hidden */
                        memmove(u->mods[i],u->mods[i+1],(size_t)(u->nmods-i-1)*sizeof u->mods[0]);
                        memmove(u->mod_full[i],u->mod_full[i+1],(size_t)(u->nmods-i-1)*sizeof u->mod_full[0]);
                        memmove(&u->mod_tick[i],&u->mod_tick[i+1],(size_t)(u->nmods-i-1)*sizeof u->mod_tick[0]);
                        u->nmods--;clamp(u);
                    }else snprintf(u->status,sizeof u->status,"could not delete %s",folder);
                }
                return;
            }
        }
    }
    if((k==SDLK_RETURN || k==SDLK_KP_ENTER) && u->open==PAGE_INFO) {
        if (u->info_ready) return;
        if (u->info_version[0] && !u->info_downloading) {
            u->info_downloading = 1;
            snprintf(u->info_status,sizeof u->info_status,"Downloading...");
            pthread_t th;
            if (pthread_create(&th,NULL,info_download_worker,u)!=0) {
                u->info_downloading=0;
                snprintf(u->info_status,sizeof u->info_status,"Download failed: thread");
            } else pthread_detach(th);
        } else if (!u->info_checking) {
            u->info_checking = 1;
            snprintf(u->info_status,sizeof u->info_status,"Checking...");
            pthread_t th;
            if (pthread_create(&th,NULL,info_check_worker,u)==0) pthread_detach(th);
            else { u->info_checking=0; snprintf(u->info_status,sizeof u->info_status,"Check failed: thread"); }
        }
        return;
    }
    if(k==SDLK_RETURN || k==SDLK_KP_ENTER)activate(u);
}

/* Apply a downloaded MoonEngine.new before anything is drawn. The running
   image is renamed to .moon-old, the new one takes its place, and the new
   binary starts on the next launch. The .moon-old is removed on the start
   after the new version is already running. */
static void launcher_apply_pending_update(const char *base)
{
#ifdef _WIN32
    char exe[1024];
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (!n || n >= sizeof exe) return;
#else
    char exe[1024];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return;
    exe[n] = 0;
#endif
    char fresh[1200], old[1200];
    snprintf(fresh, sizeof fresh, "%s.new", exe);
    snprintf(old, sizeof old, "%s.moon-old", exe);
    FILE *f = fopen(fresh, "rb");
    if (!f) {
        /* already swapped last start: the old copy has served its rollback */
        remove(old);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    if (size <= 0) return;
    remove(old);
#ifdef _WIN32
    if (!MoveFileExA(exe, old, MOVEFILE_REPLACE_EXISTING) ||
        !MoveFileExA(fresh, exe, MOVEFILE_REPLACE_EXISTING))
        fprintf(stderr, "launcher: update pending, could not swap %s\n", fresh);
#else
    if (rename(exe, old) != 0 || rename(fresh, exe) != 0)
        fprintf(stderr, "launcher: update pending, could not swap %s\n", fresh);
    else
        chmod(exe, 0755);
#endif
    (void)base;
}
int launcher_ui_run(launcher_startup *startup)
{
    struct ui u;memset(&u,0,sizeof u);u.start=startup;u.result=0;u.open=-1;
    char *base=SDL_GetBasePath();
    snprintf(u.base_path,sizeof u.base_path,"%s",base?base:"");
    launcher_apply_pending_update(u.base_path);
    if(base)SDL_free(base);
    /* Mods live with the user's data, not beside the program (the captain's call,
       2026-10-16): saves, config and mods in one folder. */
    if(moon_mods_dir(u.mods_dir,sizeof u.mods_dir)){
        snprintf(u.pref_dir,sizeof u.pref_dir,"%s",u.mods_dir);
        char *cut=strrchr(u.pref_dir,'/'); if(!cut)cut=strrchr(u.pref_dir,'\\');
        if(cut)cut[1]=0; else u.pref_dir[0]=0;
    }
    snprintf(u.browse,sizeof u.browse,"%s",startup->preferences.disc[0]?startup->preferences.disc:u.base_path);
    char *slash=strrchr(u.browse,'/');if(slash)*slash=0;
    /* --launcher-page: open a page without a mouse, for whoever has to look at
       this over a wire. The dock follows, so the picture matches the page. */
    for(int i=0;i<PAGE_COUNT;i++)
        if(startup->page[0] && !strcasecmp(startup->page,page_names[i])){u.dock=i;u.open=i;}
    mods_scan(&u);
    if(startup->install_zips){          /* --launcher-install: a dropped zip becomes a mod */
        for(int i=0;i<u.nmods;i++)
            if(has_suffix(u.mods[i],".zip"))install_zip(&u,i);
        mods_scan(&u);
    }
    announce_page(&u);
    if(SDL_Init(SDL_INIT_VIDEO)!=0){fprintf(stderr,"launcher: SDL_Init failed: %s\n",SDL_GetError());return -1;}
    /* One window for the whole app: the engine's own accessor creates it here
       and hands the same window back at START, so Android never sees a second
       one. The captain sizes this window: resizable, never smaller than the
       dock needs, and F11 toggles fullscreen. */
    plat_video_desc window_desc={UI_W,UI_H,0,0,0,0};
    u.window=plat_video_acquire_window(&window_desc);
    if(!u.window){fprintf(stderr,"launcher: no window: %s\n",SDL_GetError());SDL_Quit();return -1;}
    SDL_SetWindowTitle(u.window,"MOON ENGINE");
    SDL_SetWindowMinimumSize(u.window,900,560);
    SDL_GetWindowSize(u.window,&u.w,&u.h);
    u.ren=SDL_CreateRenderer(u.window,-1,SDL_RENDERER_ACCELERATED);
    if(!u.ren)u.ren=SDL_CreateRenderer(u.window,-1,SDL_RENDERER_SOFTWARE);
    if(!u.ren){fprintf(stderr,"launcher: no renderer: %s\n",SDL_GetError());SDL_DestroyWindow(u.window);SDL_Quit();return -1;}
    u.face_body=lt_face_open(launcher_ttf_BARLOW_REGULAR,(int)sizeof launcher_ttf_BARLOW_REGULAR);
    u.face_head=lt_face_open(launcher_ttf_BARLOWCONDENSED_SEMIBOLD,(int)sizeof launcher_ttf_BARLOWCONDENSED_SEMIBOLD);
    if(!u.face_body || !u.face_head){
        fprintf(stderr,"launcher: the embedded typefaces did not load\n");
        SDL_DestroyRenderer(u.ren);SDL_DestroyWindow(u.window);SDL_Quit();return -1;
    }
    browse_scan(&u);mods_scan(&u);load_background(&u);
    /* Deterministic page for captures, like the engine's own MOON_* hooks. */
    const char *page_env=getenv("MOON_LAUNCHER_PAGE");
    if(page_env){int p=atoi(page_env);if(p>=0 && p<PAGE_COUNT){u.dock=p;u.open=p;}}
    printf("launcher: MOON ENGINE front door, %d folder entries, %d mods, dock %s, page %s\n",
           u.nentries,u.nmods,page_names[u.dock],u.open<0?"none":page_names[u.open]);
    int frames=0;                      /* for --launcher-shot: frames drawn so far */
    while(!u.quit){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT){u.result=0;u.quit=1;}
            else if(ev.type==SDL_KEYDOWN){
                if(ev.key.keysym.sym==SDLK_F11){
                    u.fullscreen=!u.fullscreen;
                    SDL_SetWindowFullscreen(u.window,u.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
                } else key(&u,ev.key.keysym.sym);
            }else if(ev.type==SDL_WINDOWEVENT &&
                    (ev.window.event==SDL_WINDOWEVENT_SIZE_CHANGED ||
                     ev.window.event==SDL_WINDOWEVENT_RESIZED)){
                SDL_GetWindowSize(u.window,&u.w,&u.h);
            }else if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_LEFT){
                int x=ev.button.x,y=ev.button.y;
                for(int i=0;i<DOCK_ITEMS;i++){
                    int cx,cy;dock_slot(&u,i,&cx,&cy);
                    if(x>=cx-DOCK_ICON-14 && x<=cx+DOCK_ICON+14 && y>=cy-DOCK_ICON-10 && y<=cy+DOCK_ICON+30){
                        if(u.dock==i)activate(&u);else u.dock=i;
                        break;
                    }
                }
                if(u.open>=0){
                    int w=PANEL_W,px=(u.w-w)/2,list_y=150+24+58;
                    int row=(y-list_y)/PANEL_ROW_H;
                    int n=u.open==PAGE_DISC?u.nentries:u.open==PAGE_MODS?u.nmods:0;
                    int idx=row+u.scroll[u.open];
                    if(x>=px && x<=px+w && row>=0 && idx<n){u.cursor[u.open]=idx;clamp(&u);activate(&u);}
                    else if(u.open==PAGE_START && x>=px && x<=px+w && y>=list_y)hand_over(&u);
                }
            }
        }
        draw(&u);              /* once per frame: resizing and the picture need it */
        SDL_Delay(16);
        /* --launcher-shot: one frame to a PNG, then out. Same encoder the game's
           own --shot uses, so a picture of the front door can be taken on a
           machine nobody can sit at. */
        if(startup->shot[0] && ++frames==3){
            /* The launcher draws through SDL_Renderer, so its frame is read back
               through SDL, not through the game's GL target. Same PNG encoder the
               game's own --shot uses (miniz). */
            SDL_Surface *surf=SDL_CreateRGBSurfaceWithFormat(0,u.w,u.h,32,SDL_PIXELFORMAT_RGBA32);
            if(surf && SDL_RenderReadPixels(u.ren,NULL,SDL_PIXELFORMAT_RGBA32,surf->pixels,surf->pitch)==0){
                unsigned char *px=malloc((size_t)u.w*u.h*4);
                if(px){
                    for(int row=0;row<u.h;row++)
                        memcpy(px+(size_t)row*u.w*4,(unsigned char *)surf->pixels+(size_t)row*surf->pitch,(size_t)u.w*4);
                    size_t len=0;
                    void *png=tdefl_write_image_to_png_file_in_memory(px,(size_t)u.w,(size_t)u.h,4,&len);
                    FILE *f=png?fopen(startup->shot,"wb"):NULL;
                    if(f){fwrite(png,1,len,f);fclose(f);printf("launcher: wrote %s (%dx%d)\n",startup->shot,u.w,u.h);}
                    else printf("launcher: could not write %s\n",startup->shot);
                    mz_free(png);
                    free(px);
                }
            } else printf("launcher: could not read the frame back\n");
            if(surf)SDL_FreeSurface(surf);
            u.quit=1;
        }
    }
    if(u.background)SDL_DestroyTexture(u.background);
    SDL_DestroyRenderer(u.ren);
    /* START hands the window to the engine: same SDL_Window, not a new one.
       Cancelling or EXIT ends the app, so it may close the window for real. */
    if(u.result!=1){
        SDL_DestroyWindow(u.window);
        SDL_Quit();
    }
    lt_close(u.face_body);lt_close(u.face_head);
    return u.result;
}
