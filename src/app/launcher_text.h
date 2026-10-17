#ifndef MOON_LAUNCHER_TEXT_H
#define MOON_LAUNCHER_TEXT_H
#include <SDL.h>

/* Anti-aliased text for the launcher, rasterised from an embedded TrueType face
   at the size it is actually drawn. The engine cannot lend its own font: its
   glyphs are decoded from FT01 on the user's disc (src/core/font.h), and the
   launcher runs before a disc is chosen.
   Faces are opened once; each pixel size gets one cached glyph atlas. */
typedef struct lt_face lt_face;
typedef struct lt_font lt_font;

lt_face *lt_face_open(const unsigned char *ttf,int bytes);
lt_font *lt_font_size(lt_face *face,int px);

void lt_draw(SDL_Renderer *renderer,lt_font *font,int x,int y,const char *text,int tracking,SDL_Color colour);
int lt_width(lt_font *font,const char *text,int tracking);
int lt_line_height(lt_font *font);
int lt_ascent(lt_font *font);

void lt_close(lt_face *face);
#endif
