#include "plat_sdl.h"
#include "photo_pause.h"
#include "miniz.h"
#include "../../src/core/strobe_player.c"
#include "../../src/core/photo_pause.c"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "photo HUD %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int check_text(strb_player *p, const char *path, const char *text)
{
    strb_value field = find_path(p, path);
    return field.t == STRB_OBJ && field.place >= 0 && field.obj &&
           ((inst *)field.obj)->has_text[field.place] &&
           !strcmp(((inst *)field.obj)->text[field.place], text);
}
static int ink(const uint8_t *pixels, int w, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            const uint8_t *v = pixels + 4 * (y * w + x);
            n += v[0] > 100 && v[1] > 40;
        }
    return n;
}
int main(int argc, char **argv)
{
    CHECK(argc>1);
    const float expected[4][4] = {{20,20,410,308}, {20,20,308,410},
                                 {72.480003356933594f,20,307.5f,308}, {58.5f,20,231,410}};
    for (int wide=0;wide<2;wide++)for(int portrait=0;portrait<2;portrait++) {
        float box[4];photo_pause_viewport(portrait,wide,box);
        if(argc>2 && !strcmp(argv[2],"--viewport-negative"))box[2]=640;
        CHECK(!memcmp(box,expected[wide*2+portrait],sizeof box));
    }
    CHECK(!plat_video_init(&(plat_video_desc){640,480,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    gt4_vol *v = gt4_vol_open(argv[1], NULL);
    CHECK(v);
    float a[17];
    int32_t sh[19];
    CHECK(gt4_photo_tables_load(argv[1],a,sh));
    gt4_photo_mode s = {0};
    s.view_angle = 35;
    s.aperture = 7;
    s.shutter = 3;
    s.drive_camera = 2;
    s.drive_cameras = 5;
    photo_pause *host = photo_pause_open(v, &s, a, sh);
    CHECK(host);
    strb_player *p = host->movie;
    photo_pause_draw(host, 1./60, 0);
    CHECK(check_text(p,"TEXTCLIP.ZOOM","f = 35mm"));
    CHECK(check_text(p,"TEXTCLIP.CUR_CAM","3"));
    CHECK(check_text(p,"TEXTCLIP.N_CAM","5"));
    CHECK(check_text(p,"TEXTCLIP.IRIS.TEXT","F 3.2"));
    CHECK(check_text(p,"TEXTCLIP.SHUTTER.TEXT","1/125''"));
    CHECK(check_text(p,"TEXTCLIP.EXPOSURE.TEXT","+0.0 EV"));
    if (argc > 2 && !strcmp(argv[2], "--marker-negative"))
        CHECK(strb_visible(p,"HORZ.FOCUS_MARKER",0));
    if (argc > 2 && !strcmp(argv[2], "--digits-negative")) {
        /* Reintroduce the previous solid treatment of the authored digit 3. */
        size_t fills = u32(p, 0x68);
        p->d[fills + 29 * 8] = 0;
    }
    plat_video_begin_frame();
    plat_video_set_view(640, 480);
    strb_draw(p);
    int w, ht;
    uint8_t *pixels = plat_sdl_read_pixels(&w, &ht);
    CHECK(pixels);
    /* The real asset has two open brackets and an authored central cross.
       Digit 3 must retain its empty centre instead of an opaque rectangle. */
    CHECK(ink(pixels,w,202,160,248,190)>30);
    int count = ink(pixels, w, 350, 390, 375, 420);
    CHECK(count>100 && count<350);
    CHECK(pixels[4*(399*w+365)]<100);
    size_t bytes;
    void *png = tdefl_write_image_to_png_file_in_memory(pixels, w, ht, 4, &bytes);
    CHECK(png);
    char path[1024];
    char *base = SDL_GetBasePath();
    CHECK(base);
    snprintf(path, sizeof path, "%s../photo-hud-%s.png", base,
             getenv("MOON_GRAPHICS_API") ? getenv("MOON_GRAPHICS_API") : "opengl");
    SDL_free(base);
    FILE *f = fopen(path, "wb");
    CHECK(f);
    fwrite(png, 1, bytes, f);
    fclose(f);
    mz_free(png);
    free(pixels);
    plat_video_end_frame();
    s.roll = 90;
    s.manual_focus = 1;
    s.drive_camera = 0;
    for (int i = 0; i < 3; i++)
        strb_tick(p, 1. / 30);
    strb_value vert = find_path(p, "VERT"), horz = find_path(p, "HORZ"), mf = find_path(p, "AFMF");
    CHECK(((inst*)vert.obj)->visible && !((inst*)horz.obj)->visible);
    CHECK(((inst*)mf.obj)->frame==1);
    CHECK(check_text(p,"TEXTCLIP.CUR_CAM","1"));
    photo_pause_focus(host);
    s.focus_requested = 0; /* native readback already consumed the camera request */
    photo_pause_draw(host, 1./60, 0);
    strb_value marker = find_path(p, "VERT.FOCUS_MARKER");
    CHECK(((inst*)marker.obj)->playing && ((inst*)marker.obj)->frame>0);
    CHECK(!host->focus_trigger);
    photo_pause_close(host);
    gt4_vol_close(v);
    plat_video_shutdown();
    puts(
        "PASS authored PhotoPause labels, textured digits, focus brackets, portrait, MF and focus animation");
    return 0;
}
