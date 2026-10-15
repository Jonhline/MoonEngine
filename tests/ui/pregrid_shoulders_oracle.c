/* Original root key fallback plus the disc's authored quick menu. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t pc);
static int script_consumed;
static void script_key(ee_ctx *c) { c->r[2].uw[0]=script_consumed; }
static ee_fn boundary(uint32_t pc) { return pc==0x263d28?script_key:NULL; }
static int original_key(ee_ctx *c,int keysym)
{
    const uint32_t event=0x1800000;
    memcpy(c->ram+event+0x20,&keysym,4);
    memset(c->r,0,sizeof c->r);
    c->r[4].uw[0]=0x1801000;
    c->r[5].uw[0]=0x1802000;
    c->r[6].uw[0]=event;
    c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump; c->exit_jmp=&jump;
    if(setjmp(jump)) { fprintf(stderr,"original key stopped: %s\n",c->end_detail); return -1; }
    c->lookup(0x235680)(c);
    return (int)c->r[2].uw[0];
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);
    ee_ctx c; CHECK(!ee_init(&c)); c.lookup=recomp_lookup; c.hook=boundary;
    FILE *file=fopen(argv[2],"rb"); CHECK(file);
    CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,file)>0x519300); fclose(file);
    const char *why=NULL; char error[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&why); CHECK(vol && gt4_game_load_disc(argv[1]));
    menu_quick_race q={0}; q.entries=1; q.player=0; q.laps=2;
    strcpy(q.course,"tsukuba"); strcpy(q.grid[0].car,"ka_01");
    menu_set_quick_race(&q);
    menu *m=menu_open(vol,"quick-arcade",error,sizeof error); CHECK(m);
    ah_set_log(m->vm,log_script,NULL);
    for(int completed=0;completed<2;completed++) {
        menu_set_quick_completed(m,completed); menu_enter(m,0); advance(m);
        ui_widget *buttons=child(ui_root_widget(m->ui,0),"Buttons");
        for(int i=0;i<buttons->nkids;i++) {
            ui_widget *button=buttons->kids[i]; if(!available(button)) continue;
            set_focus(m,button);
            for(int side=0;side<2;side++) {
                /* Original: func_00251EF0 +0x578..0x624, D_006192E4:
                   L1/R1 produce their distinct F11/F21 keys. */
                uint32_t mask,key;
                memcpy(&mask,c.ram+0x619300+side*8,4);
                memcpy(&key,c.ram+0x619304+side*8,4);
                CHECK(mask==(side?MENU_PAD_R1:MENU_PAD_L1));
                CHECK(key==(side?0xffd2:0xffc8));
                script_consumed=0;
                int original=original_key(&c,key);
                CHECK(original==0 && key_dispatch(m,key,0)==original);
                menu_send_key(m,key,0); menu_send_key(m,key,1); advance(m);
                if(argc>3) menu_send_input(m,MENU_RIGHT);
                CHECK(m->focus[0]==button);
                script_consumed=1;
                CHECK(original_key(&c,key)==1); /* Authored handlers take precedence. */
            }
        }
    }
    CHECK(!errors);
    menu_close(m); gt4_vol_close(vol); ee_free(&c);
    puts("PASS L1/R1 key table, original root fallback and pre-race focus before/after completion");
    return 0;
}
