/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include <psapi.h>
typedef struct process_data {
    PROCESS_MEMORY_COUNTERS_EX memory;
    IO_COUNTERS io;
    DWORD handles;
    inv_status memory_status,io_status,handles_status;
} process_data;
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(process_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *s) { return inv_discover_single(c,&inv_process_provider,s); }
static inv_status refresh(inv_read_ctx *c) {
    process_data *p=(process_data *)c->data;
    HANDLE self=GetCurrentProcess();
    memset(&p->memory,0,sizeof(p->memory)); p->memory.cb=sizeof(p->memory);
    p->memory_status=GetProcessMemoryInfo(self,(PROCESS_MEMORY_COUNTERS *)&p->memory,sizeof(p->memory))?INV_OK:inv_win_status(GetLastError());
    p->io_status=GetProcessIoCounters(self,&p->io)?INV_OK:inv_win_status(GetLastError());
    p->handles_status=GetProcessHandleCount(self,&p->handles)?INV_OK:inv_win_status(GetLastError());
    return INV_OK;
}
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
inv_status inv_process_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    process_data *p=(process_data *)c->data; const char *a=in->metric->arg;
    (void)w; (void)q;
    if(!strcmp(a,"pid")) { v->as.u64=GetCurrentProcessId(); return INV_OK; }
    if(!strcmp(a,"handles")) { v->as.u64=p->handles; return p->handles_status; }
    if(!strcmp(a,"rss") || !strcmp(a,"private")) {
        v->as.u64=!strcmp(a,"rss")?p->memory.WorkingSetSize:p->memory.PrivateUsage;
        return p->memory_status;
    }
    if(!strcmp(a,"read_bytes"))v->as.u64=p->io.ReadTransferCount;
    else if(!strcmp(a,"write_bytes"))v->as.u64=p->io.WriteTransferCount;
    else return INV_UNSUPPORTED;
    return p->io_status;
}
const inv_provider_desc inv_process_provider={8,"windows.process","Self working set, private commit, handles and I/O counters",open_provider,discover,refresh,close_provider};
