/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
typedef struct cpuid_data {
    char vendor[16],brand[64];
    uint32_t ref,ecx1,edx1,ebx7,ecxext;
    inv_status status;
} cpuid_data;
static void cpuid(uint32_t leaf,uint32_t sub,uint32_t out[4]) {
#ifdef _MSC_VER
    int data[4];
    __cpuidex(data,(int)leaf,(int)sub);
    memcpy(out,data,sizeof(data));
#else
    __cpuid_count(leaf,sub,out[0],out[1],out[2],out[3]);
#endif
}
static inv_status refresh(inv_read_ctx *c) {
    cpuid_data *p=(cpuid_data *)c->data;
    GROUP_AFFINITY original,one;
    DWORD_PTR mask;
    uint32_t out[4],max,ext;
    unsigned bit=0,i;
    memset(p,0,sizeof(*p)); p->status=INV_UNSUPPORTED;
    if(!GetThreadGroupAffinity(GetCurrentThread(),&original)) {
        p->status=inv_win_status(GetLastError()); return INV_OK;
    }
    mask=original.Mask;
    while(bit<sizeof(mask)*CHAR_BIT && !(mask&((DWORD_PTR)1<<bit)))bit++;
    if(bit==sizeof(mask)*CHAR_BIT)return INV_OK;
    memset(&one,0,sizeof(one)); one.Group=original.Group; one.Mask=(DWORD_PTR)1<<bit;
    if(!SetThreadGroupAffinity(GetCurrentThread(),&one,NULL)) {
        p->status=inv_win_status(GetLastError()); return INV_OK;
    }
    p->ref=((uint32_t)original.Group<<16)|bit;
    cpuid(0,0,out); max=out[0];
    memcpy(p->vendor,out+1,4); memcpy(p->vendor+4,out+3,4); memcpy(p->vendor+8,out+2,4);
    if(max>=1) { cpuid(1,0,out); p->ecx1=out[2]; p->edx1=out[3]; }
    if(max>=7) { cpuid(7,0,out); p->ebx7=out[1]; }
    cpuid(UINT32_C(0x80000000),0,out); ext=out[0];
    if(ext>=UINT32_C(0x80000001)) { cpuid(UINT32_C(0x80000001),0,out); p->ecxext=out[2]; }
    if(ext>=UINT32_C(0x80000004)) {
        for(i=0;i<3;i++) { cpuid(UINT32_C(0x80000002)+i,0,out); memcpy(p->brand+i*16,out,16); }
        inv_trim(p->brand);
    }
    p->status=INV_OK;
    if(!SetThreadGroupAffinity(GetCurrentThread(),&original,NULL)) {
        p->status=INV_IO_ERROR;
        inv_win_error(c,GetLastError(),"restore CPUID thread affinity");
    }
    return INV_OK;
}
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(cpuid_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *s) { return inv_discover_single(c,&inv_cpuid_provider,s); }
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
inv_status inv_cpuid_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    cpuid_data *p=(cpuid_data *)c->data;
    const char *a=in->metric->arg;
    (void)w; (void)q;
    if(p->status!=INV_OK)return p->status;
    if(!strcmp(a,"vendor"))return inv_copy(v->as.string,sizeof(v->as.string),p->vendor)?INV_OK:INV_LIMIT;
    if(!strcmp(a,"brand")) {
        if(!p->brand[0])return INV_NO_DATA;
        return inv_copy(v->as.string,sizeof(v->as.string),p->brand)?INV_OK:INV_LIMIT;
    }
    if(!strcmp(a,"cpuid_reference")) { v->as.u64=p->ref; return INV_OK; }
    if(!strcmp(a,"vmx"))v->as.boolean=!!(p->ecx1&(UINT32_C(1)<<5));
    else if(!strcmp(a,"svm"))v->as.boolean=!!(p->ecxext&(UINT32_C(1)<<2));
    else if(!strcmp(a,"sse2"))v->as.boolean=!!(p->edx1&(UINT32_C(1)<<26));
    else if(!strcmp(a,"avx"))v->as.boolean=!!(p->ecx1&(UINT32_C(1)<<28));
    else if(!strcmp(a,"avx2"))v->as.boolean=!!(p->ebx7&(UINT32_C(1)<<5));
    else if(!strcmp(a,"hypervisor"))v->as.boolean=!!(p->ecx1&(UINT32_C(1)<<31));
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_cpuid_provider={6,"windows.cpuid","Direct CPUID on a group-affinity pinned reference CPU",open_provider,discover,refresh,close_provider};
