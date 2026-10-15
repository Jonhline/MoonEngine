/* Options artwork is decoded from the user's validated disc, never shipped.
   The local cache contains decoded pixels and the authored widget document. */
#include "launcher_skin.h"
#include "gpb.h"
#include "tex1.h"
#include "font_layout.h"
#include "vol.h"
#include "save.h"
#include "miniz.h"
#include "platform/platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const char *launcher_skin_asset(int index)
{
    const char *names[] = {"image/option/opt_bg_4.png", "image/option/opt_window2L.png",
        "image/option/Race.png", "image/option/Replay.png", "image/option/Screen.png",
        "image/option/Sound.png", "image/option/JP/Controller.png", "image/option/Album.png",
        "image/option/Misc.png", "image/labo/exit.png", "image/labo/exit_over.png",
        "image/option/Color.png", "image/cursor/cursor.png",
        "image/DesignWork/bt_1.png", "image/DesignWork/bt_2.png", "image/DesignWork/bt_3.png", "image/DesignWork/bt_4.png", "image/DesignWork/bt_5.png", "image/DesignWork/bt_6.png", "image/DesignWork/bt_o1.png", "image/DesignWork/bt_o2.png", "image/DesignWork/bt_o3.png", "image/DesignWork/bt_o4.png", "image/DesignWork/bt_o5.png", "image/DesignWork/bt_o6.png", "image/DesignWork/txtarea_1.png", "image/DesignWork/txtarea_2.png", "image/DesignWork/txtarea_3.png", "image/DesignWork/txtarea_4.png", "image/DesignWork/popup_pulldown1.png", "image/DesignWork/popup_pulldown2.png", "image/arcade/common/popup_item_button.png", "image/arcade/common/popup_item_button_o.png", "image/option/GT4logo.png"};
    return index >= 0 && index < 34 ? names[index] : NULL;
}
void launcher_skin_close(launcher_skin *s)
{
    if (!s) return;
    mp_free(s->project);
    for (int i = 0; i < 34; i++) { free(s->images[i].rgba); free(s->filtered[i].rgba); }
    launcher_fsr_close(s->filter);
    for (int i=0;i<3;i++) gt4_font_free(&s->fonts[i]);
    for (int i=0;i<2;i++) free(s->font_data[i]);
    memset(s, 0, sizeof *s);
}
const mp_node *launcher_skin_widget(const launcher_skin *s, const char *type, const char *name)
{
    const mp_node *root = s ? mp_find_named(s->project, "RootWindow", "OptionRoot") : NULL;
    return root ? mp_find_named(root, type, name) : NULL;
}
static int parse_project(launcher_skin *s, const uint8_t *bytes, size_t n)
{
    char error[256];
    s->project = mp_parse((const char *)bytes, n, error, sizeof error);
    return launcher_skin_widget(s, "Composite", "Item") &&
           launcher_skin_widget(s, "Composite", "Group") &&
           launcher_skin_widget(s, "ScrollWindow", "ScrollWindow");
}
static uint8_t *read_cache(const char *path, size_t *n)
{
    plat_file *f = plat_file_open(path);
    if (!f) return NULL;
    uint64_t size = plat_file_size(f);
    uint8_t *data = size >= 16 && size <= (32u << 20) ? malloc((size_t)size) : NULL;
    int ok = data && plat_file_read_at(f, 0, data, (size_t)size) == size;
    plat_file_close(f);
    if (!ok) { free(data); return NULL; }
    *n = (size_t)size;
    return data;
}
static int decode_fonts(launcher_skin *s)
{
    const char *names[]={"hb_24","hc_20","hm_16"};
    for (int i=0;i<3;i++) {
        int found=0;
        for (int k=0;k<2 && !found;k++)
            found=gt4_font_decode(s->font_data[k],s->font_size[k],names[i],&s->fonts[i])==1;
        if (!found) return 0;
    }
    return 1;
}
static int unpack_cache(launcher_skin *s, const uint8_t *data, size_t n)
{
    uint32_t header[4];
    if (n < sizeof header) return 0;
    memcpy(header, data, sizeof header);
    if (memcmp(data, "OGS6", 4) || header[1] != n || header[2] > n - 16 ||
        header[3] != mz_crc32(0, data + 16, n - 16)) return 0;
    if (!parse_project(s, data + 16, header[2])) return 0;
    size_t at = 16 + header[2];
    for (int i = 0; i < 34; i++) {
        uint32_t wh[2];
        if (n - at < 8) return 0;
        memcpy(wh, data + at, 8); at += 8;
        if (!wh[0] || !wh[1] || wh[0] > 2048 || wh[1] > 2048) return 0;
        size_t bytes = (size_t)wh[0] * wh[1] * 4;
        if (bytes > n - at) return 0;
        s->images[i].rgba = malloc(bytes);
        if (!s->images[i].rgba) return 0;
        memcpy(s->images[i].rgba, data + at, bytes); at += bytes;
        s->images[i].width = (int)wh[0]; s->images[i].height = (int)wh[1];
    }
    for (int i=0;i<2;i++) {
        uint32_t bytes;
        if (n-at<4) return 0;
        memcpy(&bytes,data+at,4); at+=4;
        if (!bytes || bytes>n-at) return 0;
        s->font_data[i]=malloc(bytes); if (!s->font_data[i]) return 0;
        memcpy(s->font_data[i],data+at,bytes); at+=bytes; s->font_size[i]=bytes;
    }
    return at == n && decode_fonts(s);
}
static void store_cache(launcher_skin *s, const uint8_t *project, size_t project_n)
{
    size_t n = 16 + project_n;
    for (int i = 0; i < 34; i++) n += 8 + (size_t)s->images[i].width * s->images[i].height * 4;
    for (int i=0;i<2;i++) n+=4+s->font_size[i];
    uint8_t *data = malloc(n);
    if (!data) return;
    memcpy(data + 16, project, project_n);
    size_t at = 16 + project_n;
    for (int i = 0; i < 34; i++) {
        uint32_t wh[2] = {(uint32_t)s->images[i].width, (uint32_t)s->images[i].height};
        memcpy(data + at, wh, 8); at += 8;
        size_t bytes = (size_t)wh[0] * wh[1] * 4;
        memcpy(data + at, s->images[i].rgba, bytes); at += bytes;
    }
    for (int i=0;i<2;i++) {
        uint32_t bytes=(uint32_t)s->font_size[i]; memcpy(data+at,&bytes,4); at+=4;
        memcpy(data+at,s->font_data[i],bytes); at+=bytes;
    }
    uint32_t header[4] = {0, (uint32_t)n, (uint32_t)project_n, mz_crc32(0, data + 16, n - 16)};
    memcpy(header, "OGS6", 4); memcpy(data, header, 16);
    /* A failed or interrupted cache write is rejected by length/CRC next time. */
    plat_file_write(s->cache_file, data, n);
    free(data);
}
static int cache_path(char *out, size_t capacity, const char *disc, const char *directory)
{
    char *pref = directory ? NULL : SDL_GetPrefPath("OpenGT4", "OpenGT4");
    if (!directory && (!pref || !*pref)) { SDL_free(pref); return 0; }
    char folder[1500];
    snprintf(folder, sizeof folder, "%s%s", directory ? directory : (pref ? pref : ""),
             directory ? "" : "launcher-options");
    SDL_free(pref);
    if (!*folder || !gt4_save_mkdir(folder)) return 0;
    uint64_t size = 0, stamp = 0;
    plat_file *file = plat_file_open(disc);
    if (!file) return 0;
    size = plat_file_size(file); plat_file_close(file);
#ifdef _WIN32
    wchar_t wide[1600]; WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, disc, -1, wide, 1600) ||
        !GetFileAttributesExW(wide, GetFileExInfoStandard, &attributes)) return 0;
    stamp = ((uint64_t)attributes.ftLastWriteTime.dwHighDateTime << 32) |
            attributes.ftLastWriteTime.dwLowDateTime;
