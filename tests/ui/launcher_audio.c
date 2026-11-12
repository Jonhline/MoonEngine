#include "launcher_audio.h"
#include "music.h"
#include "sound.h"
#include "vol.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"launcher audio %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned word(const uint8_t *p)
{
    return p[0] | (unsigned)p[1]<<8 | (unsigned)p[2]<<16 | (unsigned)p[3]<<24;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    CHECK(!launcher_audio_open(NULL,0));
    launcher_audio *a=launcher_audio_open(argv[1],0); CHECK(a);
    Uint64 start=SDL_GetTicks64();
    while(!launcher_audio_ready(a) && SDL_GetTicks64()-start<15000) SDL_Delay(10);
    CHECK(launcher_audio_ready(a));
    const char *error=NULL;
    gt4_vol *v=gt4_vol_open(argv[1],&error); CHECK(v);
    size_t nn=0,bn=0;
    uint8_t *names=gt4_vol_read(v,"/sound/gt4se.inf",&nn);
    uint8_t *bank=gt4_vol_read(v,"/sound/gt4sys.ins",&bn); CHECK(names && bank);
    const char *effects[]={"cursor","select","ok","cancel"};
    for(int k=0;k<4;k++) {
        int16_t *expected=NULL; size_t frames=0;
        for(unsigned i=0;i<word(names+12);i++) {
            uint8_t *e=names+16+8*i;
            if (!strcmp((char *)names+word(e),effects[k])) {
                CHECK(e[7]==0 && gt4_sound_render(bank,bn,e[5],&expected,&frames)); break;
            }
        }
        CHECK(expected && frames);
        launcher_audio_suspend_music(a,1);
        launcher_audio_volume(a,0,128); launcher_audio_play(a,effects[k]);
        int16_t *actual=calloc(frames,4); CHECK(actual);
        launcher_audio_render(a,actual,(int)frames);
        if(argc>2 && k==0) actual[0]^=1;
        CHECK(!memcmp(actual,expected,frames*4));
        int nonzero=0; for(size_t i=0;i<frames*2;i++) nonzero|=actual[i]!=0;
        CHECK(nonzero);
        free(actual); free(expected);
    }
    int16_t silence[2048];
    launcher_audio_volume(a,0,0); launcher_audio_play(a,"ok");
    launcher_audio_render(a,silence,1024);
    for(int i=0;i<2048;i++) CHECK(!silence[i]);
    launcher_audio_close(a);
    /* BGM comparison starts a fresh decoder/playhead independently. */
    a=launcher_audio_open(argv[1],0); CHECK(a);
    start=SDL_GetTicks64();
    while(!launcher_audio_ready(a) && SDL_GetTicks64()-start<15000) SDL_Delay(10);
    CHECK(launcher_audio_ready(a));
    gt4_music *m=gt4_music_open(v,"map"); CHECK(m && gt4_music_prepare_begin(m,1));
    for(int i=0;i<8;i++) CHECK(gt4_music_prepare_step(m,NULL,NULL));
    int16_t expected[8192],actual[8192];
    gt4_music_render_available(expected,4096,m,4096);
    launcher_audio_volume(a,120,0);
    launcher_audio_suspend_music(a,1); launcher_audio_render(a,actual,4096);
    for(int i=0;i<8192;i++) CHECK(!actual[i]);
    launcher_audio_suspend_music(a,0); launcher_audio_render(a,actual,4096);
    CHECK(!memcmp(actual,expected,sizeof actual));
    int audible=0;
    for(int block=0;block<80 && !audible;block++) {
        SDL_Delay(5);
        launcher_audio_render(a,actual,4096);
        for(int i=0;i<8192;i++) audible|=actual[i]!=0;
    }
    CHECK(audible);
    gt4_music_close(m); launcher_audio_close(a);
    a=launcher_audio_open("missing-launcher-disc.iso",0); CHECK(a);
    SDL_Delay(50); launcher_audio_render(a,silence,1024);
    CHECK(!launcher_audio_ready(a));
    for(int i=0;i<2048;i++) CHECK(!silence[i]);
    launcher_audio_close(a);
    free(names); free(bank); gt4_vol_close(v);
    puts("PASS launcher: four GTSE waveforms, native SQT prefix, mute and missing disc");
    return 0;
}
