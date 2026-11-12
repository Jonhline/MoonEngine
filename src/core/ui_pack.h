/* Linear-box allocation in axis order, func_002E99E0. */
#ifndef MOON_UI_PACK_H
#define MOON_UI_PACK_H

typedef struct {
    float front, back, cross, cross_size, extra, gap;
} ui_pack_cursor;
typedef struct {
    float main_size, cross_size;
    int expand_main, fill_main, expand_cross, fill_cross, reverse;
} ui_pack_item;
typedef struct {
    float main, cross, main_size, cross_size;
} ui_pack_rect;

static ui_pack_rect ui_pack_next(ui_pack_cursor *c, ui_pack_item v)
{
    float slot = v.main_size, offset = 0, cross_offset = 0;
    if (v.expand_main) {
        slot += c->extra;
        if (v.fill_main)
            v.main_size += c->extra;
        else
            offset = c->extra * .5f;
    }
    if (v.expand_cross) {
        if (v.fill_cross)
            v.cross_size = c->cross_size;
        else
            cross_offset = (c->cross_size - v.cross_size) * .5f;
    }
    ui_pack_rect r = {0, c->cross + cross_offset, v.main_size, v.cross_size};
    if (v.reverse) {
        r.main = c->back - slot + offset;
        c->back -= slot + c->gap;
    } else {
        r.main = c->front + offset;
        c->front += slot + c->gap;
    }
    return r;
}
#endif
