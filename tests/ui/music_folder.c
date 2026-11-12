#include "music_folder.h"
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"music folder line %d\n",__LINE__);return 1;} } while(0)
int main(int argc,char **argv) {
    CHECK(argc>=2);
    char config[1100],folder[1100],again[1100],file[1200];
    snprintf(config,sizeof config,"%s/profile-\xc3\xa7/nested/options.ini",argv[1]);
    CHECK(app_music_folder(config,folder,sizeof folder));
    snprintf(file,sizeof file,"%s/keep.wav",folder);
    const uint8_t bytes[]={1,2,3};
    CHECK(gt4_save_write(file,bytes,sizeof bytes));
    CHECK(app_music_folder(config,again,sizeof again));
    CHECK(!strcmp(folder,again));
    uint8_t *read=NULL;size_t size=0;uint64_t stamp=0;
    CHECK(gt4_save_read(file,&read,&size,&stamp)>0);
    CHECK(size==sizeof bytes && !memcmp(read,bytes,size));free(read);
    CHECK(!app_music_folder(config,again,2));
    CHECK(!app_music_folder("",again,sizeof again));
    snprintf(config,sizeof config,"%s/keep.wav/options.ini",folder);
    int blocked=app_music_folder(config,again,sizeof again);
    if(argc>2) CHECK(blocked); /* Negative: an existing file is not a directory. */
    else CHECK(!blocked);
    return 0;
}
