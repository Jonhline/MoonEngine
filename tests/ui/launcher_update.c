#include "launcher_release.h"
#include "launcher_update.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"update check line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    if(argc>2 && !strcmp(argv[1],"--network")) {
#ifdef _WIN32
        const char *url=getenv("OPENGT4_TEST_UPDATE_URL"),*token=getenv("OPENGT4_TEST_UPDATE_TOKEN");
        launcher_update_job *j=launcher_update_start(url,token,NULL);CHECK(j);
        launcher_update_manifest m;int r=0;
        for(int i=0;i<300 && !(r=launcher_update_poll(j,&m,NULL));i++) Sleep(50);
        if(!strcmp(argv[2],"hidden")) {CHECK(r==-1);launcher_update_release(j);puts("PASS no update offer on equal/error response");return 0;}
        CHECK(r==1 && launcher_update_newer(m.version,OPENGT4_VERSION));launcher_update_release(j);
        j=launcher_update_start(url,token,&m);CHECK(j);
        const wchar_t *path=NULL;r=0;
        for(int i=0;i<600 && !(r=launcher_update_poll(j,NULL,&path));i++) Sleep(50);
        CHECK(r==1 && path && launcher_update_verify(path,&m));
        m.sha256[0]=m.sha256[0]=='0'?'1':'0';CHECK(!launcher_update_verify(path,&m));
        launcher_update_release(j);
        j=launcher_update_start(url,token,&m);CHECK(j);r=0;
        for(int i=0;i<600 && !(r=launcher_update_poll(j,NULL,NULL));i++) Sleep(50);
        CHECK(r==-1);launcher_update_release(j);
        puts("PASS authenticated WinHTTP metadata/download and bad hash rejection");return 0;
#else
        return 1;
#endif
    }
    CHECK(launcher_update_newer("0.3.0","0.2.0"));CHECK(!launcher_update_newer("0.3.0","0.3.0"));
    CHECK(!launcher_update_newer("0.2.9","0.3.0"));CHECK(launcher_update_newer("1.0.0","0.99.99"));
    CHECK(!launcher_update_newer("0.03.0","0.2.0"));CHECK(!launcher_update_newer("1.0.0.exe","0.2.0"));
    launcher_update_manifest m;
    const char *valid="{\"version\":\"0.3.0\",\"setup\":\"OpenGT4-0.3.0-setup.exe\",\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\"size\":3,\"notes\":\"Test\\nupdate\"}";
    CHECK(launcher_update_parse(valid,&m) && m.size==3 && !strcmp(m.notes,"Test\nupdate"));
    char damaged[4096];strcpy(damaged,valid);char *p=strstr(damaged,"OpenGT4");p[0]='/' ;CHECK(!launcher_update_parse(damaged,&m));
    CHECK(!launcher_update_parse("{}",&m));CHECK(!launcher_update_parse("{\"version\":\"0.3.0\",}",&m));
    CHECK(!launcher_update_parse("{\"notes\":\"\\u\"}",&m));
    CHECK(!launcher_update_parse("{\"size\":999999999999999999999999}",&m));
    CHECK(launcher_update_parse(valid,&m));
#ifdef _WIN32
    wchar_t folder[MAX_PATH],file[MAX_PATH];CHECK(GetTempPathW(MAX_PATH,folder));CHECK(GetTempFileNameW(folder,L"OGT",0,file));
    HANDLE f=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);CHECK(f!=INVALID_HANDLE_VALUE);
    DWORD n;CHECK(WriteFile(f,"abc",3,&n,NULL)&&n==3);CloseHandle(f);
    CHECK(launcher_update_verify(file,&m));m.sha256[0]='0';CHECK(!launcher_update_verify(file,&m));
    CHECK(launcher_update_parse(valid,&m));m.size=4;CHECK(!launcher_update_verify(file,&m));DeleteFileW(file);
    CHECK(!launcher_update_start("http://example.invalid/","token",NULL));
    CHECK(!launcher_update_start("https://example.invalid/","bad\r\ntoken",NULL));
#endif
    if(argc>1 && !strcmp(argv[1],"--negative")) CHECK(launcher_update_newer("0.3.0","0.3.0"));
    puts("PASS update version, JSON, paths, SHA-256, size and transport validation");return 0;
}
