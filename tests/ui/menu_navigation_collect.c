/* Actual 0026F9E0 recursion and clip stack; widget getters are the boundary.
   The fixture supplies widget geometry/flags, not navigation decisions. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#include "ee.h"

ee_fn recomp_lookup(uint32_t pc);
enum { NODE_COUNT = 40, NODE_BASE = 0x1800000, NODE_SIZE = 0x200 };
static ui_widget nodes[NODE_COUNT];
static ui_widget *children[NODE_COUNT][NODE_COUNT];
static int focusable[NODE_COUNT], clipped[NODE_COUNT];
static uint32_t random_state = 267018;
static unsigned next_word(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static int node_index(uint32_t address)
{
    CHECK(address >= NODE_BASE && address < NODE_BASE + NODE_COUNT * NODE_SIZE);
    CHECK((address - NODE_BASE) % NODE_SIZE == 0);
    return (address - NODE_BASE) / NODE_SIZE;
}
static uint32_t address(const ui_widget *w)
{
    return w ? NODE_BASE + (uint32_t)(w - nodes) * NODE_SIZE : 0;
}
static void can_focus(ee_ctx *c)
{
    int i = node_index(c->r[4].uw[0]);
    c->r[2].uw[0] = focusable[i] && nodes[i].visible && !nodes[i].insensitive;
}
static void visible(ee_ctx *c)
{
    c->r[2].uw[0] = nodes[node_index(c->r[4].uw[0])].visible;
}
static void insensitive(ee_ctx *c)
{
    c->r[2].uw[0] = nodes[node_index(c->r[4].uw[0])].insensitive;
}
static void put_float(ee_ctx *c, uint32_t at, float value)
{
    memcpy(c->ram + at, &value, sizeof value);
}
static void size_get(ee_ctx *c)
{
    ui_widget *w = nodes + node_index(c->r[4].uw[0]);
    put_float(c, c->r[5].uw[0], w->w);
    put_float(c, c->r[6].uw[0], w->h);
}
static void region_get(ee_ctx *c)
{
    ui_widget *w = nodes + node_index(c->r[5].uw[0]);
    const float r[4] = {0, 0, w->w, w->h};
    memcpy(c->ram + c->r[4].uw[0], r, sizeof r);
}
static void child_count(ee_ctx *c)
{
    c->r[2].uw[0] = nodes[node_index(c->r[4].uw[0])].nkids;
}
static void child_first(ee_ctx *c)
{
    ui_widget *w = nodes + node_index(c->r[4].uw[0]);
    c->r[2].uw[0] = w->nkids ? address(w->kids[0]) : 0;
}
static void child_next(ee_ctx *c)
{
    ui_widget *w = nodes + node_index(c->r[4].uw[0]), *p = w->parent;
    c->r[2].uw[0] = 0;
    if (p)
        for (int i = 0; i + 1 < p->nkids; i++)
            if (p->kids[i] == w)
                c->r[2].uw[0] = address(p->kids[i + 1]);
}
static void x_get(ee_ctx *c)
{
    c->fpr[0].f = nodes[node_index(c->r[4].uw[0])].x;
}
static void y_get(ee_ctx *c)
{
    c->fpr[0].f = nodes[node_index(c->r[4].uw[0])].y;
}
static void clip_get(ee_ctx *c)
{
    c->r[2].uw[0] = clipped[node_index(c->r[4].uw[0])];
}
static void descend_get(ee_ctx *c)
{
    c->r[2].uw[0] = 1;
}
static ee_fn boundary(uint32_t pc)
{
    switch (pc) {
    case 0x265ea8:
        return can_focus;
    case 0x25b498:
        return size_get;
    case 0x267f70:
    case 0x268228:
        return region_get;
    case 0x265dc8:
        return visible;
    case 0x2661c8:
        return insensitive;
    case 0x206880:
        return child_count;
    case 0x206868:
        return child_first;
    case 0x25c300:
        return child_next;
    case 0x25b2b0:
        return x_get;
    case 0x25b310:
        return y_get;
    case 0x100:
        return clip_get;
    case 0x108:
        return descend_get;
    default:
        return NULL;
    }
}
static ee_fn lookup(uint32_t pc)
{
    ee_fn f = boundary(pc);
    return f ? f : recomp_lookup(pc);
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    CHECK(ee_load_image(&c, argv[1], 0x100000) > 0);
    c.hook = boundary;
    c.lookup = lookup;
    mp_node *properties[2][2];
    char text[128], error[128];
    for (int focus = 0; focus < 2; focus++)
        for (int clip = 0; clip < 2; clip++) {
            snprintf(text, sizeof text, "Composite { can_focus digit {%d} clip digit {%d} }", focus,
                     clip);
            properties[focus][clip] = mp_parse(text, strlen(text), error, sizeof error);
            CHECK(properties[focus][clip]);
        }
    const uint32_t nav = 0x1900000, vector = nav + 0x100, data = nav + 0x200;
    const uint32_t stack = nav + 0x2000, vtable = nav + 0x3000;
    ee_w32(&c, vtable + 0x1ac, 0x100);
    ee_w32(&c, vtable + 0x1b4, 0x108);
    for (unsigned sample = 0; sample < 2000; sample++) {
        memset(nodes, 0, sizeof nodes);
        for (int i = 0; i < NODE_COUNT; i++) {
            ui_widget *w = nodes + i;
            focusable[i] = i && next_word() % 3 == 0;
            clipped[i] = next_word() % 3 == 0;
            w->src = properties[focusable[i]][clipped[i]];
            w->type = "Composite";
            w->visible = i == 0 || next_word() % 8 != 0;
            w->insensitive = i && next_word() % 8 == 0;
            w->x = (int)(next_word() % 160) - 40;
            w->y = (int)(next_word() % 140) - 30;
            w->w = next_word() % 120;
            w->h = next_word() % 100;
            w->kids = children[i];
            if (i) {
                ui_widget *p = nodes + next_word() % i;
                w->parent = p;
                p->kids[p->nkids++] = w;
            }
            ee_w32(&c, address(w) + 4, vtable);
        }
        ui_widget *owner = sample % 3 == 0 ? NULL : nodes + next_word() % NODE_COUNT;
        ui_widget *widgets[512];
        menu_nav_item actual[512];
        int count = 0;
        menu_input input = MENU_UP + sample % 4;
        navigation_collect(nodes, owner, NULL, 0, input, widgets, actual, &count, 0);
        memset(c.ram + nav, 0, 0x100);
        ee_w32(&c, nav + 0x3c, address(owner));
        ee_w32(&c, nav + 0x44, stack);
        ee_w32(&c, nav + 0x48, stack);
        ee_w32(&c, nav + 0x4c, stack + 0x1000);
        put_float(&c, nav + 4, input == MENU_LEFT ? -1 : input == MENU_RIGHT ? 1 : 0);
        put_float(&c, nav + 8, input == MENU_UP ? -1 : input == MENU_DOWN ? 1 : 0);
        ee_w32(&c, vector + 4, data);
        ee_w32(&c, vector + 8, data);
        ee_w32(&c, vector + 12, data + 44 * NODE_COUNT);
        c.r[29].uw[0] = 0x1ef0000;
        c.r[4].uw[0] = nav;
        c.r[5].uw[0] = vector;
        c.r[6].uw[0] = address(nodes);
        c.r[7].uw[0] = 0;
        c.fpr[12].f = nodes[0].x;
        c.fpr[13].f = nodes[0].y;
        recomp_lookup(0x26f9e0)(&c);
        CHECK(count == (int)(ee_r32(&c, vector + 8) - data) / 44);
        if (argc > 2 && count)
            actual[0].group++; /* Mutation: lose the original group's priority. */
        for (int i = 0; i < count; i++) {
            uint32_t at = data + i * 44;
            CHECK(address(widgets[i]) == ee_r32(&c, at));
            CHECK(actual[i].group == (int)ee_r32(&c, at + 4));
            CHECK(!memcmp(&actual[i].source, c.ram + at + 8, sizeof(menu_nav_rect)));
            CHECK(!memcmp(&actual[i].target, c.ram + at + 24, sizeof(menu_nav_rect)));
        }
    }
    for (int f = 0; f < 2; f++)
        for (int k = 0; k < 2; k++)
            mp_free(properties[f][k]);
    ee_free(&c);
    puts("PASS 2000 widget trees against original 0026F9E0 collection and clipping");
    return 0;
}