#endif
    uint32_t key = mz_crc32(0, (const uint8_t *)disc, strlen(disc));
    int length = snprintf(out, capacity, "%s/options-%08x-%llx-%llx.bin", folder, key,
                          (unsigned long long)size, (unsigned long long)stamp);
    return length > 0 && (size_t)length < capacity;
}
int launcher_skin_open(launcher_skin *s, const char *disc, const char *cache)
{
    memset(s, 0, sizeof *s);
    if (!disc || !*disc || !cache_path(s->cache_file, sizeof s->cache_file, disc, cache)) return 0;
    size_t n = 0;
    uint8_t *cached = read_cache(s->cache_file, &n);
    if (cached && unpack_cache(s, cached, n)) {
        free(cached); s->cache_hit = 1; return 1;
    }
    free(cached);
    char path[1600]; strcpy(path, s->cache_file);
    launcher_skin_close(s); strcpy(s->cache_file, path);
    const char *error = NULL;
    gt4_vol *volume = gt4_vol_open(disc, &error);
    if (!volume) return 0;
    size_t project_n = 0, package_n = 0;
    uint8_t *project = gt4_vol_read(volume, "/projects/option/option.mproject", &project_n);
    uint8_t *package = gt4_vol_read(volume, "/projects/option/US/OptionRoot.gpb", &package_n);
    s->font_data[0]=gt4_vol_read(volume,"/font/system.fnt",&s->font_size[0]);
    s->font_data[1]=gt4_vol_read(volume,"/font/menu.fnt",&s->font_size[1]);
    size_t cursor_n = 0;
    uint8_t *cursor = gt4_vol_read(volume, "/projects/cursor/US/CursorRoot.gpb", &cursor_n);
    size_t controls_n=0;
    uint8_t *controls=gt4_vol_read(volume,"/projects/option/US/SoundRoot.gpb",&controls_n);
    gt4_vol_close(volume);
    int ok = project && package && cursor && controls && parse_project(s, project, project_n);
    for (int i = 0; ok && i < 34; i++) {
        size_t asset_n = 0;
        uint8_t *asset = gpb_load(i >= 13 && i < 33 ? controls : i == 12 ? cursor : package, i >= 13 && i < 33 ? controls_n : i == 12 ? cursor_n : package_n, launcher_skin_asset(i), &asset_n);
        tex1_image *images = NULL;
        int count = asset ? tex1_decode(asset, asset_n, &images) : -1;
        free(asset);
        ok = count == 1;
        if (ok) {
            s->images[i].width = images[0].width; s->images[i].height = images[0].height;
            s->images[i].rgba = images[0].rgba; images[0].rgba = NULL;
        }
        if (count > 0) tex1_free(images, count);
    }
    ok=ok && s->font_data[0] && s->font_data[1] && decode_fonts(s);
    if (ok) store_cache(s, project, project_n);
    free(project); free(package); free(cursor); free(controls);
    if (!ok) launcher_skin_close(s);
    return ok;
}
void launcher_skin_filter(launcher_skin *s)
{
    if (!s || s->filter) return;
    s->filter=launcher_fsr_open();
    if(!s->filter) return;
    for(int i=0;i<34;i++) {
        if(i==12 || s->images[i].width>1024 || s->images[i].height>1024) continue;
        launcher_skin_image *source=&s->images[i],*output=&s->filtered[i];
        output->width=source->width*2; output->height=source->height*2;
        output->rgba=launcher_fsr_image(s->filter,source->rgba,source->width,source->height,output->width,output->height);
    }
}
#ifdef _WIN32
static void image_opacity(const launcher_skin *s,HDC dc,int image,RECT r,unsigned opacity)
{
    if (!s || image < 0 || image >= 34 || !s->images[image].rgba) return;
    const launcher_skin_image *im = s->filtered[image].rgba ? &s->filtered[image] : &s->images[image];
    BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = im->width; info.bmiHeader.biHeight = -im->height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    uint32_t *pixels;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!bitmap) return;
    for (int i = 0; i < im->width * im->height; i++) {
        const uint8_t *c = im->rgba + i * 4; unsigned a = c[3];
        pixels[i] = (a << 24) | ((c[0]*a/255) << 16) | ((c[1]*a/255) << 8) | (c[2]*a/255);
    }
    HDC source = CreateCompatibleDC(dc); HBITMAP old = SelectObject(source, bitmap);
    AlphaBlend(dc, r.left, r.top, r.right-r.left, r.bottom-r.top, source, 0, 0,
               im->width, im->height, (BLENDFUNCTION){AC_SRC_OVER,0,(BYTE)opacity,AC_SRC_ALPHA});
    SelectObject(source, old); DeleteDC(source); DeleteObject(bitmap);
}
void launcher_skin_image_draw(const launcher_skin *s,HDC dc,int image,RECT r)
{
    image_opacity(s,dc,image,r,255);
}
static void color_face(HDC dc, const mp_node *node, RECT r)
{
    const mp_node *colors = mp_prop(node, "color");
    if (!colors || !colors->count) return;
    double c[4][4];
    for (int i = 0; i < 4; i++) {
        const mp_node *v = colors->items[colors->count == 4 ? i : 0];
        for (int k = 0; k < 4; k++) c[i][k] = k < v->count ? v->items[k]->number : 255;
    }
    int w = r.right-r.left, h = r.bottom-r.top;
    if (w <= 0 || h <= 0) return;
    BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    uint32_t *pixels;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!bitmap) return;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        double u = w > 1 ? (double)x/(w-1) : 0, v = h > 1 ? (double)y/(h-1) : 0;
        double weights[4] = {(1-u)*(1-v),u*(1-v),u*v,(1-u)*v}, rgba[4] = {0};
        for (int k = 0; k < 4; k++) for (int i = 0; i < 4; i++) rgba[k] += weights[i]*c[i][k];
        unsigned a = (unsigned)(rgba[3]+.5);
        pixels[y*w+x] = (a<<24) | ((unsigned)(rgba[0]*a/255+.5)<<16) |
                         ((unsigned)(rgba[1]*a/255+.5)<<8) | (unsigned)(rgba[2]*a/255+.5);
    }
    HDC source = CreateCompatibleDC(dc); HBITMAP old = SelectObject(source, bitmap);
    AlphaBlend(dc,r.left,r.top,w,h,source,0,0,w,h,(BLENDFUNCTION){AC_SRC_OVER,0,255,AC_SRC_ALPHA});
    SelectObject(source,old); DeleteDC(source); DeleteObject(bitmap);
}
void launcher_skin_background(const launcher_skin *s, HDC dc, int width, int height)
{
    /* Original: OptionRoot option.mproject direct ImageFace/ColorFace order. */
    RECT full = {0,0,width,height};
    HBRUSH gray = CreateSolidBrush(RGB(245,245,245)); FillRect(dc,&full,gray); DeleteObject(gray);
    launcher_skin_image_draw(s,dc,0,full);
    const mp_node *root = launcher_skin_widget(s,"RootWindow","OptionRoot");
    const mp_node *const *children; int count = mp_children(root,&children);
    for (int i=0;i<count;i++) if (!strcmp(children[i]->name,"ColorFace")) {
        double g[4] = {0,0,640,480}; mp_numbers(children[i],"geometry",g,4);
        RECT r = {(LONG)lround(g[0]*width/640),(LONG)lround(g[1]*height/480),
                  (LONG)lround((g[0]+g[2])*width/640),(LONG)lround((g[1]+g[3])*height/480)};
        color_face(dc,children[i],r);
    }
}
void launcher_skin_group(const launcher_skin *s, HDC dc, RECT r)
{
    const mp_node *group = launcher_skin_widget(s,"Composite","Group");
    const mp_node *const *children; int count = mp_children(group,&children);
    double geometry[4] = {0,0,520,30}; mp_numbers(group,"geometry",geometry,4);
    for (int i=0;i<count;i++) if (!strcmp(children[i]->name,"ColorFace")) {
        double g[4]={0,0,geometry[2],geometry[3]}; mp_numbers(children[i],"geometry",g,4);
        double sx=(r.right-r.left)/geometry[2],sy=(r.bottom-r.top)/geometry[3];
        RECT dst={r.left+(LONG)lround(g[0]*sx),r.top+(LONG)lround(g[1]*sy),
                  r.left+(LONG)lround((g[0]+g[2])*sx),r.top+(LONG)lround((g[1]+g[3])*sy)};
        color_face(dc,children[i],dst);
    }
}
static void font_line(const gt4_font *font,HDC dc,const char *text,RECT r,double sx,double sy,
                      int align,const uint8_t color[4],double skew,double offset)
{
    const tex1_image *im=&font->atlases[font->natlases>1?1:0];
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=im->width; info.bmiHeader.biHeight=-im->height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    uint32_t *pixels;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0);
    if (!bitmap) return;
    for(int i=0;i<im->width*im->height;i++) {
        const uint8_t *c=im->rgba+i*4; unsigned a=c[3]*color[3]/255;
        pixels[i]=(a<<24)|((c[0]*color[0]*a/65025)<<16)|((c[1]*color[1]*a/65025)<<8)|
                   (c[2]*color[2]*a/65025);
    }
    HDC source=CreateCompatibleDC(dc); HBITMAP old=SelectObject(source,bitmap);
    double width=gt4_font_original_width(font,text,(float)sx),x=r.left+offset;
    if (align%3==1) x+=(r.right-r.left-width)*.5;
    else if (align%3==2) x+=r.right-r.left-width;
    double y=r.top+font->baseline*sy+offset;
    if (align/3==1) y+=(r.bottom-r.top-font->height*sy)*.5;
    else if (align/3==2) y+=r.bottom-r.top-font->height*sy;
    unsigned previous=0;
    for (const unsigned char *at=(const unsigned char *)text;*at;) {
        unsigned code=*at++;
        if ((code&0xe0)==0xc0 && *at) { code=(code&31)<<6; code|=*at++&63; }
        else if (code>=128) code='?';
        const gt4_glyph *g=gt4_font_glyph(font,code);
        if (!g) g=gt4_font_glyph(font,'?');
        if (!g) continue;
        float ink=0,step=gt4_font_original_step(font,previous,code,(float)sx,&ink);
        int gx=(int)lround(x+ink),gy=(int)lround(y+g->y*sy);
        int w=(int)lround(g->w*sx),h=(int)lround(g->h*sy);
        /* Original: 00492978 glyph bearings/contour advance. GDI replaces
           the textured glyph raster boundary, retaining authored skew. */
        if (w>0 && h>0) for (int row=0;row<h;row++) {
            int shift=(int)lround(tan(skew*.0174532925199433)*(gy+row-y));
            AlphaBlend(dc,gx+shift,gy+row,w,1,source,g->u,g->v+row*g->h/h,g->w,1,
                       (BLENDFUNCTION){AC_SRC_OVER,0,255,AC_SRC_ALPHA});
        }
        x+=step; previous=code==' '?0:code;
    }
    SelectObject(source,old); DeleteDC(source); DeleteObject(bitmap);
}
void launcher_skin_text(const launcher_skin *s,HDC dc,const mp_node *style,RECT r,const wchar_t *text)
{
    const char *name=mp_string(style,"font");
    int index=name&&!strcmp(name,"hb_24")?0:name&&!strcmp(name,"hc_20")?1:2;
    char utf8[4096];
    if (!WideCharToMultiByte(CP_UTF8,0,text,-1,utf8,sizeof utf8,NULL,NULL)) return;
    double sx=mp_number(style,"scale_x",1)*900/640,sy=mp_number(style,"scale_y",1)*700/480;
    double rgba[4]={255,255,255,255},skew[2]={0};
    mp_numbers(style,"text_color",rgba,4); mp_numbers(style,"skew",skew,2);
    uint8_t color[4]; for(int k=0;k<4;k++) color[k]=(uint8_t)rgba[k];
    int saved=SaveDC(dc); IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);
    double shadow=mp_number(style,"shadow",0);
    if (shadow>0) {
        double dark[4]={0,0,0,128}; mp_numbers(style,"shadow_color",dark,4);
        uint8_t tint[4]; for(int k=0;k<4;k++) tint[k]=(uint8_t)dark[k];
        font_line(&s->fonts[index],dc,utf8,r,sx,sy,(int)mp_number(style,"align",1),tint,skew[0],shadow);
    }
    font_line(&s->fonts[index],dc,utf8,r,sx,sy,(int)mp_number(style,"align",1),color,skew[0],0);
    RestoreDC(dc,saved);
}

