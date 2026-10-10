/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include "windows_math.h"
#include <psapi.h>
typedef struct memory_data {
    MEMORYSTATUSEX memory;
    PERFORMANCE_INFORMATION performance;
    inv_status memory_status,performance_status;
} memory_data;
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(memory_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *s) { return inv_discover_single(c,&inv_memory_provider,s); }
static inv_status refresh(inv_read_ctx *c) {
    memory_data *p=(memory_data *)c->data;
    memset(&p->memory,0,sizeof(p->memory)); p->memory.dwLength=sizeof(p->memory);
    p->memory_status=GlobalMemoryStatusEx(&p->memory)?INV_OK:inv_win_status(GetLastError());
    memset(&p->performance,0,sizeof(p->performance)); p->performance.cb=sizeof(p->performance);
    p->performance_status=GetPerformanceInfo(&p->performance,sizeof(p->performance))?INV_OK:inv_win_status(GetLastError());
    return INV_OK; /* Different source failures remain per-field, not group-wide. */
}
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
inv_status inv_memory_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    memory_data *p=(memory_data *)c->data;
    const char *a=in->metric->arg;
    uint64_t used,pages;
    double fraction;
    inv_status st;
    (void)w; (void)q;
    if(!strcmp(a,"total")||!strcmp(a,"available")||!strcmp(a,"used")||!strcmp(a,"utilization")) {
        if(p->memory_status!=INV_OK)return p->memory_status;
        st=inv_win_memory_used(p->memory.ullTotalPhys,p->memory.ullAvailPhys,&used,&fraction);
        if(st!=INV_OK)return st;
        if(!strcmp(a,"total"))v->as.u64=p->memory.ullTotalPhys;
        else if(!strcmp(a,"available"))v->as.u64=p->memory.ullAvailPhys;
        else if(!strcmp(a,"used"))v->as.u64=used;
        else v->as.f64=fraction;
        return INV_OK;
    }
    if(p->performance_status!=INV_OK)return p->performance_status;
    if(!strcmp(a,"process_count")) { v->as.u64=p->performance.ProcessCount; return INV_OK; }
    if(!strcmp(a,"thread_count")) { v->as.u64=p->performance.ThreadCount; return INV_OK; }
    if(!strcmp(a,"handle_count")) { v->as.u64=p->performance.HandleCount; return INV_OK; }
    if(!strcmp(a,"commit_used"))pages=p->performance.CommitTotal;
    else if(!strcmp(a,"commit_limit"))pages=p->performance.CommitLimit;
    else if(!strcmp(a,"commit_peak"))pages=p->performance.CommitPeak;
    else if(!strcmp(a,"cache"))pages=p->performance.SystemCache;
    else if(!strcmp(a,"kernel_paged"))pages=p->performance.KernelPaged;
    else if(!strcmp(a,"kernel_nonpaged"))pages=p->performance.KernelNonpaged;
    else return INV_UNSUPPORTED;
    return inv_win_pages_bytes(pages,p->performance.PageSize,&v->as.u64);
}
const inv_provider_desc inv_memory_provider={1,"windows.memory","Physical memory and separate system commit counters",open_provider,discover,refresh,close_provider};
