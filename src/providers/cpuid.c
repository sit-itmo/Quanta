/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <cpuid.h>
#include <sched.h>
typedef struct cpuid_data {
    char vendor[16],brand[64];
    unsigned ref,ecx1,edx1,ebx7,ecxext;
    inv_status status;
} cpuid_data;
static inv_status cpuid_open(inv_read_ctx *c) {
    cpu_set_t original,one;
    unsigned cpu,a,b,cc,d,i;
    cpuid_data *p=(cpuid_data *)calloc(1,sizeof(*p));
    if(!p)return INV_NO_MEMORY;
    c->data=p;
    p->status=INV_UNSUPPORTED;
    if(sched_getaffinity(0,sizeof(original),&original))return INV_OK;
    for(cpu=0; cpu<CPU_SETSIZE; cpu++)if(CPU_ISSET(cpu,&original))break;
    if(cpu==CPU_SETSIZE)return INV_OK;
    CPU_ZERO(&one);
    CPU_SET(cpu,&one);
    if(sched_setaffinity(0,sizeof(one),&one)) {
        p->status=inv_errno_status(errno);
        return INV_OK;
    }
    p->ref=cpu;
    if(__get_cpuid(0,&a,&b,&cc,&d)) {
        memcpy(p->vendor,&b,4);
        memcpy(p->vendor+4,&d,4);
        memcpy(p->vendor+8,&cc,4);
        p->status=INV_OK;
        if(__get_cpuid(1,&a,&b,&cc,&d)) {
            p->ecx1=cc;
            p->edx1=d;
        }
        if(__get_cpuid_count(7,0,&a,&b,&cc,&d))p->ebx7=b;
        if(__get_cpuid(0x80000001u,&a,&b,&cc,&d))p->ecxext=cc;
        if(__get_cpuid_max(0x80000000u,NULL)>=0x80000004u) {
            for(i=0; i<3; i++) {
                __get_cpuid(0x80000002u+i,&a,&b,&cc,&d);
                memcpy(p->brand+i*16,&a,4);
                memcpy(p->brand+i*16+4,&b,4);
                memcpy(p->brand+i*16+8,&cc,4);
                memcpy(p->brand+i*16+12,&d,4);
            }
            inv_trim(p->brand);
        }
    }
    if(sched_setaffinity(0,sizeof(original),&original))p->status=INV_IO_ERROR;
    return INV_OK;
}
static inv_status cpuid_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_cpuid_provider,s);
}
static void cpuid_close(inv_read_ctx *c) {
    free(c->data);
    c->data=NULL;
}
inv_status inv_cpuid_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    cpuid_data *p=(cpuid_data *)c->data;
    const char *a=in->metric->arg;
    (void)w;
    (void)q;
    if(p->status!=INV_OK)return p->status;
    if(!strcmp(a,"vendor"))return inv_copy(v->as.string,sizeof(v->as.string),p->vendor)?INV_OK:INV_LIMIT;
    if(!strcmp(a,"brand")) {
        if(!p->brand[0])return INV_NO_DATA;
        return inv_copy(v->as.string,sizeof(v->as.string),p->brand)?INV_OK:INV_LIMIT;
    }
    if(!strcmp(a,"cpuid_reference")) {
        v->as.u64=p->ref;
        return INV_OK;
    }
    if(!strcmp(a,"vmx"))v->as.boolean=!!(p->ecx1&(1u<<5));
    else if(!strcmp(a,"svm"))v->as.boolean=!!(p->ecxext&(1u<<2));
    else if(!strcmp(a,"sse2"))v->as.boolean=!!(p->edx1&(1u<<26));
    else if(!strcmp(a,"avx"))v->as.boolean=!!(p->ecx1&(1u<<28));
    else if(!strcmp(a,"avx2"))v->as.boolean=!!(p->ebx7&(1u<<5));
    else if(!strcmp(a,"hypervisor"))v->as.boolean=!!(p->ecx1&(1u<<31));
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_cpuid_provider= {
    6,"x86.cpuid","Direct CPUID on a pinned, affinity-allowed reference CPU",cpuid_open,cpuid_discover,NULL,cpuid_close
};
