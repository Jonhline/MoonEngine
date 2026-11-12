#include "save_package.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#include <utime.h>
#endif

#define PACKAGE_FILES 16384
#define PACKAGE_NAME 480
#define PACKAGE_FILE_LIMIT (512ull*1024*1024)
#define PACKAGE_TOTAL_LIMIT (16ull*1024*1024*1024)
static const char manifest[]="MoonEnginePlayerPackage=1\nprofile=profile.ogt4\nlibrary=shared\n";
typedef struct { char name[PACKAGE_NAME]; uint64_t bytes; time_t modified; int created; } package_item;
typedef struct {
    package_item *items; unsigned count;
    uint64_t total;
    moon_save_package_report *report;
    void (*pump)(void *); void *user;
} package_context;
static int fail(package_context *c,const char *message)
{
    if (!c->report->error[0]) snprintf(c->report->error,sizeof c->report->error,"%s",message);
    return 0;
}
#ifdef _WIN32
static wchar_t *wide_path(const char *path)
{
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
    wchar_t *w=n ? malloc((size_t)n*sizeof *w) : NULL;
    if (w && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,w,n)) {free(w);w=NULL;}
    return w;
}
#endif
static FILE *file_open(const char *path,const char *mode)
{
#ifdef _WIN32
    wchar_t *w=wide_path(path), m[16]; FILE *f=NULL;
    if (w && MultiByteToWideChar(CP_UTF8,0,mode,-1,m,16)) f=_wfopen(w,m);
    free(w); return f;
#else
    return fopen(path,mode);
#endif
}
/* 0 absent, 1 regular file, 2 directory, -1 error/link/other. */
static int file_kind(const char *path,uint64_t *size,time_t *modified)
{
#ifdef _WIN32
    wchar_t *w=wide_path(path); WIN32_FILE_ATTRIBUTE_DATA d;
    int ok=w && GetFileAttributesExW(w,GetFileExInfoStandard,&d); free(w);
    if (!ok) return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND ? 0 : -1;
    if (d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) return -1;
    if (size) *size=((uint64_t)d.nFileSizeHigh<<32)|d.nFileSizeLow;
    if (modified) *modified=(time_t)((((uint64_t)d.ftLastWriteTime.dwHighDateTime<<32)|d.ftLastWriteTime.dwLowDateTime)/10000000ull-11644473600ull);
    return d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY ? 2 : 1;
#else
    struct stat s;
    if (lstat(path,&s)) return errno==ENOENT ? 0 : -1;
    if (size) *size=(uint64_t)s.st_size;
    if (modified) *modified=s.st_mtime;
    return S_ISREG(s.st_mode) ? 1 : S_ISDIR(s.st_mode) ? 2 : -1;
#endif
}
static int file_remove(const char *path,int directory)
{
#ifdef _WIN32
    wchar_t *w=wide_path(path); int ok=w && (directory ? RemoveDirectoryW(w) : DeleteFileW(w));
    free(w); return ok;
#else
    return !(directory ? rmdir(path) : unlink(path));
#endif
}
static int directory_create(const char *path)
{
#ifdef _WIN32
    wchar_t *w=wide_path(path); int ok=w && CreateDirectoryW(w,NULL); free(w); return ok;
#else
    return mkdir(path,0700)==0;
#endif
}
static int compare_name(const char *a,const char *b)
{
#ifdef _WIN32
    wchar_t *wa=wide_path(a), *wb=wide_path(b);
    int result=wa && wb ? CompareStringOrdinal(wa,-1,wb,-1,TRUE) : 0;
    free(wa);free(wb);
    return result ? result-CSTR_EQUAL : strcmp(a,b);
#else
    return strcasecmp(a,b);
#endif
}
static int same_name(const char *a,const char *b)
{
    return compare_name(a,b)==0;
}
static int safe_name(const char *name)
{
    size_t n=strlen(name);
    if (!n || n>=PACKAGE_NAME || name[0]=='/' || name[n-1]=='/' ||
        strpbrk(name,"\\:<>\"|?*")) return 0;
    for (const unsigned char *p=(const unsigned char *)name;*p;p++) if (*p<32 || *p==127) return 0;
    unsigned levels=0;
    for (const char *slash=name;*slash;slash++) if (*slash=='/' && ++levels>16) return 0;
    const char *p=name;
    while (*p) {
        const char *slash=strchr(p,'/'); size_t len=slash ? (size_t)(slash-p) : strlen(p);
        if (!len || (len==1 && *p=='.') || (len==2 && !memcmp(p,"..",2)) ||
            p[len-1]=='.' || p[len-1]==' ') return 0;
        char base[16]; size_t k=0;
        while (k<len && k<sizeof base-1 && p[k]!='.') {base[k]=p[k];k++;} base[k]=0;
        for (size_t j=0;j<k;j++) if (base[j]>='a' && base[j]<='z') base[j]-=32;
        if (same_name(base,"CON") || same_name(base,"NUL") || same_name(base,"AUX") || same_name(base,"PRN") ||
            (k==4 && (!memcmp(base,"COM",3) || !memcmp(base,"LPT",3)) && base[3]>='1' && base[3]<='9')) return 0;
        if (!slash) break; p=slash+1;
    }
#ifdef _WIN32
    wchar_t *w=wide_path(name); if (!w) return 0; free(w);
#endif
    return 1;
}
static int library_name(const char *name)
{
    const char *roots[]={"photos/","replays/","films/","imports/"};
    if (!safe_name(name)) return 0;
    for (unsigned i=0;i<sizeof roots/sizeof *roots;i++)
        if (!strncmp(name,roots[i],strlen(roots[i]))) return 1;
    return 0;
}
static int full_path(char out[1400],const char *root,const char *name)
{
    return snprintf(out,1400,"%s/%s",root,name)<1400;
}
static int add_item(package_context *c,const char *name,uint64_t bytes,time_t modified)
{
    if (c->count==PACKAGE_FILES || strlen(name)>=PACKAGE_NAME || bytes>PACKAGE_FILE_LIMIT ||
        bytes>PACKAGE_TOTAL_LIMIT-c->total) return fail(c,"The player package exceeds supported file or size limits.");
    package_item *item=&c->items[c->count++];
    snprintf(item->name,sizeof item->name,"%s",name); item->bytes=bytes;item->modified=modified;
    c->total+=bytes;return 1;
}
static int item_compare(const void *left,const void *right)
{
    return compare_name(((const package_item *)left)->name,((const package_item *)right)->name);
}
static int validate_names(package_context *c)
{
    /* Sorting and binary lookup keep large complete libraries out of a
       quadratic case-folding pass. Names and every ancestor are checked. */
    qsort(c->items,c->count,sizeof *c->items,item_compare);
    for (unsigned i=0;i<c->count;i++) {
        if (i && same_name(c->items[i-1].name,c->items[i].name))
            return fail(c,"The package contains duplicate or ambiguous filenames.");
        package_item key=c->items[i];
        for (char *slash=key.name;*slash;slash++) if (*slash=='/') {
            *slash=0;
            int found=bsearch(&key,c->items,c->count,sizeof *c->items,item_compare)!=NULL;
            *slash='/';
            if (found) return fail(c,"Conflicting file and directory paths.");
        }
    }
    return 1;
}
static int collect(package_context *c,const char *relative,int depth)
{
    if (depth>16) return fail(c,"A library folder is nested too deeply.");
    char path[1400]; uint64_t bytes=0;time_t modified=0;
    if (!full_path(path,gt4_save_directory(),relative)) return fail(c,"A library path is too long.");
    int kind=file_kind(path,&bytes,&modified);
    if (!kind) return 1;
    if (kind==1) return library_name(relative) ? add_item(c,relative,bytes,modified) : fail(c,"Unsupported library filename.");
    if (kind!=2) return fail(c,"A library path is unreadable or is a link. Nothing was exported.");
#ifdef _WIN32
    char pattern[1400]; if (snprintf(pattern,sizeof pattern,"%s/*",path)>=sizeof pattern) return 0;
    wchar_t *wp=wide_path(pattern); WIN32_FIND_DATAW d;
    HANDLE h=wp ? FindFirstFileW(wp,&d) : INVALID_HANDLE_VALUE; free(wp);
    if (h==INVALID_HANDLE_VALUE) return fail(c,"Could not list a library folder.");
    int ok=1;
    do {
        if (!wcscmp(d.cFileName,L".") || !wcscmp(d.cFileName,L"..")) continue;
        char name[PACKAGE_NAME], child[PACKAGE_NAME];
        if (!WideCharToMultiByte(CP_UTF8,0,d.cFileName,-1,name,sizeof name,NULL,NULL) ||
            snprintf(child,sizeof child,"%s/%s",relative,name)>=sizeof child || !safe_name(child) || !collect(c,child,depth+1)) {ok=0;break;}
    } while (FindNextFileW(h,&d));
    if (ok && GetLastError()!=ERROR_NO_MORE_FILES) ok=0;
    FindClose(h);return ok ? 1 : fail(c,"Could not read the entire library.");
#else
    DIR *dir=opendir(path); if (!dir) return fail(c,"Could not list a library folder.");
    struct dirent *d; int ok=1; errno=0;
    while ((d=readdir(dir))) {
        if (!strcmp(d->d_name,".") || !strcmp(d->d_name,"..")) continue;
        char child[PACKAGE_NAME];
        if (snprintf(child,sizeof child,"%s/%s",relative,d->d_name)>=sizeof child || !safe_name(child) || !collect(c,child,depth+1)) {ok=0;break;}
        errno=0;
    }
    if (errno) ok=0;
    closedir(dir); return ok ? 1 : fail(c,"Could not read the entire library.");
#endif
}
static int context_open(package_context *c,moon_save_package_report *report,void (*pump)(void *),void *user)
{
    memset(report,0,sizeof *report);memset(c,0,sizeof *c);
    c->report=report;c->pump=pump;c->user=user;
    c->items=calloc(PACKAGE_FILES,sizeof *c->items);
    return c->items!=NULL || fail(c,"Not enough memory for the player package.");
}
static int writer_add(mz_zip_archive *zip,const char *archive_name,const char *source,uint64_t size,time_t modified)
{
    FILE *f=file_open(source,"rb"); if (!f) return 0;
    MZ_TIME_T stamp=(MZ_TIME_T)modified;
    int ok=mz_zip_writer_add_cfile(zip,archive_name,f,size,&stamp,NULL,0,MZ_BEST_SPEED,NULL,0,NULL,0);
    fclose(f);return ok;
}
static int rename_replace(const char *source,const char *destination)
{
#ifdef _WIN32
    wchar_t *a=wide_path(source), *b=wide_path(destination);
    int ok=a && b && MoveFileExW(a,b,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    free(a);free(b);return ok;
#else
    return !rename(source,destination);
#endif
}
int moon_save_package_is_file(const char *path)
{
    FILE *f=file_open(path,"rb"); unsigned char signature[4];
    int yes=f && fread(signature,1,4,f)==4 && !memcmp(signature,"PK\3\4",4);
    if (f) fclose(f);return yes;
}
int moon_save_package_export(int slot,const char *path,void (*pump)(void *),void *user,moon_save_package_report *report)
{
    package_context c;if (!context_open(&c,report,pump,user)) return 0;
    char profile[1400], temporary[1400];gt4_save_info info;uint8_t *payload=NULL;size_t payload_size=0;
    int ok=path && gt4_save_export_destination(path) && snprintf(temporary,sizeof temporary,"%s.tmp",path)<sizeof temporary &&
           snprintf(profile,sizeof profile,"%s/slot%02d.ogt4",gt4_save_directory(),slot)<sizeof profile && gt4_save_info_get(slot,&info)>0;
    int recovered=ok ? gt4_save_read(profile,&payload,&payload_size,NULL) : 0;free(payload);
    if (recovered==2) strncat(profile,".bak",sizeof profile-strlen(profile)-1);
    const char *roots[]={"photos","replays","films","imports"};
    for (unsigned i=0;ok && i<sizeof roots/sizeof *roots;i++) ok=collect(&c,roots[i],0);
    if (ok) ok=validate_names(&c);
    if (!ok || recovered<=0) {free(c.items);return fail(&c,"Could not read the complete saved player and library.");}
    FILE *output=file_open(temporary,"wbx");
    if (!output) {free(c.items);return fail(&c,"Could not create a temporary package. Existing files were kept.");}
    mz_zip_archive zip={0};int initialized=mz_zip_writer_init_cfile(&zip,output,MZ_ZIP_FLAG_WRITE_ZIP64);
    uint64_t profile_size=0;time_t modified=0;
    ok=initialized && file_kind(profile,&profile_size,&modified)==1 &&
       mz_zip_writer_add_mem(&zip,"manifest.ini",manifest,sizeof manifest-1,MZ_BEST_SPEED) &&
       writer_add(&zip,"profile.ogt4",profile,profile_size,modified);
    for (unsigned i=0;ok && i<c.count;i++) {
        char source[1400], entry[600];
        ok=full_path(source,gt4_save_directory(),c.items[i].name) &&
           snprintf(entry,sizeof entry,"library/%s",c.items[i].name)<sizeof entry &&
           writer_add(&zip,entry,source,c.items[i].bytes,c.items[i].modified);
        if (pump) pump(user);
    }
    if (ok) ok=mz_zip_writer_finalize_archive(&zip);
    if (initialized) mz_zip_writer_end(&zip);
    if (ok && fflush(output)) ok=0;
#ifdef _WIN32
    if (ok && _commit(_fileno(output))) ok=0;
#else
    if (ok && fsync(fileno(output))) ok=0;
#endif
    if (fclose(output)) ok=0;
    if (ok) ok=rename_replace(temporary,path);
    if (!ok) {file_remove(temporary,0);fail(&c,"Could not finish the complete package. Existing export was kept.");}
    else {report->files=c.count+1;report->bytes=c.total+profile_size;}
    free(c.items);return ok;
}

/* Parent directories are checked component by component; existing links never
   redirect extraction/promotion outside the selected save directory. */
static int parents(const char *root,const char *name,int create)
{
    char path[1400];if (!full_path(path,root,name)) return 0;
    size_t start=strlen(root)+1;
    for (size_t i=start;path[i];i++) if (path[i]=='/') {
        path[i]=0;int kind=file_kind(path,NULL,NULL);
        if (!kind && create) kind=directory_create(path) ? 2 : -1;
        path[i]='/';if (kind!=2 && (kind || create)) return 0;
        if (!kind && !create) return 1;
    }
    return 1;
}
static int same_contents(const char *a,const char *b)
{
    FILE *fa=file_open(a,"rb"), *fb=file_open(b,"rb");
    if (!fa || !fb) {if (fa) fclose(fa);if (fb) fclose(fb);return 0;}
    unsigned char left[65536],right[65536];int ok=1;
    for (;;) {
        size_t x=fread(left,1,sizeof left,fa), y=fread(right,1,sizeof right,fb);
        if (x!=y || memcmp(left,right,x)) {ok=0;break;}
        if (!x) {ok=!ferror(fa) && !ferror(fb);break;}
    }
    fclose(fa);fclose(fb);return ok;
}
static int promote(const char *source,const char *destination)
{
#ifdef _WIN32
    wchar_t *a=wide_path(source), *b=wide_path(destination);
    int ok=a && b && MoveFileW(a,b);free(a);free(b);return ok;
#else
    FILE *in=file_open(source,"rb"), *out=in ? file_open(destination,"wbx") : NULL;
    if (!out) {if (in) fclose(in);return 0;}
    char buffer[65536];size_t n;int ok=1;
    while ((n=fread(buffer,1,sizeof buffer,in))) if (fwrite(buffer,1,n,out)!=n) {ok=0;break;}
    if (ferror(in)) ok=0;fclose(in);if (fclose(out)) ok=0;
    if (!ok) file_remove(destination,0);return ok;
#endif
}
static void set_time(const char *path,time_t stamp)
{
#ifdef _WIN32
    wchar_t *w=wide_path(path);HANDLE h=w ? CreateFileW(w,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL) : INVALID_HANDLE_VALUE;
    free(w);
    if (h!=INVALID_HANDLE_VALUE) {
        uint64_t ticks=((uint64_t)stamp+11644473600ull)*10000000ull;
        FILETIME ft={(DWORD)ticks,(DWORD)(ticks>>32)};SetFileTime(h,NULL,NULL,&ft);CloseHandle(h);
    }
#else
    struct utimbuf t={stamp,stamp};utime(path,&t);
#endif
}
static void clean_stage(package_context *c,const char *stage)
{
    for (unsigned i=0;i<c->count;i++) {
        char path[1400];if (!full_path(path,stage,c->items[i].name)) continue;
        file_remove(path,0);
        char *slash;
        while ((slash=strrchr(path,'/')) && (size_t)(slash-path)>strlen(stage)) {*slash=0;file_remove(path,1);}
    }
    char profile[1400];if (full_path(profile,stage,"profile.ogt4")) file_remove(profile,0);
    file_remove(stage,1);
}
static int validate_profile(const char *path)
{
    uint8_t *payload=NULL;size_t size=0;
    if (gt4_save_read(path,&payload,&size,NULL)!=1) return 0;
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",NULL,NULL,NULL);ah_val name;
    int ok=gt4_state_merge(vm,game,payload,size) && ah_get_member(vm,game,"username",&name) && gt4_save_valid_name(ah_cstr(name));
    ah_vm_free(vm);free(payload);return ok;
}
typedef struct {
    gt4_save_writer writer;void *user;
    char profile[1400];int wrote;
} package_commit;
static int commit_write(const char *path,const uint8_t *data,size_t size,void *user)
{
    package_commit *commit=user;
    int profile=!strcmp(path,commit->profile);
    if (profile && file_kind(path,NULL,NULL)!=0) return 0;
    int ok=commit->writer ? commit->writer(path,data,size,commit->user) : gt4_save_write(path,data,size);
    if (ok && profile) commit->wrote=1;
    return ok;
}
int moon_save_package_import(int slot,const char *path,gt4_save_writer writer,void (*pump)(void *),void *user,moon_save_package_report *report)
{
    package_context c;if (!context_open(&c,report,pump,user)) return 0;
    gt4_save_info info;
    if (slot<1 || slot>GT4_SAVE_SLOTS || gt4_save_info_get(slot,&info)!=0) {free(c.items);return fail(&c,"Choose an empty slot before importing a complete player package.");}
    FILE *input=file_open(path,"rb");mz_zip_archive zip={0};
    int initialized=input && mz_zip_reader_init_cfile(&zip,input,0,0);
    int ok=initialized, profile_index=-1, manifest_index=-1;
    unsigned entries=ok ? mz_zip_reader_get_num_files(&zip) : 0;
    if (entries<2 || entries>PACKAGE_FILES+2) ok=0;
    for (unsigned i=0;ok && i<entries;i++) {
        mz_zip_archive_file_stat s;
        if (!mz_zip_reader_file_stat(&zip,i,&s) || s.m_is_directory || s.m_is_encrypted || !s.m_is_supported ||
            mz_zip_reader_get_filename(&zip,i,NULL,0)>sizeof s.m_filename ||
            ((s.m_external_attr>>16)&0170000)==0120000) {ok=0;break;}
        if (!strcmp(s.m_filename,"manifest.ini")) {if (manifest_index>=0) {ok=0;break;}manifest_index=(int)i;}
        else if (!strcmp(s.m_filename,"profile.ogt4")) {
            if (profile_index>=0 || s.m_uncomp_size>GT4_SAVE_LIMIT+32) {ok=0;break;}profile_index=(int)i;
        } else if (strncmp(s.m_filename,"library/",8) || !library_name(s.m_filename+8) ||
                   !add_item(&c,s.m_filename+8,s.m_uncomp_size,(time_t)s.m_time)) {ok=0;break;}
    }
    if (ok) ok=validate_names(&c);
    char header[sizeof manifest]={0};mz_zip_archive_file_stat ms;
    if (ok) ok=manifest_index>=0 && profile_index>=0 && mz_zip_reader_file_stat(&zip,(unsigned)manifest_index,&ms) &&
                ms.m_uncomp_size==sizeof manifest-1 && mz_zip_reader_extract_to_mem(&zip,(unsigned)manifest_index,header,sizeof header,0) &&
                !memcmp(header,manifest,sizeof manifest-1);
    char stage[1400]={0};int stage_created=0;static unsigned serial;
    if (ok) {
        int made=0;
        for (unsigned tries=0;tries<100 && !made;tries++) {
            if (snprintf(stage,sizeof stage,"%s/.moon-import-%llu-%u",gt4_save_directory(),(unsigned long long)time(NULL),++serial)>=sizeof stage) break;
            made=directory_create(stage);
        }
        ok=made;stage_created=made;
    }
    if (!ok) fail(&c,"Invalid or unsupported player package. No player or library files were changed.");
    char staged_profile[1400];
    if (ok) {
        ok=full_path(staged_profile,stage,"profile.ogt4");
        FILE *f=ok ? file_open(staged_profile,"wbx") : NULL;
        ok=f && mz_zip_reader_extract_to_cfile(&zip,(unsigned)profile_index,f,0);
        if (f && fclose(f)) ok=0;
        if (ok) ok=validate_profile(staged_profile);
    }
    /* Extract and CRC-check every file before touching the live library. */
    for (unsigned i=0;ok && i<c.count;i++) {
        char staged[1400],entry[600];
        ok=full_path(staged,stage,c.items[i].name) && parents(stage,c.items[i].name,1) &&
           snprintf(entry,sizeof entry,"library/%s",c.items[i].name)<sizeof entry;
        int index=ok ? mz_zip_reader_locate_file(&zip,entry,NULL,MZ_ZIP_FLAG_CASE_SENSITIVE) : -1;
        FILE *f=index>=0 ? file_open(staged,"wbx") : NULL;
        ok=f && mz_zip_reader_extract_to_cfile(&zip,(unsigned)index,f,0);
        if (f && fclose(f)) ok=0;
        if (pump) pump(user);
    }
    if (initialized) mz_zip_reader_end(&zip);if (input) fclose(input);
    /* Same names are reused only when contents are identical, including lists.
       This preserves all references without overwriting another library. */
    for (unsigned i=0;ok && i<c.count;i++) {
        char target[1400],staged[1400];
        ok=full_path(target,gt4_save_directory(),c.items[i].name) && full_path(staged,stage,c.items[i].name) && parents(gt4_save_directory(),c.items[i].name,0);
        int kind=ok ? file_kind(target,NULL,NULL) : -1;
        if (kind && (kind!=1 || !same_contents(staged,target))) {
            snprintf(report->error,sizeof report->error,"Library conflict: %.160s. Use an empty library; existing files were kept.",c.items[i].name);ok=0;
        }
    }
    for (unsigned i=0;ok && i<c.count;i++) {
        char target[1400],staged[1400];full_path(target,gt4_save_directory(),c.items[i].name);full_path(staged,stage,c.items[i].name);
        int kind=file_kind(target,NULL,NULL);
        if (!kind) {
            ok=parents(gt4_save_directory(),c.items[i].name,1) && promote(staged,target);
            if (ok) {c.items[i].created=1;set_time(target,c.items[i].modified);}
        } else ok=kind==1 && same_contents(staged,target);
        if (pump) pump(user);
    }
    package_commit commit={0};commit.writer=writer;commit.user=user;
    snprintf(commit.profile,sizeof commit.profile,"%s/slot%02d.ogt4",gt4_save_directory(),slot);
    if (ok) ok=gt4_save_import_ps2_with_writer(slot,staged_profile,commit_write,&commit);
    if (!ok) {
        for (unsigned i=0;i<c.count;i++) if (c.items[i].created) {
            char target[1400];if (full_path(target,gt4_save_directory(),c.items[i].name)) file_remove(target,0);
        }
        /* The destination was empty before this import. A failed active-index
           write can leave its new slot on disk; remove only that new file. */
        if (commit.wrote) file_remove(commit.profile,0);
        fail(&c,"Could not import the complete package. Your current player and library were kept.");
    } else {report->files=c.count+1;report->bytes=c.total;}
    if (stage_created) clean_stage(&c,stage);
    free(c.items);return ok;
}
