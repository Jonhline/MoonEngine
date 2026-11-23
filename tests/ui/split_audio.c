/* Actual Battle2P camera bridge and shared mixer, no physics mutation. */
#include <SDL.h>
#include "platform/platform.h"
#include "plat_sdl.h"
static int silent_music(plat_audio_cb cb, void *user) { (void)cb; (void)user; return 0; }
static void silent_release(void *user) { (void)user; }
#define plat_audio_set_music silent_music
#define plat_audio_release_music silent_release
#include "../../src/app/race_audio.c"
#undef plat_audio_set_music
#undef plat_audio_release_music
#include "../../src/app/race_game.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"split audio %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int energy(race_audio *a, int car)
{
    int sum=0;
    for(int k=0;k<3;k++)for(int j=0;j<2;j++) {
        gt4_engine_voice *v=&a->engine[car][k].control.voice[j];
        sum+=abs(v->left)+abs(v->right);
    }
    return sum;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    CHECK(!plat_video_init(&(plat_video_desc){640,448,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    race_game *g=race_game_open(argv[1]);CHECK(g);
    gt4_race_desc options={0}; options.tire_2p=1;
    race_game_set_split(g,&options);
    const char *other[]={"mustang_gt_05"}; int paints[]={0}; char error[256];
    CHECK(race_game_start(g,"tsukuba","crx_sir_90",0,other,paints,1,error,sizeof error));
    int ready=0;
    for(int i=0;i<10000 && !ready;i++){ready=race_game_poll(g);SDL_Delay(1);}
    CHECK(ready==1);
    race_game_update(g,0);
    CHECK(g->audio && g->audio->split_listener);
    CHECK(!memcmp(g->audio->second_eye,g->split_eye[1],sizeof g->split_eye[1]));
    CHECK(g->audio->second_view==g->camera_2p);
    /* Move only the listener, keeping the original race state byte-for-byte. */
    float eye[2][3],rows[2][9];int views[2]={0,0};
    memcpy(eye,g->split_eye,sizeof eye);memcpy(rows,g->split_rows,sizeof rows);
    int p2_reference=0;
    for(int scenario=0;scenario<2;scenario++) {
        eye[0][0]=g->split_eye[0][0]+scenario*10000.f;
        race_audio_update_split(g->audio,g->run,eye,rows,views,-1,128);
        if(argc>2)g->audio->split_listener=0; /* previous P1-only bridge */
        race_audio_camera_cut(g->audio);
        for(int tick=0;tick<20;tick++)race_audio_step(g->audio,g->run);
        int p2=energy(g->audio,1);
        fprintf(stderr,"P1 camera offset %d: P2 engine level %d\n",scenario*10000,p2);
        CHECK(p2>0);
        if(!scenario)p2_reference=p2;else CHECK(p2==p2_reference);
    }
    CHECK(g->audio->second_have_eye);
    CHECK(!memcmp(g->audio->second_previous,eye[1],sizeof eye[1]));
    race_audio_update(g->audio,g->run,eye[0],rows[0],0,-1,128);
    CHECK(!g->audio->split_listener);
    race_game_close(g);plat_video_shutdown();
    puts("PASS independent split listeners, distant P1, P2 history and single-view reset");
    return 0;
}
