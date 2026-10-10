/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <sys/stat.h>
#include <time.h>
static volatile LONG interrupted;
static LARGE_INTEGER frequency;
static BOOL WINAPI control_handler(DWORD event) {
    if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) {
        InterlockedExchange(&interrupted,SIGINT);
        return TRUE;
    }
    /* Windows can terminate a console on CLOSE/LOGOFF; never promise cleanup then. */
    return FALSE;
}
static void signal_handler(int sig) { InterlockedExchange(&interrupted,sig); }
int inv_install_signal_handlers(void) {
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0)return 0;
    if(signal(SIGINT,signal_handler)==SIG_ERR || signal(SIGTERM,signal_handler)==SIG_ERR)return 0;
    return SetConsoleCtrlHandler(control_handler,TRUE)!=0;
}
int inv_interrupted(void) { return (int)InterlockedCompareExchange(&interrupted,0,0); }
uint64_t inv_now_ns(void) {
    LARGE_INTEGER t,f;
    uint64_t value,divisor;
    /* Safe also for library users that did not install process signal handlers. */
    f=frequency;
    if(!f.QuadPart && !QueryPerformanceFrequency(&f))return 0;
    if(f.QuadPart<=0 || !QueryPerformanceCounter(&t) || t.QuadPart<0)return 0;
    value=(uint64_t)t.QuadPart; divisor=(uint64_t)f.QuadPart;
    /* Quotient first: QPC ticks * 1e9 would overflow after a long uptime. */
    return value/divisor*INV_NS_S + (uint64_t)((double)(value%divisor)*(double)INV_NS_S/(double)divisor);
}
uint64_t inv_win_filetime(FILETIME ft) { return ((uint64_t)ft.dwHighDateTime<<32)|ft.dwLowDateTime; }
uint64_t inv_realtime_ns(void) {
    FILETIME ft;
    uint64_t ticks;
    GetSystemTimeAsFileTime(&ft);
    ticks=inv_win_filetime(ft);
    if(ticks<UINT64_C(116444736000000000))return 0;
    ticks-=UINT64_C(116444736000000000);
    return ticks>UINT64_MAX/100?UINT64_MAX:ticks*100;
}
void inv_sleep_ns(uint64_t ns) {
    /* Small chunks keep Ctrl+C and UI cancellation responsive; no timeBeginPeriod. */
    while(ns && !inv_interrupted()) {
        DWORD ms=(DWORD)INV_MIN(ns/INV_NS_MS+(ns%INV_NS_MS!=0),UINT64_C(20));
        uint64_t start=inv_now_ns(),elapsed;
        Sleep(ms); elapsed=inv_now_ns()-start;
        if(!elapsed || elapsed>=ns)break;
        ns-=elapsed;
    }
}
inv_status inv_win_status(DWORD code) {
    switch(code) {
    case ERROR_SUCCESS:return INV_OK;
    case ERROR_ACCESS_DENIED:case ERROR_PRIVILEGE_NOT_HELD:return INV_PERMISSION_DENIED;
    case ERROR_FILE_NOT_FOUND:case ERROR_PATH_NOT_FOUND:case ERROR_NOT_FOUND:
    case ERROR_DEV_NOT_EXIST:case ERROR_NOT_READY:return INV_NOT_PRESENT;
    case ERROR_NOT_SUPPORTED:case ERROR_CALL_NOT_IMPLEMENTED:return INV_UNSUPPORTED;
    case ERROR_NO_DATA:return INV_NO_DATA;
    case ERROR_NOT_ENOUGH_MEMORY:case ERROR_OUTOFMEMORY:return INV_NO_MEMORY;
    case ERROR_INSUFFICIENT_BUFFER:case ERROR_BUFFER_OVERFLOW:case ERROR_MORE_DATA:return INV_LIMIT;
    case ERROR_TIMEOUT:case WAIT_TIMEOUT:return INV_TIMEOUT;
    case ERROR_INVALID_DATA:case ERROR_BAD_FORMAT:return INV_INVALID_DATA;
    case ERROR_OPERATION_ABORTED:return INV_CANCELLED;
    default:return INV_IO_ERROR;
    }
}
void inv_win_error(inv_read_ctx *c,DWORD code,const char *operation) {
    snprintf(c->error,sizeof(c->error),"%s: Windows error %lu",operation,(unsigned long)code);
}
int inv_win_utf8(char *dst,size_t cap,const wchar_t *s) {
    int n;
    if(!cap || cap>(size_t)INT_MAX)return 0;
    dst[0]=0;
    n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,dst,(int)cap,NULL,NULL);
    if(n<=0) { dst[0]=0; return 0; }
    return 1;
}
wchar_t *inv_win_widen(const char *s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    wchar_t *w;
    if(n<=0) { errno=EINVAL; return NULL; }
    w=(wchar_t *)malloc((size_t)n*sizeof(*w));
    if(!w) { errno=ENOMEM; return NULL; }
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)) { free(w); errno=EINVAL; return NULL; }
    return w;
}
FILE *inv_fopen_utf8(const char *path,const char *mode) {
    wchar_t *p=inv_win_widen(path),*m;
    FILE *f;
    if(!p)return NULL;
    m=inv_win_widen(mode);
    if(!m) { free(p); return NULL; }
    f=_wfopen(p,m); free(m); free(p);
    return f;
}
FILE *inv_create_output(const char *path) {
    wchar_t *wide=inv_win_widen(path);
    FILE *f;
    int fd=-1;
    errno_t err;
    if(!wide)return NULL;
    err=_wsopen_s(&fd,wide,_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY|_O_NOINHERIT,
                 _SH_DENYWR,_S_IREAD|_S_IWRITE);
    free(wide);
    if(err || fd<0) { errno=(int)err; return NULL; }
    f=_fdopen(fd,"wb");
    if(!f) { int saved=errno; _close(fd); errno=saved; }
    return f;
}
unsigned long inv_process_id(void) { return (unsigned long)GetCurrentProcessId(); }
int inv_path(char *out,size_t cap,const char *root,const char *rel) {
    int n=snprintf(out,cap,"%s/%s",root,rel);
    return n>=0 && (size_t)n<cap;
}
inv_status inv_read_text(const char *path,char *out,size_t cap) {
    FILE *f;
    size_t n;
    int extra;
    inv_status status=INV_OK;
    if(cap<2)return INV_LIMIT;
    out[0]=0; f=inv_fopen_utf8(path,"rb");
    if(!f)return inv_errno_status(errno);
    n=fread(out,1,cap-1,f); extra=fgetc(f);
    if(ferror(f))status=INV_IO_ERROR;
    else if(extra!=EOF)status=INV_LIMIT;
    else if(memchr(out,0,n))status=INV_INVALID_DATA;
    fclose(f); out[n]=0;
    if(status!=INV_OK)out[0]=0;
    return status;
}
inv_status inv_read_u64(const char *path,uint64_t *v) {
    char s[128]; inv_status st=inv_read_text(path,s,sizeof(s));
    return st!=INV_OK?st:inv_parse_u64(s,v)?INV_OK:INV_INVALID_DATA;
}
int inv_win_system_drive(wchar_t *out,size_t cap) {
    wchar_t directory[MAX_PATH];
    UINT n=GetWindowsDirectoryW(directory,MAX_PATH);
    if(!n || n>=MAX_PATH || cap<4 || directory[1]!=L':')return 0;
    out[0]=directory[0]; out[1]=L':'; out[2]=L'\\'; out[3]=0;
    return 1;
}
