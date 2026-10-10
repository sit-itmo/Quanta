/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <sys/statvfs.h>
typedef struct fs_data {
    uint64_t total,free,available;
} fs_data;
static inv_status fs_open(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(fs_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status fs_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_filesystem_provider,s);
}
static inv_status fs_refresh(inv_read_ctx *c) {
    struct statvfs st;
    fs_data *d=(fs_data *)c->data;
    uint64_t size;
    if(statvfs("/",&st))return inv_errno_status(errno);
    size=(uint64_t)(st.f_frsize?st.f_frsize:st.f_bsize);
    if(!size||(uint64_t)st.f_blocks>UINT64_MAX/size||(uint64_t)st.f_bavail>(uint64_t)st.f_blocks||st.f_bfree>st.f_blocks)return INV_INVALID_DATA;
    d->total=(uint64_t)st.f_blocks*size;
    d->free=(uint64_t)st.f_bfree*size;
    d->available=(uint64_t)st.f_bavail*size;
    return INV_OK;
}
static void fs_close(inv_read_ctx *c) {
    free(c->data);
    c->data=NULL;
}
inv_status inv_filesystem_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    fs_data *d=(fs_data *)c->data;
    const char *a=in->metric->arg;
    (void)w;
    (void)q;
    if(!strcmp(a,"total_bytes"))v->as.u64=d->total;
    else if(!strcmp(a,"available_bytes"))v->as.u64=d->available;
    else if(!strcmp(a,"used_bytes"))v->as.u64=d->total-d->free;
    else if(!strcmp(a,"utilization")) {
        if(!d->total)return INV_NO_DATA;
        v->as.f64=(double)(d->total-d->free)/(double)d->total;
    }
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_filesystem_provider= {
    7,"linux.statvfs","statvfs('/') in the collector's mount namespace",fs_open,fs_discover,fs_refresh,fs_close
};
