/* Re-enter actual pause script without file reads or GPU texture creation. */
#include <SDL.h>
#include "platform/platform.h"
#include "plat_sdl.h"
#include "race_pause.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"pause resident %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);
    for(int split=0;split<2;split++) {
        race_pause *p=race_pause_open(vol,split,!split,0,NULL,NULL);CHECK(p);
        int ready=0;
        for(int i=0;i<80 && !ready;i++)ready=race_pause_prepare(p);
        CHECK(ready);
        /* The volume is deliberately unavailable during every opening/draw. */
        gt4_vol_close(vol);vol=NULL;
        for(int visit=0;visit<6;visit++) {
            if (!(argc>2 && visit==1)) CHECK(race_pause_restart(p,NULL,NULL));
            CHECK(race_pause_state(p)==RACE_PAUSE_OPEN);
            for(int tick=0;tick<120;tick++)race_pause_update(p,1./60);
            plat_video_begin_frame();race_pause_draw(p);
            if (!visit) {
                int w,h;unsigned char *rgba=plat_sdl_read_pixels(&w,&h);
                size_t size=0;void *png=rgba?tdefl_write_image_to_png_file_in_memory(rgba,w,h,4,&size):NULL;
                char path[256];snprintf(path,sizeof path,"nov03-pause-%s-%dp.png",
                    getenv("MOON_GRAPHICS_API")?getenv("MOON_GRAPHICS_API"):"opengl",split?2:1);
                FILE *file=png?fopen(path,"wb"):NULL;
                if(file){fwrite(png,1,size,file);fclose(file);}mz_free(png);free(rgba);
            }
            plat_video_end_frame();
            race_pause_key(p,13);
            for(int tick=0;tick<120;tick++)race_pause_update(p,1./60);
            CHECK(race_pause_state(p)==RACE_PAUSE_CONTINUE);
        }
        race_pause_close(p);
        vol=gt4_vol_open(argv[1],NULL);CHECK(vol);
    }
    gt4_vol_close(vol);plat_video_shutdown();
    puts("PASS 1P/2P resident pause repeatedly opens, draws, continues and resets without disc");
    return 0;
}
