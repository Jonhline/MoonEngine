/* P: native PC update transport. No game simulation or disc data is involved. */
#include "launcher_release.h"
#include "launcher_update.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
static int version(const char *s,unsigned v[3])
{
    for(int k=0;k<3;k++) {
        if (!isdigit((unsigned char)*s)) return 0;
        const char *first=s; unsigned n=0;
        while(isdigit((unsigned char)*s)) {
            unsigned d=*s++-'0'; if(n>(INT_MAX-d)/10u) return 0; n=n*10+d;
        }
        if(s-first>1 && *first=='0') return 0;
        v[k]=n;
        if(k<2 && *s++!='.') return 0;
    }
    return !*s;
}
int launcher_update_newer(const char *remote,const char *local)
{
    unsigned a[3],b[3]; if(!remote||!local||!version(remote,a)||!version(local,b)) return 0;
    for(int k=0;k<3;k++) if(a[k]!=b[k]) return a[k]>b[k];
    return 0;
}
static void space(const char **s) { while(isspace((unsigned char)**s)) ++*s; }
static int string(const char **s,char *out,size_t capacity)
{
    if(*(*s)++!='"') return 0;
    size_t n=0;
    while(**s && **s!='"') {
        unsigned char c=*(*s)++;
        if(c<32) return 0;
        if(c=='\\') {
            c=*(*s)++; if(!c) return 0;
            switch(c) {
            case '"': case '\\': case '/': break;
            case 'b': c='\b'; break; case 'f': c='\f'; break;
            case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
            case 'u': {
                unsigned code=0;
                for(int i=0;i<4;i++) { unsigned char h=*(*s)++; if(!isxdigit(h)) return 0;
                    code=code*16+(isdigit(h)?h-'0':tolower(h)-'a'+10); }
                /* Release metadata is UTF-8. Escaped non-ASCII notes are
                   validated but replaced; filenames/version remain ASCII. */
                c=code>=32 && code<127?(unsigned char)code:'?'; break;
            }
            default:return 0;
            }
        }
        if(n+1>=capacity) return 0;
        out[n++]=(char)c;
    }
    if(**s!='"') return 0;
    ++*s; out[n]=0; return 1;
}
int launcher_update_parse(const char *json,launcher_update_manifest *m)
{
    if(!json || !m) return 0;
    memset(m,0,sizeof *m); const char *s=json; unsigned seen=0;
    space(&s); if(*s++!='{') return 0;
    for(;;) {
        char key[64]; space(&s); if(!string(&s,key,sizeof key)) return 0;
        space(&s); if(*s++!=':') return 0; space(&s);
        unsigned bit=0; char *out=NULL; size_t cap=0;
        if(!strcmp(key,"version")) {bit=1;out=m->version;cap=sizeof m->version;}
        else if(!strcmp(key,"setup")) {bit=2;out=m->setup;cap=sizeof m->setup;}
        else if(!strcmp(key,"sha256")) {bit=4;out=m->sha256;cap=sizeof m->sha256;}
        else if(!strcmp(key,"notes")) {bit=16;out=m->notes;cap=sizeof m->notes;}
        else if(!strcmp(key,"size")) bit=8;
        else return 0;
        if(seen&bit) return 0; seen|=bit;
        if(out) {if(!string(&s,out,cap)) return 0;}
        else {
            if(!isdigit((unsigned char)*s)) return 0;
            while(isdigit((unsigned char)*s)) {
                m->size=m->size*10+(*s++-'0'); if(m->size>1073741824u) return 0;
            }
        }
        space(&s); if(*s=='}') {++s;break;} if(*s++!=',') return 0;
    }
    space(&s); if(*s || (seen&15)!=15 || !m->size) return 0;
    unsigned parts[3]; if(!version(m->version,parts) || strlen(m->sha256)!=64) return 0;
    for(int i=0;i<64;i++) if(!isxdigit((unsigned char)m->sha256[i])) return 0;
    size_t n=strlen(m->setup); if(n<5 || strcmp(m->setup+n-4,".exe") || strstr(m->setup,"..")) return 0;
    for(const char *p=m->setup;*p;p++) if(!isalnum((unsigned char)*p)&&*p!='.'&&*p!='-'&&*p!='_') return 0;
    return 1;
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
struct launcher_update_job {
    volatile LONG refs,done,cancel;
    int download,ok,launched;
    char url[2048],token[1024];
    launcher_update_manifest manifest;
    wchar_t folder[MAX_PATH],setup[MAX_PATH];
};
static void clean(launcher_update_job *j)
{
    if(j->setup[0] && !j->launched) DeleteFileW(j->setup);
    if(j->folder[0] && !j->launched) RemoveDirectoryW(j->folder);
}
void launcher_update_release(launcher_update_job *j)
{
    if(j && InterlockedDecrement(&j->refs)==0) {clean(j); free(j);}
}
static HINTERNET request(const char *url,const char *token,HINTERNET *session,HINTERNET *connection)
{
    wchar_t full[4096],host[512],path[3072],auth[1400];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url,-1,full,4096) ||
       !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,token,-1,auth,1400) ||
       wcspbrk(auth,L"\r\n")) return NULL;
    URL_COMPONENTS u={0}; u.dwStructSize=sizeof u; u.lpszHostName=host;u.dwHostNameLength=512;
    u.lpszUrlPath=path;u.dwUrlPathLength=3072;u.dwUserNameLength=u.dwPasswordLength=(DWORD)-1;
    if(!WinHttpCrackUrl(full,0,0,&u) || u.nScheme!=INTERNET_SCHEME_HTTPS ||
       u.dwUserNameLength || u.dwPasswordLength) return NULL;
    *session=WinHttpOpen(L"MoonEngine/" L"updater",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!*session) return NULL;
    WinHttpSetTimeouts(*session,3000,3000,5000,5000);
    *connection=WinHttpConnect(*session,host,u.nPort,0); if(!*connection) return NULL;
    HINTERNET r=WinHttpOpenRequest(*connection,L"GET",path,NULL,WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!r) return NULL;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(r,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof redirect);
    wchar_t header[1500]; swprintf(header,1500,L"Authorization: Bearer %s\r\n",auth);
    DWORD status=0,n=sizeof status;
    if(!WinHttpSendRequest(r,header,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(r,NULL) ||
       !WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                           NULL,&status,&n,NULL) || status!=200) {WinHttpCloseHandle(r);return NULL;}
    return r;
}
int launcher_update_verify(const wchar_t *path,const launcher_update_manifest *m)
{
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(f==INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER size; int ok=GetFileSizeEx(f,&size) && (uint64_t)size.QuadPart==m->size;
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;
    ok=ok && BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)==0 &&
       BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)==0;
    unsigned char buf[65536],digest[32]; DWORD n;
    while(ok) {
        if(!ReadFile(f,buf,sizeof buf,&n,NULL)) {ok=0;break;}
        if(!n) break; ok=BCryptHashData(hash,buf,n,0)==0;
    }
    ok=ok && BCryptFinishHash(hash,digest,sizeof digest,0)==0;
    if(ok) for(int i=0;i<32;i++) {char pair[3]={m->sha256[i*2],m->sha256[i*2+1],0};
        if(digest[i]!=(unsigned char)strtoul(pair,NULL,16)) {ok=0;break;} }
    if(hash) BCryptDestroyHash(hash); if(alg) BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(f); return ok;
}
static int receive(launcher_update_job *j,HINTERNET r)
{
    unsigned char data[65536]; DWORD n; uint64_t total=0;
    HANDLE f=INVALID_HANDLE_VALUE; char json[16385];
    if(j->download) {
        wchar_t temp[MAX_PATH],unique[MAX_PATH];
        if(!GetTempPathW(MAX_PATH,temp) || !GetTempFileNameW(temp,L"OG4",0,unique)) return 0;
        DeleteFileW(unique); if(!CreateDirectoryW(unique,NULL)) return 0;
        wcscpy(j->folder,unique); swprintf(j->setup,MAX_PATH,L"%s\\setup.exe",unique);
        f=CreateFileW(j->setup,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(f==INVALID_HANDLE_VALUE) return 0;
    }
    int ok=1; ULONGLONG deadline=GetTickCount64()+(j->download?1200000:20000);
    while(!InterlockedCompareExchange(&j->cancel,0,0)) {
        if(GetTickCount64()>deadline) {ok=0;break;}
        if(!WinHttpReadData(r,data,sizeof data,&n)) {ok=0;break;}
        if(!n) break;
        uint64_t limit=j->download?j->manifest.size:sizeof json-1;
        if(total+n>limit) {ok=0;break;}
        if(j->download) {DWORD written; if(!WriteFile(f,data,n,&written,NULL)||written!=n) {ok=0;break;}}
        else memcpy(json+total,data,n);
        total+=n;
    }
    if(f!=INVALID_HANDLE_VALUE) CloseHandle(f);
    if(InterlockedCompareExchange(&j->cancel,0,0)) ok=0;
    if(!ok) return 0;
    if(j->download) return total==j->manifest.size && launcher_update_verify(j->setup,&j->manifest);
    json[total]=0;
    return !memchr(json,0,(size_t)total) && launcher_update_parse(json,&j->manifest) &&
           launcher_update_newer(j->manifest.version,MOON_VERSION);
}
static DWORD WINAPI worker(void *context)
{
    launcher_update_job *j=context;
    HINTERNET session=NULL,connection=NULL;
    char url[2300]; snprintf(url,sizeof url,"%s%s",j->url,j->download?j->manifest.setup:"latest.json");
    HINTERNET r=request(url,j->token,&session,&connection);
    j->ok=r && receive(j,r);
    if(r) WinHttpCloseHandle(r); if(connection) WinHttpCloseHandle(connection);
    if(session) WinHttpCloseHandle(session);
    InterlockedExchange(&j->done,1); launcher_update_release(j); return 0;
}
launcher_update_job *launcher_update_start(const char *url,const char *token,
                                          const launcher_update_manifest *download)
{
    if(!url || strncmp(url,"https://",8) || strlen(url)>=2048 || !*token ||
       strlen(token)>=1024 || strpbrk(token,"\r\n") || url[strlen(url)-1]!='/') return NULL;
    launcher_update_job *j=calloc(1,sizeof *j); if(!j) return NULL;
    j->refs=2;strcpy(j->url,url);strcpy(j->token,token);
    if(download) {j->download=1;j->manifest=*download;}
    HANDLE thread=CreateThread(NULL,0,worker,j,0,NULL);
    if(!thread) {free(j);return NULL;} CloseHandle(thread);return j;
}
int launcher_update_poll(launcher_update_job *j,launcher_update_manifest *m,const wchar_t **setup)
{
    if(!j || !InterlockedCompareExchange(&j->done,0,0)) return 0;
    if(!j->ok) return -1;
    if(m) *m=j->manifest; if(setup) *setup=j->download?j->setup:NULL; return 1;
}
int launcher_update_install(launcher_update_job *j)
{
    if(!j || !j->download || launcher_update_poll(j,NULL,NULL)!=1 ||
       !launcher_update_verify(j->setup,&j->manifest)) return 0;
    wchar_t executable[32768],command[32768];
    if(!GetModuleFileNameW(NULL,executable,32768)) return 0;
    wchar_t *end=wcsrchr(executable,L'\\'); if(!end) return 0; *end=0;
    if(swprintf(command,32768,L"\"%s\" /VERYSILENT /CURRENTUSER /SUPPRESSMSGBOXES /NORESTART /DIR=\"%s\"",
                j->setup,executable)<0) return 0;
    STARTUPINFOW startup={0};startup.cb=sizeof startup; PROCESS_INFORMATION process;
    if(!CreateProcessW(j->setup,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&process)) return 0;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);j->launched=1;return 1;
}
#endif
