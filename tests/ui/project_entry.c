#define main purchase_fixture_main
#define plat_draw_quad_blend fixture_draw_quad_blend
#include "../gtmode/purchase_menu.c"
#undef plat_draw_quad_blend
#undef main
static float black_alpha;
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                          float u,float v,float u1,float v1,const uint8_t c[4],float factor)
{
    (void)u;(void)v;(void)u1;(void)v1;
    if(!t && x==0 && y==0 && w==640 && h==480 && !c[0] && !c[1] && !c[2])
        black_alpha=factor;
}
static float opacity(menu *m)
{black_alpha=0;ui_draw_root(menu_ui(m),-1,NULL,NULL);return black_alpha;}
int main(int argc,char **argv)
{
    CHECK(argc>3);
    const char *err=NULL;
    gt4_vol *vol=gt4_vol_open(argv[1],&err);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    for(int i=2;i+1<argc;i+=2) {
        menu *m=menu_open(vol,argv[i],(char[256]){0},256);CHECK(m);
        int reveal=atoi(argv[i+1]);
        for(int cached=0;cached<2;cached++) {
            ui_fade(menu_ui(m),0,1,.5f); /* stale outgoing cached state */
            menu_project_entry(m);
            CHECK(fabsf(opacity(m)-(reveal?1.f:0))<1e-6f);
            ui_animate(menu_ui(m),-1,.5f); /* loading cannot consume reveal */
            CHECK(fabsf(opacity(m)-(reveal?1.f:0))<1e-6f);
            m->pending_kind=0;apply_pages(m); /* commit prepared destination */
            if(getenv("MOON_PROJECT_FADE_NEGATIVE") && !strcmp(argv[i],"logger"))
                ui_fade(menu_ui(m),0,0,0);
            CHECK(fabsf(opacity(m)-(reveal?1.f:0))<1e-6f);
            ui_animate(menu_ui(m),-1,7.f/60);
            CHECK(opacity(m)==(reveal?68.f/128:0)); /* GS alpha quantization */
            ui_animate(menu_ui(m),-1,9.f/60);
            CHECK(opacity(m)==0 && ui_fade_remaining(menu_ui(m))==0);
        }
        printf("PASS %s authored panIn=%d, fresh/cached black-mid-clear\n",argv[i],reveal);
        if(!strcmp(argv[i],"gtmode") || !strcmp(argv[i],"arcade")) {
            ah_obj *game=game_object(m);ah_val result;
            ah_set_member(m->vm,game,"major_menu_project",ah_str(m->vm,"arcade"));
            ah_set_member(m->vm,game,"next_sequence",ah_str(m->vm,"load_replay"));
            CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)==0);
            if(getenv("MOON_REPLAY_RETURN_NEGATIVE") && !strcmp(argv[i],"gtmode"))
                snprintf(memcard_origin,sizeof memcard_origin,"arcade");
            CHECK(!strcmp(m->next_project,"labomode"));
            CHECK(!strcmp(memcard_origin,argv[i]));
            menu *labo=menu_open(vol,"labomode",(char[256]){0},256);CHECK(labo);
            menu_set_origin(labo,argv[i]);menu_setting_context(labo,m);
            CHECK(menu_open_labo(labo,"ReplayTheaterRoot"));menu_tick(labo);
            ah_obj *theater=get(labo->vm,labo->project,"ReplayTheaterRoot").u.o;
            ah_val context=ah_objv(labo->context);
            CHECK(ah_call_member(labo->vm,theater,"exit",1,&context,&result)>=0);
            ticks(labo);
            printf("replay return %s -> %s\n",argv[i],labo->next_project);
            CHECK(!strcmp(labo->next_project,argv[i]));
            menu_close(labo);
        }
        menu_close(m);
    }
    gt4_vol_close(vol);gt4_save_shutdown();return 0;
}
