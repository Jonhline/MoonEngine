/* Launcher disc/cache boundary, independent of a game or a saved career. */
#include "launcher.h"
#include "launcher_skin.h"
#include "gpb.h"
#include "tex1.h"
#include "vol.h"
#include "save.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"launcher skin line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(int argc, char **argv)
{
    CHECK(argc>=3);
    char message[256];
    CHECK(launcher_validate_disc(argv[1],message,sizeof message));
    CHECK(gt4_save_mkdir(argv[2]));
    launcher_skin cold={0},warm={0};
    CHECK(launcher_skin_open(&cold,argv[1],argv[2]));
    CHECK(launcher_skin_open(&warm,argv[1],argv[2]) && warm.cache_hit);
    const char *error=NULL; size_t package_n=0;
    gt4_vol *vol=gt4_vol_open(argv[1],&error); CHECK(vol);
    uint8_t *package=gt4_vol_read(vol,"/projects/option/US/OptionRoot.gpb",&package_n);
    size_t cursor_n=0;
    uint8_t *cursor=gt4_vol_read(vol,"/projects/cursor/US/CursorRoot.gpb",&cursor_n);
    size_t controls_n=0;
    uint8_t *controls=gt4_vol_read(vol,"/projects/option/US/SoundRoot.gpb",&controls_n);
    gt4_vol_close(vol); CHECK(package && cursor && controls);
    size_t pixels=0;
    for (int i=0;i<34;i++) {
        size_t n=0; uint8_t *asset=gpb_load(i>=13 && i<33?controls:i==12?cursor:package,i>=13 && i<33?controls_n:i==12?cursor_n:package_n,launcher_skin_asset(i),&n);
        tex1_image *images=NULL; CHECK(asset && tex1_decode(asset,n,&images)==1); free(asset);
        CHECK(cold.images[i].width==images[0].width && cold.images[i].height==images[0].height);
        CHECK(warm.images[i].width==images[0].width && warm.images[i].height==images[0].height);
        size_t bytes=(size_t)images[0].width*images[0].height*4;
        if (argc>3 && !strcmp(argv[3],"--negative") && i==4) warm.images[i].rgba[0]^=1;
        CHECK(!memcmp(cold.images[i].rgba,images[0].rgba,bytes));
        CHECK(!memcmp(warm.images[i].rgba,images[0].rgba,bytes));
        pixels+=bytes/4; tex1_free(images,1);
    }
    free(package); free(cursor); free(controls);
#ifdef _WIN32
    HCURSOR pointer=launcher_skin_cursor(&warm); CHECK(pointer);
    ICONINFO pointer_info={0}; CHECK(GetIconInfo(pointer,&pointer_info));
    CHECK(!pointer_info.fIcon && pointer_info.xHotspot==16 && pointer_info.yHotspot==0);
    DeleteObject(pointer_info.hbmColor); DeleteObject(pointer_info.hbmMask); DestroyCursor(pointer);
    const mp_node *style=mp_find_named(launcher_skin_widget(&warm,"Composite","OptionItem"),"TextFace","label");
    CHECK(launcher_skin_paragraph(&warm,NULL,style,(RECT){0,0,200,200},L"Start the game with your selected disc and settings.")>1);
    CHECK(launcher_skin_paragraph(&warm,NULL,style,(RECT){0,0,200,50},L"Play")==1);
    CHECK(launcher_skin_paragraph(&warm,NULL,style,(RECT){0,0,200,1},L"No room")==0);
#endif
    for(int i=0;i<2;i++) CHECK(cold.font_size[i]==warm.font_size[i] &&
                             !memcmp(cold.font_data[i],warm.font_data[i],cold.font_size[i]));
    for(int i=0;i<3;i++) {
        CHECK(cold.fonts[i].count==warm.fonts[i].count && cold.fonts[i].count>0);
        CHECK(!memcmp(cold.fonts[i].glyphs,warm.fonts[i].glyphs,
                      (size_t)cold.fonts[i].count*sizeof(gt4_glyph)));
    }
    double item[4],label[4],icon[4],scroll[4];
    const mp_node *widget=launcher_skin_widget(&warm,"Composite","Item"); CHECK(widget);
    CHECK(mp_numbers(widget,"geometry",item,4)==4);
    CHECK(mp_numbers(mp_find_named(widget,"TextFace","label"),"geometry",label,4)==4);
    CHECK(mp_numbers(mp_find_named(widget,"ImageFace","bg"),"geometry",icon,4)==4);
    CHECK(mp_numbers(launcher_skin_widget(&warm,"ScrollWindow","ScrollWindow"),"geometry",scroll,4)==4);
    CHECK(item[2]==102 && item[3]==91 && label[1]==54 && icon[1]==9 && scroll[0]==26 && scroll[1]==62);
    char cache[1600]; strcpy(cache,warm.cache_file);
    launcher_skin_close(&warm);
    CHECK(plat_file_write(cache,"interrupted cache",17));
    CHECK(launcher_skin_open(&warm,argv[1],argv[2]) && !warm.cache_hit);
    for (int i=0;i<34;i++) CHECK(!memcmp(cold.images[i].rgba,warm.images[i].rgba,
                (size_t)cold.images[i].width*cold.images[i].height*4));
    launcher_skin_close(&warm);
    CHECK(launcher_skin_open(&warm,argv[1],argv[2]) && warm.cache_hit);
    launcher_skin_close(&warm); launcher_skin_close(&cold);
    char wrong[1600]; snprintf(wrong,sizeof wrong,"%s/wrong.iso",argv[2]);
    CHECK(plat_file_write(wrong,"not an ISO",10));
    CHECK(!launcher_validate_disc(wrong,message,sizeof message));
    CHECK(!launcher_skin_open(&warm,wrong,argv[2]) && !warm.project);
    launcher_skin_close(&warm);
    printf("PASS launcher skin: 34 disc assets, %zu exact texels, authored geometry, cache reuse/corruption and invalid ISO\n",pixels);
    return 0;
}