HCURSOR launcher_skin_cursor(const launcher_skin *s)
{
    const launcher_skin_image *im = &s->images[12];
    if (!im->rgba || im->width != 32 || im->height != 32) return NULL;
    /* Original: 00230CF8/0022FFD0, 32x32 sprite rotates about its tip (16,0).
       P: Windows positions the sprite directly at the physical mouse. */
    BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = 32; info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    uint32_t *pixels;
    HBITMAP color = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!color) return NULL;
    for (int i = 0; i < 1024; i++) {
        const uint8_t *c = im->rgba + i * 4;
        pixels[i] = ((unsigned)c[3] << 24) | ((c[0]*c[3]/255) << 16) |
                    ((c[1]*c[3]/255) << 8) | (c[2]*c[3]/255);
    }
    uint8_t mask_bits[128] = {0};
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, mask_bits);
    ICONINFO icon = {0}; icon.xHotspot = 16; icon.yHotspot = 0;
    icon.hbmColor = color; icon.hbmMask = mask;
    HCURSOR cursor = mask ? (HCURSOR)CreateIconIndirect(&icon) : NULL;
    DeleteObject(mask); DeleteObject(color); return cursor;
}
void launcher_skin_highlight(const launcher_skin *s, HDC dc, RECT r, float amount)
{
    const mp_node *item = launcher_skin_widget(s, "Composite", "OptionItem");
    const mp_node *face = mp_find_named(item, "ColorFace", "bgf");
    double color[4] = {0};
    const mp_node *colors = mp_prop(face, "color");
    if (!colors || !colors->count || amount <= 0) return;
    for (int i=0; i<4; i++) color[i] = colors->items[0]->items[i]->number;
    uint32_t a = (unsigned)(color[3] * fminf(1, amount));
    uint32_t pixel = (a<<24) | ((unsigned)(color[0]*a/255)<<16) |
                     ((unsigned)(color[1]*a/255)<<8) | (unsigned)(color[2]*a/255);
    BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof info.bmiHeader;
    info.bmiHeader.biWidth=1; info.bmiHeader.biHeight=-1;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    uint32_t *bits;
    HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&bits,NULL,0);
    if (!bmp) return;
    *bits=pixel; HDC src=CreateCompatibleDC(dc); HBITMAP old=SelectObject(src,bmp);
    AlphaBlend(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,src,0,0,1,1,
               (BLENDFUNCTION){AC_SRC_OVER,0,255,AC_SRC_ALPHA});
    SelectObject(src,old); DeleteDC(src); DeleteObject(bmp);
}
int launcher_skin_paragraph(const launcher_skin *s,HDC dc,const mp_node *style,RECT r,const wchar_t *text)
{
    const char *name=mp_string(style,"font");
    int index=name&&!strcmp(name,"hb_24")?0:name&&!strcmp(name,"hc_20")?1:2;
    const gt4_font *font=&s->fonts[index];
    char utf8[4096];
    if (!WideCharToMultiByte(CP_UTF8,0,text,-1,utf8,sizeof utf8,NULL,NULL)) return 0;
    double sx=mp_number(style,"scale_x",1)*900/640,sy=mp_number(style,"scale_y",1)*700/480;
    double rgba[4]={0,0,0,255}; mp_numbers(style,"text_color",rgba,4);
    uint8_t color[4]; for(int i=0;i<4;i++) color[i]=(uint8_t)rgba[i];
    int height=(int)ceil(font->height*sy),lines=0,saved=dc?SaveDC(dc):0;
    if (height<=0) return 0;
    if (dc) IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);
    char *at=utf8;
    while (*at && r.top+height<=r.bottom) {
        char *end=at,*space=NULL;
        while (*end && *end!='\n') {
            if (*end==' ') space=end;
            char next=end[1]; end[1]=0;
            double width=gt4_font_original_width(font,at,(float)sx); end[1]=next;
            if (width>r.right-r.left) break;
            end++;
        }
        if (*end && *end!='\n' && space && space>at) end=space;
        if (end==at) end++;
        char held=*end; *end=0;
        if (dc) font_line(font,dc,at,r,sx,sy,0,color,0,0);
        *end=held; at=end;
        if (*at=='\n' || *at==' ') at++;
        r.top+=height; lines++;
    }
    if (dc) RestoreDC(dc,saved);
    return lines;
}

