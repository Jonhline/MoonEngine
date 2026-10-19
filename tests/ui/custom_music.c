#include "music_library.h"
#include "music_override.h"
#include "pcm_music.h"
#include <SDL.h>
#undef main
#include "../test_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "custom music line %d: %s\n", __LINE__, #x);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static FILE *file(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t p[2048], m[16];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, p, 2048) ||
        !MultiByteToWideChar(CP_UTF8, 0, mode, -1, m, 16))
        return NULL;
    return _wfopen(p, m);
#else
    return fopen(path, mode);
#endif
}
static void word(FILE *f, unsigned x, int n)
{
    for (int i = 0; i < n; i++)
        fputc((x >> (8 * i)) & 255, f);
}
static int wave(const char *path, int constant)
{
    FILE *f = file(path, "wb");
    if (!f)
        return 0;
    fwrite("RIFF", 1, 4, f);
    word(f, 36 + 96000, 4);
    fwrite("WAVEfmt ", 1, 8, f);
    word(f, 16, 4);
    word(f, 1, 2);
    word(f, 2, 2);
    word(f, 48000, 4);
    word(f, 192000, 4);
    word(f, 4, 2);
    word(f, 16, 2);
    fwrite("data", 1, 4, f);
    word(f, 96000, 4);
    for (int i = 0; i < 24000; i++) {
        unsigned v = constant ? 2300 : 1000 + (unsigned)i % 12000;
        word(f, v, 2);
        word(f, v, 2);
    }
    return fclose(f) == 0;
}
static int samples(music_library *m, int context, int expected, int16_t out[128])
{
    for (int i = 0; i < 3000; i++) {
        if (music_library_render(m, context, out, 64, 128, 0) && out[0] == expected)
            return 1;
        SDL_Delay(1);
    }
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    CHECK(SDL_Init(SDL_INIT_TIMER)==0);
    char path[1024], a[1024], b[1024], playlist[1024], bad[1024];
    snprintf(path, sizeof path, "%s-%lu-configura\xc3\xa7\xc3\xa3o.ini", argv[1],
             test_process_id());
    snprintf(a, sizeof a, "%s-%lu-Som S\xc3\xa3o Paulo.wav", argv[1], test_process_id());
    snprintf(b, sizeof b, "%s-%lu-b.wav", argv[1], test_process_id());
    snprintf(playlist, sizeof playlist, "%s-%lu.m3u8", argv[1], test_process_id());
    snprintf(bad, sizeof bad, "%s-%lu-invalid.m3u8", argv[1], test_process_id());
    CHECK(wave(a,0) && wave(b,1));
    moon_options store;
    CHECK(moon_options_load(&store,path,NULL));
    music_library *m = music_library_open(&store);
    CHECK(m && !music_library_enabled(m));
    int one = music_library_create(m, "Minha sele\xc3\xa7\xc3\xa3o"),
        two = music_library_create(m, "Race");
    CHECK(one==0 && two==1);
    CHECK(music_library_add(m,one,a,"Som de teste") && music_library_add(m,two,b,NULL));
    CHECK(music_library_assign(m,ML_MENU,one) && music_library_assign(m,ML_RACE,two));
    int16_t out[128];
    for (int i = 0; i < 128; i++)
        out[i] = -1234;
    CHECK(!music_library_render(m,ML_MENU,out,64,128,0) && out[0]==-1234);
    CHECK(music_library_enable(m,1));
    CHECK(samples(m,ML_MENU,1000,out));
    CHECK(music_library_render(m,ML_MENU,out,64,128,1));
    for (int i = 0; i < 128; i++)
        CHECK(out[i]==0);
    CHECK(music_library_render(m,ML_MENU,out,64,128,0) && out[0]==1064);
    CHECK(music_library_render(m,ML_MENU,out,64,64,0) && out[0]==564);
    CHECK(samples(m,ML_RACE,2300,out));
    CHECK(samples(m,ML_MENU,1000,out));
    for (int c = ML_REPLAY; c < ML_CONTEXTS; c++) {
        CHECK(music_library_assign(m,c,two));
        CHECK(samples(m,c,2300,out));
    }
    CHECK(music_library_track_rename(m,one,0,"Faixa personalizada"));
    CHECK(music_library_add(m,one,b,"Second") && music_library_move(m,one,1,-1));
    CHECK(!strcmp(music_library_list(m,one)->tracks[0].title,"Second"));
    CHECK(music_library_track_enabled(m,one,0) && !music_library_list(m,one)->tracks[0].enabled);
    CHECK(music_library_shuffle(m,one));
    CHECK(music_library_export(m,one,playlist));
    CHECK(!music_library_export(m,one,a));    /* Never overwrite original audio. */
    CHECK(!music_library_export(m,one,path)); /* Nor live preferences. */
    int imported = music_library_import(m, playlist);
    CHECK(imported==2);
    const ml_playlist *p = music_library_list(m, imported);
    CHECK(p && p->count==2 && p->shuffle && !p->tracks[0].enabled && !strcmp(p->tracks[1].title,"Faixa personalizada"));
    CHECK(music_library_available(p->tracks[0].path) && music_library_available(p->tracks[1].path));
    int invalid_order[2]={0,0}, reversed[2]={1,0}, flags[2]={0,1};
    CHECK(!music_library_jukebox_commit(m,imported,invalid_order,flags,2,0));
    CHECK(music_library_jukebox_commit(m,imported,reversed,flags,2,0));
    CHECK(!strcmp(music_library_list(m,imported)->tracks[0].title,"Faixa personalizada"));
    CHECK(music_library_list(m,imported)->tracks[0].enabled && !music_library_list(m,imported)->tracks[1].enabled);
    flags[0]=1;flags[1]=0;
    CHECK(music_library_jukebox_commit(m,imported,reversed,flags,2,1));
    int copy = music_library_duplicate(m, imported);
    CHECK(copy==3 && music_library_rename(m,copy,"Duplicada"));
    FILE *f = file(bad, "wb");
    CHECK(f);
    fputs("#EXTM3U\nhttps://example.invalid/track.mp3\n", f);
    fclose(f);
    CHECK(music_library_import(m,bad)==-1 && music_library_count(m)==4);
    CHECK(music_library_remove(m,copy,0));
    CHECK(music_library_available(a) && music_library_available(b));
    CHECK(music_library_delete(m,copy) && music_library_count(m)==3);
    music_library_install(m);
    CHECK(samples(m,ML_RACE,2300,out));
    CHECK(moon_music_override_render(ML_RACE,out,64,128,0));
    char title[128];
    CHECK(moon_music_override_title(ML_RACE,title,sizeof title));
    CHECK(music_library_enable(m,0));
    if (argc > 2 && !strcmp(argv[2], "--negative"))
        music_library_enable(m, 1); /* Negative: break the disabled-source contract. */
    for (int i = 0; i < 128; i++)
        out[i] = -1234;
    CHECK(!moon_music_override_render(ML_RACE,out,64,128,0) && out[0]==-1234);
    music_library_preview(m, two, 0);
    CHECK(samples(m,ML_MENU,2300,out)); /* Audition without enabling the extension. */
    CHECK(!music_library_enabled(m));
    CHECK(music_library_enable(m,0));
    out[0] = -1234;
    CHECK(!moon_music_override_render(ML_MENU,out,64,128,0) && out[0]==-1234);
    music_library_preview(m, -1, -1);
    music_library_close(m);
    CHECK(!moon_music_override_render(ML_MENU,out,64,128,0));
    m = music_library_open(&store);
    CHECK(m && music_library_count(m)==3 && music_library_assignment(m,ML_RACE)==two && !music_library_enabled(m));
    CHECK(!strcmp(music_library_list(m,one)->tracks[1].title,"Faixa personalizada"));
    char missing[1024];
    snprintf(missing, sizeof missing, "%s-%lu-not-present.wav", argv[1], test_process_id());
    int absent = music_library_create(m, "Missing files");
    CHECK(absent==3 && music_library_add(m,absent,missing,NULL));
    CHECK(music_library_assign(m,ML_LAUNCHER,absent) && music_library_enable(m,1));
    for (int i = 0; i < 100; i++) {
        out[0] = -1234;
        CHECK(!music_library_render(m,ML_LAUNCHER,out,64,128,0) && out[0]==-1234);
        SDL_Delay(1);
    }
    CHECK(music_library_delete(m,absent));
    char pending[1040];
    snprintf(pending, sizeof pending, "%s.music.tmp", path);
    f = file(pending, "wb");
    CHECK(f);
    fputs("pending", f);
    fclose(f);
    CHECK(!music_library_rename(m,one,"Must roll back") && !strcmp(music_library_list(m,one)->name,"Minha sele\xc3\xa7\xc3\xa3o"));
#ifdef _WIN32
    wchar_t pending_wide[2048];
    CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,pending,-1,pending_wide,2048));
    CHECK(DeleteFileW(pending_wide));
