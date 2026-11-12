/* P tooling integration: original authored profile and native car/license getters. */
#define main unused_pregrid_main
#include "pregrid_menu.c"
#undef main
#include "career_vehicle.h"
#include "career_setting.h"
#include "career_parts.h"
static ah_val saved(ah_vm *vm,ah_obj *o,const char *key){ah_val v=ah_nil();ah_get_member(vm,o,key,&v);return v;}
int main(int argc,char **argv)
{
    CHECK(argc>=4);const char *iso=argv[1],*mode=argv[2],*file=argv[3];const char *error=NULL;char msg[256],dir[1200];
    snprintf(dir,sizeof dir,"%s.session",file);gt4_save_init(dir);
    gt4_vol *vol=gt4_vol_open(iso,&error);CHECK(vol&&gt4_game_load_disc(iso));
    menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);ah_vm *vm=m->vm;ah_obj *game=game_object(m);
    if(!strcmp(mode,"create")){
        ah_set_member(vm,game,"username",ah_str(vm,"Friendly editor"));
        gt4_career_vehicle *factory=gt4_career_vehicle_open(iso);CHECK(factory);
        gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
        ah_obj *garage=saved(vm,game,"garage").u.o;ah_val args[]={ah_str(vm,"ka_01"),ah_int(0)},ret,index=ah_int(0);
        CHECK(ah_call_member(vm,garage,"addNewCar",2,args,&ret)>=0);
        CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)>=0);
        /* Force the same cached and indexed record paths that a played save has. */
        ah_obj *race=saved(vm,game,"race_record").u.o;ah_val code=ah_str(vm,"l0b0001");
        CHECK(ah_call_member(vm,race,"getUnit",1,&code,&ret)>=0);
        uint8_t *data=NULL;size_t size=0;CHECK(gt4_state_pack(vm,game,&data,&size));CHECK(gt4_save_write(file,data,size));free(data);
        gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);
    }else{
        uint8_t *data=NULL;size_t size=0;uint64_t stamp;CHECK(gt4_save_read(file,&data,&size,&stamp)==1);CHECK(gt4_state_replace(vm,game,data,size));free(data);gt4_game_bind(vm,game);
        if(!strcmp(mode,"verify-cappuccino")){
            gt4_career_vehicle *factory=gt4_career_vehicle_open(iso);CHECK(factory);
            gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
            CHECK(gt4_save_capture(vm,game,1));
            menu_save_refresh(m); /* Same restore/bind boundary used by the game. */
            game=game_object(m);
            ah_obj *garage=saved(vm,game,"garage").u.o;
            int index=(int)ah_num(saved(vm,garage,"riding_index"));CHECK(index==1);
            ah_obj *car=ah_array_get(saved(vm,garage,"cars").u.o,index).u.o;
            const char *label=ah_cstr(saved(vm,car,"car_code"));CHECK(label);
            CHECK(!strcmp(label,"cappuccino_91")||!strcmp(label,"cappuccino_95"));
            int mass=!strcmp(label,"cappuccino_91")?7:6;
            uint8_t body[0x4b8],current[0x4b8];int32_t stats[3];ah_val ret;
            CHECK(gt4_game_car_body(vm,car,body)&&menu_riding_car_body(m,current));
            CHECK(!memcmp(body,current,sizeof body));
            CHECK(gt4_career_vehicle_stats(factory,current,stats));
            CHECK(stats[0]>15900&&stats[0]<16100&&stats[2]==mass);
            CHECK(ah_call_member(vm,car,"getCurrentPower",0,NULL,&ret)>=0&&ah_num(ret)==160);
            CHECK(ah_call_member(vm,car,"getCurrentWeight",0,NULL,&ret)>=0&&ah_num(ret)==mass);
            if(argc>4)CHECK(stats[2]==1); /* WILL_FAIL: stock/incorrect mass cannot pass. */
            gt4_game_set_stats_reader(NULL,NULL);gt4_career_vehicle_close(factory);
            menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();return 0;
        }
        ah_obj *race=saved(vm,game,"race_record").u.o;ah_val code,ret;
        const char *groups[]={"l0b","l0a","lib","lia","l0s"};
        for(int group=0;group<5;group++){
        code=ah_str(vm,groups[group]);CHECK(ah_call_member(vm,race,"isLicensePassed",1,&code,&ret)>=0&&ah_truthy(ret));
        for(int i=1;i<=16;i++){
            char label[16];snprintf(label,sizeof label,"%s%04d",groups[group],i);code=ah_str(vm,label);ah_val unit,result;
            CHECK(ah_call_member(vm,race,"getUnit",1,&code,&unit)>=0);
            result=saved(vm,unit.u.o,"result");CHECK(ah_cstr(result)&&!strcmp(ah_cstr(result),"gold"));
            CHECK(gt4_game_license_best(vm,game,label)>0&&gt4_game_license_best(vm,game,label)<0x157529ffu);
        }
        }
        ah_obj *garage=saved(vm,game,"garage").u.o,*cars=saved(vm,garage,"cars").u.o;
        ah_obj *car=ah_array_get(cars,0).u.o;uint8_t body[0x4b8],current[0x4b8],unit[32];
        CHECK(gt4_game_car_body(vm,car,body)&&gt4_game_car_unit(vm,car,unit));
        CHECK(gt4_career_blob_get(vm,garage,"current_car_ps2",current,sizeof current)&&!memcmp(body,current,sizeof body));
        gt4_game_bind_car(vm,car);CHECK(ah_call_member(vm,car,"getOdometer",0,NULL,&ret)>=0&&ret.u.i==1234567);
        for(int i=0;i<3;i++)CHECK(body[i*0x178+0x18]==7);
        CHECK(((unit[8]|(unsigned)unit[9]<<8)>>2&127)==7);
        int32_t tcs;CHECK(gt4_setting_get(body,GT4_SETTING_TCSC,0,&tcs));
        CHECK(tcs==(argc>4&&!strcmp(argv[4],"--negative")?8:7));
        CHECK(saved(vm,car,"color_index").u.i==7&&saved(vm,car,"odometer").u.i==1234567);
    }
    menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();return 0;
}