void launcher_skin_button(const launcher_skin *s,HDC dc,RECT r,float selected)
{
    /* Original: option.mproject Cancel.bg/bgf and FrameImageFace in ui.c;
       natural-size corners clamp to half the destination before stretching. */
    int w=(int)fmin(16.*900/640,(r.right-r.left)*.5),h=(int)fmin(16.*700/480,(r.bottom-r.top)*.5);
    RECT parts[6]={{r.left,r.top,r.left+w,r.top+h},{r.left,r.bottom-h,r.left+w,r.bottom},
        {r.left+w,r.top,r.right-w,r.top+h},{r.left+w,r.bottom-h,r.right-w,r.bottom},
        {r.right-w,r.top,r.right,r.top+h},{r.right-w,r.bottom-h,r.right,r.bottom}};
    for(int i=0;i<6;i++) {
        launcher_skin_image_draw(s,dc,13+i,parts[i]);
        if(selected>0) image_opacity(s,dc,19+i,parts[i],(unsigned)(255*fminf(1,selected)));
    }
}
void launcher_skin_field(const launcher_skin *s,HDC dc,RECT r,int selected)
{
    /* Original: option.mproject pulldown.bg, frame_width=21/frame_height=12. */
    int w=(int)fmin(21.*900/640,(r.right-r.left)*.5),h=(int)fmin(12.*700/480,(r.bottom-r.top)*.5);
    RECT parts[7]={{r.left,r.top,r.left+w,r.top+h},{r.left,r.bottom-h,r.left+w,r.bottom},
        {r.left+w,r.top,r.right-w,r.top+h},{r.left+w,r.bottom-h,r.right-w,r.bottom},
        {r.right-w,r.top,r.right,r.top+h},{r.right-w,r.bottom-h,r.right,r.bottom},
        {r.left+w,r.top+h,r.right-w,r.bottom-h}};
    for(int i=0;i<7;i++) launcher_skin_image_draw(s,dc,i==6?(selected?32:31):25+i,parts[i]);
    if (selected) launcher_skin_highlight(s,dc,r,1);
}
void launcher_skin_control_text(const launcher_skin *s,HDC dc,RECT r,const wchar_t *text,int center)
{
    char utf8[4096];
    if(!WideCharToMultiByte(CP_UTF8,0,text,-1,utf8,sizeof utf8,NULL,NULL)) return;
    const gt4_font *font=&s->fonts[2];
    double sx=900./640,sy=700./480;
    /* P: PC fields fit the disc font to the available control height/width. */
    double ratio=fmin(1,(r.bottom-r.top)/(font->height*sy)); sx*=ratio; sy*=ratio;
    double width=gt4_font_original_width(font,utf8,(float)sx);
    if(width>r.right-r.left) { ratio=(r.right-r.left)/width; sx*=ratio; sy*=ratio; }
    const uint8_t color[4]={0,0,0,255};
    int saved=SaveDC(dc); IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);
    font_line(font,dc,utf8,r,sx,sy,center?4:3,color,0,0); RestoreDC(dc,saved);
}
#endif
