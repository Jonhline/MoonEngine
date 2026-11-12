/* Complete-package interchange uses generated data only, in isolated folders. */
#include "save_package.h"
#include "replay_file.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#define PID _getpid()
#else
#include <unistd.h>
#define PID getpid()
#endif
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"save package %d: %s (%s)\n",__LINE__,#x,report.error);return 1;}} while(0)
static int raw_write(const char *path,const void *bytes,size_t size)
{
    FILE *f=fopen(path,"wb");if (!f) return 0;
    int ok=fwrite(bytes,1,size,f)==size;return !fclose(f) && ok;
}
static void *raw_read(const char *path,size_t *size)
{
    FILE *f=fopen(path,"rb");if (!f) return NULL;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    void *p=n>=0 ? malloc((size_t)n+1) : NULL;
    if (!p || fread(p,1,(size_t)n,f)!=(size_t)n) {free(p);p=NULL;}
    fclose(f);if (p) *size=(size_t)n;return p;
}
static int equal_file(const char *a,const char *b)
{
    size_t x=0,y=0;void *aa=raw_read(a,&x), *bb=raw_read(b,&y);
    int ok=aa && bb && x==y && !memcmp(aa,bb,x);free(aa);free(bb);return ok;
}
static int failing_writer(const char *path,const uint8_t *data,size_t size,void *user)
{
    (void)path;(void)data;(void)size;(void)user;return 0;
}
static int fail_activation(const char *path,const uint8_t *data,size_t size,void *user)
{
    int *calls=user;
    return ++*calls==1 ? gt4_save_write(path,data,size) : 0;
}
static int evil_zip(const char *output,const char *profile,const char *name,int duplicate,int version)
{
    mz_zip_archive zip={0};
    const char *header=version ? "MoonEnginePlayerPackage=9\nprofile=profile.ogt4\nlibrary=shared\n"
                               : "MoonEnginePlayerPackage=1\nprofile=profile.ogt4\nlibrary=shared\n";
    int ok=mz_zip_writer_init_file(&zip,output,0) &&
           mz_zip_writer_add_mem(&zip,"manifest.ini",header,strlen(header),0) &&
           mz_zip_writer_add_file(&zip,"profile.ogt4",profile,NULL,0,0) &&
           mz_zip_writer_add_mem(&zip,name,"test",4,0);
    if (ok && duplicate) ok=mz_zip_writer_add_mem(&zip,"library/photos/FILE.png","other",5,0);
    if (ok) ok=mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);return ok;
}
static int corrupt_member(const char *path)
{
    mz_zip_archive zip={0};mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_init_file(&zip,path,0)) return 0;
    int index=mz_zip_reader_locate_file(&zip,"library/photos/valid.png",NULL,0);
    int ok=index>=0 && mz_zip_reader_file_stat(&zip,(unsigned)index,&stat);
    mz_zip_reader_end(&zip);if (!ok) return 0;
    FILE *f=fopen(path,"r+b");if (!f) return 0;
    unsigned char header[30];
    ok=!fseek(f,(long)stat.m_local_header_ofs,SEEK_SET) && fread(header,1,30,f)==30;
    long offset=(long)stat.m_local_header_ofs+30+header[26]+((unsigned)header[27]<<8)+header[28]+((unsigned)header[29]<<8);
    if (ok) ok=!fseek(f,offset,SEEK_SET);
    int value=ok ? fgetc(f) : EOF;
    if (ok) ok=value!=EOF && !fseek(f,offset,SEEK_SET) && fputc(value^1,f)!=EOF;
    fclose(f);return ok;
}
int main(int argc,char **argv)
{
    moon_save_package_report report={0};CHECK(argc>=2);
    char root[1000], source[1100], destination[1100], package[1100], file[1400];
    snprintf(root,sizeof root,"%s/%llu-%d",argv[1],(unsigned long long)time(NULL),PID);
    snprintf(source,sizeof source,"%s/source",root);snprintf(destination,sizeof destination,"%s/destination",root);
    snprintf(package,sizeof package,"%s/Complete.moonplayer",root);
    gt4_save_init(source);CHECK(gt4_save_create(1,"Complete player"));
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",NULL,NULL,NULL);
    gt4_save_restore(vm,game);ah_set_member(vm,game,"money",ah_int(987654));
    CHECK(gt4_save_capture(vm,game,1) && gt4_save_store());ah_vm_free(vm);
    snprintf(file,sizeof file,"%s/photos",source);CHECK(gt4_save_mkdir(file));
    uint8_t pixels[16]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};size_t png_size;
    void *png=tdefl_write_image_to_png_file_in_memory(pixels,2,2,4,&png_size);CHECK(png);
    snprintf(file,sizeof file,"%s/photos/photo.png",source);CHECK(raw_write(file,png,png_size));free(png);
    char photo_list[64]={0};snprintf(photo_list,sizeof photo_list,"photo.png");
    snprintf(file,sizeof file,"%s/photos/slideshow.list",source);CHECK(raw_write(file,photo_list,sizeof photo_list));
    snprintf(file,sizeof file,"%s/photos/previews",source);CHECK(gt4_save_mkdir(file));
    snprintf(file,sizeof file,"%s/photos/previews/photo.cache",source);CHECK(raw_write(file,"auxiliary",9));
    snprintf(file,sizeof file,"%s/films",source);CHECK(gt4_save_mkdir(file));
    snprintf(file,sizeof file,"%s/films/scene.ogt4film",source);CHECK(gt4_save_write(file,(uint8_t *)"film-stream-fixture",19));
    snprintf(file,sizeof file,"%s/imports/legacy",source);CHECK(gt4_save_mkdir(file));
    snprintf(file,sizeof file,"%s/imports/legacy/GAMEDATA",source);CHECK(raw_write(file,"legacy fixture",14));
    gt4_replay_file replay={0};uint8_t command[32]={0};replay.tape=command;replay.steps=1;replay.laps=1;
    snprintf(replay.course,sizeof replay.course,"tsukuba");snprintf(replay.car,sizeof replay.car,"ka_01");
    char replay_name[64];CHECK(gt4_replay_store(&replay,replay_name,sizeof replay_name));
    char ghost_name[64];replay.kind=GT4_REPLAY_BEST_LAP;
    CHECK(gt4_replay_store(&replay,ghost_name,sizeof ghost_name));
    char replay_list[1][64];snprintf(replay_list[0],64,"%s",replay_name);CHECK(gt4_replay_playlist_write(replay_list,1));
    CHECK(gt4_save_create(2,"Other player") && gt4_save_load_slot(1));
    gt4_save_mark_dirty();CHECK(moon_save_package_export(1,package,NULL,NULL,&report));
    CHECK(gt4_save_active()==1 && gt4_save_dirty() && report.files==9);
    snprintf(file,sizeof file,"%s/photos/unsafe.moonplayer",source);
    CHECK(!moon_save_package_export(1,file,NULL,NULL,&report));
    /* A different library/installation receives all files and the profile. */
    gt4_save_init(destination);CHECK(gt4_save_create(1,"Existing player"));
    CHECK(moon_save_package_import(2,package,NULL,NULL,NULL,&report));
    CHECK(gt4_save_active()==2 && !strcmp(gt4_save_player(),"Complete player"));
    char original_profile[1400],imported_profile[1400];
    snprintf(original_profile,sizeof original_profile,"%s/slot01.ogt4",source);
    snprintf(imported_profile,sizeof imported_profile,"%s/slot02.ogt4",destination);
    uint8_t *original_payload=NULL,*imported_payload=NULL;size_t original_size=0,imported_size=0;
    CHECK(gt4_save_read(original_profile,&original_payload,&original_size,NULL)>0);
    CHECK(gt4_save_read(imported_profile,&imported_payload,&imported_size,NULL)>0);
    CHECK(original_size==imported_size && !memcmp(original_payload,imported_payload,original_size));
    free(original_payload);free(imported_payload);
    vm=ah_vm_new();game=ah_module_new(vm,"game",NULL,NULL,NULL);gt4_save_restore(vm,game);
    ah_val money;CHECK(ah_get_member(vm,game,"money",&money) && ah_num(money)==987654);ah_vm_free(vm);
    const char *media[]={"photos/photo.png","photos/slideshow.list","photos/previews/photo.cache","films/scene.ogt4film","imports/legacy/GAMEDATA","replays/playlist.ogt4l"};
    for (unsigned i=0;i<sizeof media/sizeof *media;i++) {
        char a[1400],b[1400];snprintf(a,sizeof a,"%s/%s",source,media[i]);snprintf(b,sizeof b,"%s/%s",destination,media[i]);
        if (argc>2 && !strcmp(argv[2],"--negative") && !i) CHECK(raw_write(b,"broken",6));
        CHECK(equal_file(a,b));
    }
    gt4_replay_file restored;CHECK(gt4_replay_load(replay_name,&restored));CHECK(restored.steps==1 && !memcmp(restored.tape,command,32));gt4_replay_free(&restored);
    CHECK(gt4_replay_load(ghost_name,&restored));CHECK(restored.kind==GT4_REPLAY_BEST_LAP && restored.steps==1);gt4_replay_free(&restored);
    char listed[2][64];CHECK(gt4_replay_playlist_read(listed,2)==1 && !strcmp(listed[0],replay_name));
    CHECK(moon_save_package_import(3,package,NULL,NULL,NULL,&report)); /* identical shared media */
    CHECK(!moon_save_package_import(3,package,NULL,NULL,NULL,&report)); /* occupied */
    snprintf(file,sizeof file,"%s/photos/photo.png",destination);CHECK(raw_write(file,"keep local",10));
    CHECK(!moon_save_package_import(4,package,NULL,NULL,NULL,&report));
    CHECK(strstr(report.error,"conflict") && gt4_save_active()==3);
    size_t retained_size=0;void *retained=raw_read(file,&retained_size);CHECK(retained && retained_size==10 && !memcmp(retained,"keep local",10));free(retained);
    gt4_save_info info;CHECK(gt4_save_info_get(4,&info)==0);
    /* Failure after media promotion rolls back only this import's new files. */
    char failing[1100];snprintf(failing,sizeof failing,"%s/failing",root);gt4_save_init(failing);CHECK(gt4_save_create(1,"Keep"));
    CHECK(!moon_save_package_import(2,package,failing_writer,NULL,NULL,&report));
    CHECK(gt4_save_active()==1 && gt4_save_info_get(2,&info)==0);
    int writes=0;
    CHECK(!moon_save_package_import(2,package,fail_activation,NULL,&writes,&report));
    CHECK(writes==2 && gt4_save_active()==1 && gt4_save_info_get(2,&info)==0);
    snprintf(file,sizeof file,"%s/photos/photo.png",failing);FILE *absent=fopen(file,"rb");CHECK(!absent);
    /* Unsupported versions, escaping/alias paths and invalid profile payloads. */
    char invalid[1100],profile[1400];snprintf(invalid,sizeof invalid,"%s/invalid.moonplayer",root);snprintf(profile,sizeof profile,"%s/slot01.ogt4",source);
    const char *bad[]={"library/photos/../../escape","library/photos/con.txt","library/photos/a:b","library/photos/file.png"};
    for (unsigned i=0;i<4;i++) {
        CHECK(evil_zip(invalid,profile,bad[i],i==3,0));
        CHECK(!moon_save_package_import(2,invalid,NULL,NULL,NULL,&report));CHECK(gt4_save_active()==1 && gt4_save_info_get(2,&info)==0);
    }
    CHECK(evil_zip(invalid,profile,"library/photos/valid.png",0,0));CHECK(corrupt_member(invalid));
    CHECK(!moon_save_package_import(2,invalid,NULL,NULL,NULL,&report));
    CHECK(gt4_save_active()==1 && gt4_save_info_get(2,&info)==0);
    CHECK(evil_zip(invalid,profile,"library/photos/valid.png",0,1));CHECK(!moon_save_package_import(2,invalid,NULL,NULL,NULL,&report));
    snprintf(file,sizeof file,"%s/bad-profile.ogt4",root);CHECK(raw_write(file,"invalid",7));
    CHECK(evil_zip(invalid,file,"library/photos/valid.png",0,0));CHECK(!moon_save_package_import(2,invalid,NULL,NULL,NULL,&report));
    gt4_save_shutdown();
    puts("Complete package: profile, PNG, replay playback data, film bytes, playlists, auxiliary/legacy files, collisions, rollback and unsafe archives passed");
    return 0;
}
