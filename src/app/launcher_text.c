/* Anti-aliased launcher text: one glyph atlas per (face, pixel size), drawn as
   tinted textured quads through SDL. Rasterised with stb_truetype (public
   domain). The launcher is single-threaded, so no locking is needed. */
#define STB_TRUETYPE_IMPLEMENTATION
#include "launcher_text.h"
#include "../../third_party/stb/stb_truetype.h"
#include <stdlib.h>
#include <string.h>

#define LT_FIRST 32
#define LT_LAST 126
#define LT_GLYPHS (LT_LAST-LT_FIRST+1)
#define LT_COLUMNS 12

typedef struct { int x,y,w,h,xoff,yoff,adv; } lt_glyph;

struct lt_font {
    stbtt_fontinfo info;
    float scale;
    int px,ascent,descent,line;
    lt_glyph glyphs[LT_GLYPHS];
    unsigned char *coverage;      /* atlas pixels, white + coverage in alpha */
    SDL_Texture *atlas;           /* created on the first draw: needs the renderer */
    int atlas_w,atlas_h;
};
struct lt_face {
    unsigned char *data;
    struct lt_font *sizes[8];
    int count;
};

lt_face *lt_face_open(const unsigned char *ttf,int bytes)
{
    if(!ttf || bytes<=0)return NULL;
    lt_face *face=calloc(1,sizeof *face);
    if(!face)return NULL;
    face->data=malloc((size_t)bytes);
    if(!face->data){free(face);return NULL;}
    memcpy(face->data,ttf,(size_t)bytes);
    stbtt_fontinfo probe;
    if(stbtt_GetFontOffsetForIndex(face->data,0)<0 ||
       !stbtt_InitFont(&probe,face->data,stbtt_GetFontOffsetForIndex(face->data,0))){
        free(face->data);free(face);return NULL;
    }
    return face;
}

static struct lt_font *build(lt_face *face,int px)
{
    struct lt_font *font=calloc(1,sizeof *font);
    if(!font)return NULL;
    int offset=stbtt_GetFontOffsetForIndex(face->data,0);
    if(!stbtt_InitFont(&font->info,face->data,offset)){free(font);return NULL;}
    font->px=px;
    font->scale=stbtt_ScaleForPixelHeight(&font->info,(float)px);
    int a=0,d=0,gap=0;stbtt_GetFontVMetrics(&font->info,&a,&d,&gap);
    font->ascent=(int)(a*font->scale);font->descent=(int)(-d*font->scale);
    font->line=font->ascent+font->descent;
    int cell_w=0,cell_h=0;
    for(int i=0;i<LT_GLYPHS;i++){
        int w=0,h=0,xo=0,yo=0;
        unsigned char *bitmap=stbtt_GetCodepointBitmap(&font->info,font->scale,font->scale,
                                                       LT_FIRST+i,&w,&h,&xo,&yo);
        if(bitmap)stbtt_FreeBitmap(bitmap,NULL);
        int adv=0,lsb=0;stbtt_GetCodepointHMetrics(&font->info,LT_FIRST+i,&adv,&lsb);
        font->glyphs[i].w=w;font->glyphs[i].h=h;
        font->glyphs[i].xoff=xo;font->glyphs[i].yoff=yo;
        font->glyphs[i].adv=(int)(adv*font->scale);
        if(w>cell_w)cell_w=w;if(h>cell_h)cell_h=h;
    }
    cell_w+=2;cell_h+=2;
    int rows=(LT_GLYPHS+LT_COLUMNS-1)/LT_COLUMNS;
    font->atlas_w=cell_w*LT_COLUMNS;font->atlas_h=cell_h*rows;
    font->coverage=calloc((size_t)font->atlas_w*font->atlas_h*4,1);
    if(!font->coverage){free(font);return NULL;}
    for(int i=0;i<LT_GLYPHS;i++){
        int w=0,h=0,xo=0,yo=0;
        unsigned char *bitmap=stbtt_GetCodepointBitmap(&font->info,font->scale,font->scale,
                                                       LT_FIRST+i,&w,&h,&xo,&yo);
        font->glyphs[i].x=(i%LT_COLUMNS)*cell_w+1;
        font->glyphs[i].y=(i/LT_COLUMNS)*cell_h+1;
        if(!bitmap)continue;
        for(int gy=0;gy<h;gy++)for(int gx=0;gx<w;gx++){
            unsigned char cov=bitmap[gy*w+gx];
            if(!cov)continue;
            size_t at=((size_t)(font->glyphs[i].y+gy)*font->atlas_w+font->glyphs[i].x+gx)*4;
            font->coverage[at+0]=255;font->coverage[at+1]=255;font->coverage[at+2]=255;
            font->coverage[at+3]=cov;
        }
        stbtt_FreeBitmap(bitmap,NULL);
    }
    return font;
}

lt_font *lt_font_size(lt_face *face,int px)
{
    if(!face || px<6)return NULL;
    for(int i=0;i<face->count;i++)if(face->sizes[i]->px==px)return face->sizes[i];
    if(face->count>=8)return face->sizes[0];
    struct lt_font *font=build(face,px);
    if(!font)return face->count?face->sizes[0]:NULL;
    face->sizes[face->count++]=font;
    return font;
}

int lt_line_height(lt_font *font){return font?font->line:0;}
int lt_ascent(lt_font *font){return font?font->ascent:0;}

int lt_width(lt_font *font,const char *text,int tracking)
{
    if(!font || !text)return 0;
    int total=0;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++){
        int idx=(*p<LT_FIRST || *p>LT_LAST)?('?'-LT_FIRST):(*p-LT_FIRST);
        total+=font->glyphs[idx].adv+tracking;
    }
    return total;
}

void lt_draw(SDL_Renderer *renderer,lt_font *font,int x,int y,const char *text,int tracking,SDL_Color colour)
{
    if(!renderer || !font || !text)return;
    if(!font->atlas){
        font->atlas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,
                                      font->atlas_w,font->atlas_h);
        if(!font->atlas)return;
        SDL_UpdateTexture(font->atlas,NULL,font->coverage,font->atlas_w*4);
        SDL_SetTextureBlendMode(font->atlas,SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureColorMod(font->atlas,colour.r,colour.g,colour.b);
    SDL_SetTextureAlphaMod(font->atlas,colour.a);
    int cx=x;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++){
        int idx=(*p<LT_FIRST || *p>LT_LAST)?('?'-LT_FIRST):(*p-LT_FIRST);
        lt_glyph *glyph=&font->glyphs[idx];
        if(glyph->w && glyph->h){
            SDL_Rect src={glyph->x,glyph->y,glyph->w,glyph->h};
            SDL_Rect dst={cx+glyph->xoff,y+font->ascent+glyph->yoff,glyph->w,glyph->h};
            SDL_RenderCopy(renderer,font->atlas,&src,&dst);
        }
        cx+=glyph->adv+tracking;
    }
}

void lt_close(lt_face *face)
{
    if(!face)return;
    for(int i=0;i<face->count;i++){
        if(face->sizes[i]->atlas)SDL_DestroyTexture(face->sizes[i]->atlas);
        free(face->sizes[i]->coverage);
        free(face->sizes[i]);
    }
    free(face->data);free(face);
}