#else
    CHECK(remove(pending)==0);
#endif

    f = file(bad, "wb");
    CHECK(f);
    fwrite("#EXTM3U\ntrack", 1, 13, f);
    fputc(0, f);
    fputs(".wav\n", f);
    fclose(f);
    CHECK(music_library_import(m,bad)==-1);
    if (argc > 3 && !strcmp(argv[2], "--compressed")) {
        int compressed = music_library_create(m, "Compressed audio");
        CHECK(compressed>=0 && music_library_add(m,compressed,argv[3],NULL));
        CHECK(music_library_assign(m,ML_LAUNCHER,compressed));
        int heard = 0;
        for (int i = 0; i < 5000 && !heard; i++) {
            if (music_library_render(m, ML_LAUNCHER, out, 64, 128, 0))
                for (int j = 0; j < 128; j++)
                    if (abs(out[j]) > 100)
                        heard = 1;
            SDL_Delay(1);
        }
        CHECK(heard);
        /* The generated tone is 440 Hz: 440 sign changes in 0.5 s of 48 kHz
           output; a source left at 44.1 kHz would give about 404. */
        int crossings = 0, peak = 0, sign = 0;
        for (int block = 0; block < 375; block++) {
            CHECK(music_library_render(m, ML_LAUNCHER, out, 64, 128, 0));
            for (int j = 0; j < 128; j += 2) {
                int v = out[j];
                peak = abs(v) > peak ? abs(v) : peak;
                if (abs(v) > 500) {
                    crossings += sign && (v > 0) != (sign > 0);
                    sign = v > 0 ? 1 : -1;
                }
            }
            SDL_Delay(1);
        }
        printf("compressed: %d sign changes in 0.5 s, peak %d\n", crossings, peak);
        CHECK(crossings >= 425 && crossings <= 455 && peak > 6000 && peak < 10000);
    }
    if(argc>3 && !strcmp(argv[2],"--mixed")) {
        music_library_preview(m,-1,-1);
        music_library_disc(m,argv[3]);
        int mixed=music_library_create(m,"Mixed race and slideshow");
        CHECK(mixed>=0 && music_library_add(m,mixed,b,"User song"));
        CHECK(music_library_add(m,mixed,"gt4:0","Disc original"));
        CHECK(music_library_assign(m,ML_RACE,mixed) && music_library_enable(m,1));
        CHECK(samples(m,ML_RACE,2300,out));
        /* Consume the rest of the exact half-second local WAV, then compare
           the next source against the original disc decoder sample by sample. */
        for(int block=1;block<375;block++) {
            CHECK(music_library_render(m,ML_RACE,out,64,128,0));
            for(int k=0;k<128;k++)CHECK(out[k]==2300);
            SDL_Delay(2);
        }
        const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[3],&error);CHECK(v);
        gt4_pcm_music *original=gt4_pcm_music_open(v,"racing",0);CHECK(original);
        int16_t expected[128];
        for(int block=0;block<128;block++) {
            if(block==64) {
                CHECK(music_library_render(m,ML_RACE,out,64,128,1));
                for(int k=0;k<128;k++)CHECK(!out[k]);
            }
            gt4_pcm_music_render(expected,64,original);
            CHECK(music_library_render(m,ML_RACE,out,64,128,0));
            CHECK(!memcmp(out,expected,sizeof expected));
            SDL_Delay(2);
        }
        gt4_pcm_music_close(original);gt4_vol_close(v);
        CHECK(music_library_export(m,mixed,playlist));
        int imported_mixed=music_library_import(m,playlist);CHECK(imported_mixed>=0);
        CHECK(!strcmp(music_library_list(m,imported_mixed)->tracks[1].path,"gt4:0"));
        int catalog=music_library_catalog(m);CHECK(catalog>=0);
        CHECK(music_library_list(m,catalog)->catalog);
        puts("PASS mixed playlist: local WAV -> original disc decoder, 8192 frames byte-exact; mixed M3U roundtrip");
    }
    music_library_close(m);
    SDL_Quit();
    puts(
        "PASS optional music: Unicode files, context routing, pause cursor, gain, edits, atomic persistence, M3U references, preview, original-source fallback");
    return 0;
}
