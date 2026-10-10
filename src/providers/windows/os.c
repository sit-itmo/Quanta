/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
typedef LONG (WINAPI *rtl_version_fn)(OSVERSIONINFOEXW *);
typedef struct os_data {
    OSVERSIONINFOEXW version;
    char product[INV_TEXT_CAP],build[INV_TEXT_CAP],hostname[INV_TEXT_CAP],arch[32];
    inv_status version_status,hostname_status;
    uint64_t uptime_ms,online;
} os_data;
static int registry_string(HKEY key,const wchar_t *name,char *out,size_t cap) {
    wchar_t text[512];
    DWORD type=0,size=sizeof(text);
    LSTATUS status=RegQueryValueExW(key,name,NULL,&type,(BYTE *)text,&size);
    if(status!=ERROR_SUCCESS || type!=REG_SZ || size<2 || size>sizeof(text) || size%sizeof(wchar_t))return 0;
    text[INV_ARRAY_SIZE(text)-1]=0;
    if(!wmemchr(text,0,size/sizeof(wchar_t)))return 0;
    return inv_win_utf8(out,cap,text);
}
static inv_status refresh(inv_read_ctx *c) {
    os_data *p=(os_data *)c->data;
    SYSTEM_INFO system;
    HKEY key;
    wchar_t host[256];
    DWORD size=(DWORD)INV_ARRAY_SIZE(host);
    HMODULE module=GetModuleHandleW(L"ntdll.dll");
    FARPROC entry=module?GetProcAddress(module,"RtlGetVersion"):NULL;
    rtl_version_fn version=NULL;
    p->version_status=INV_UNSUPPORTED;
    memset(&p->version,0,sizeof(p->version));
    p->version.dwOSVersionInfoSize=sizeof(p->version);
    /* Copy between function-pointer representations without a GCC cast warning. */
    if(entry)memcpy(&version,&entry,sizeof(version));
    if(version && version(&p->version)==0)p->version_status=INV_OK;
    p->product[0]=0; p->build[0]=0;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",0,KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key)==ERROR_SUCCESS) {
        (void)registry_string(key,L"ProductName",p->product,sizeof(p->product));
        (void)registry_string(key,L"BuildLabEx",p->build,sizeof(p->build));
        RegCloseKey(key);
    }
    if(!p->product[0])inv_copy(p->product,sizeof(p->product),"Microsoft Windows");
    if(GetComputerNameExW(ComputerNameDnsHostname,host,&size))
        p->hostname_status=inv_win_utf8(p->hostname,sizeof(p->hostname),host)?INV_OK:INV_LIMIT;
    else p->hostname_status=inv_win_status(GetLastError());
    GetNativeSystemInfo(&system);
    switch(system.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:inv_copy(p->arch,sizeof(p->arch),"x86_64");break;
    case PROCESSOR_ARCHITECTURE_INTEL:inv_copy(p->arch,sizeof(p->arch),"x86");break;
    case PROCESSOR_ARCHITECTURE_ARM64:inv_copy(p->arch,sizeof(p->arch),"aarch64");break;
    case PROCESSOR_ARCHITECTURE_ARM:inv_copy(p->arch,sizeof(p->arch),"arm");break;
    default:snprintf(p->arch,sizeof(p->arch),"arch-%u",(unsigned)system.wProcessorArchitecture);break;
    }
    p->uptime_ms=GetTickCount64();
    p->online=GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return INV_OK;
}
static inv_status open_provider(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(os_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *sink) { return inv_discover_single(c,&inv_os_provider,sink); }
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
inv_status inv_os_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *window,uint32_t *quality) {
    os_data *p=(os_data *)c->data;
    const char *arg=in->metric->arg;
    (void)window; (void)quality;
    if(!strcmp(arg,"name"))return inv_copy(v->as.string,sizeof(v->as.string),p->product)?INV_OK:INV_LIMIT;
    if(!strcmp(arg,"arch"))return inv_copy(v->as.string,sizeof(v->as.string),p->arch)?INV_OK:INV_LIMIT;
    if(!strcmp(arg,"hostname")) {
        if(p->hostname_status!=INV_OK)return p->hostname_status;
        return inv_copy(v->as.string,sizeof(v->as.string),p->hostname)?INV_OK:INV_LIMIT;
    }
    if(!strcmp(arg,"uptime")) { v->as.f64=(double)p->uptime_ms/1000.0; return INV_OK; }
    if(!strcmp(arg,"online")) { v->as.u64=p->online; return p->online?INV_OK:INV_NO_DATA; }
    if(p->version_status!=INV_OK)return p->version_status;
    if(!strcmp(arg,"release"))snprintf(v->as.string,sizeof(v->as.string),"%lu.%lu.%lu",
        (unsigned long)p->version.dwMajorVersion,(unsigned long)p->version.dwMinorVersion,(unsigned long)p->version.dwBuildNumber);
    else if(!strcmp(arg,"version")) {
        if(p->build[0])return inv_copy(v->as.string,sizeof(v->as.string),p->build)?INV_OK:INV_LIMIT;
        snprintf(v->as.string,sizeof(v->as.string),"NT build %lu; service pack %u.%u",
          (unsigned long)p->version.dwBuildNumber,(unsigned)p->version.wServicePackMajor,(unsigned)p->version.wServicePackMinor);
    } else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_os_provider={0,"windows.system","Windows system identity and uptime",open_provider,discover,refresh,close_provider};
