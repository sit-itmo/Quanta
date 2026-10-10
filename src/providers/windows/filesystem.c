/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
typedef struct filesystem_data { uint64_t total,free,available; inv_status status; } filesystem_data;
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(filesystem_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *s) { return inv_discover_single(c,&inv_filesystem_provider,s); }
static inv_status refresh(inv_read_ctx *c) {
    filesystem_data *p=(filesystem_data *)c->data;
    wchar_t root[4];
    ULARGE_INTEGER total,available;
    if(!inv_win_system_drive(root,INV_ARRAY_SIZE(root))) { p->status=INV_UNSUPPORTED; return INV_OK; }
    if(!GetDiskFreeSpaceExW(root,&available,&total,NULL)) {
        DWORD error=GetLastError(); inv_win_error(c,error,"GetDiskFreeSpaceExW"); p->status=inv_win_status(error); return INV_OK;
    }
    p->total=total.QuadPart; p->free=available.QuadPart; p->available=available.QuadPart;
    p->status=p->total && p->available<=p->total?INV_OK:INV_INVALID_DATA;
    return INV_OK;
}
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
inv_status inv_filesystem_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    filesystem_data *p=(filesystem_data *)c->data; const char *a=in->metric->arg;
    (void)w; (void)q;
    if(p->status!=INV_OK)return p->status;
    if(!strcmp(a,"total_bytes"))v->as.u64=p->total;
    else if(!strcmp(a,"available_bytes"))v->as.u64=p->available;
    else if(!strcmp(a,"used_bytes"))v->as.u64=p->total-p->free;
    else if(!strcmp(a,"utilization"))v->as.f64=(double)(p->total-p->free)/(double)p->total;
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_filesystem_provider={7,"windows.filesystem","Space on the Windows installation volume (never inferred from current directory)",open_provider,discover,refresh,close_provider};
