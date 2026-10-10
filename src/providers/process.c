/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <unistd.h>
#include <ctype.h>
static inv_status proc_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_process_provider,s);
}
inv_status inv_process_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    char p[INV_PATH_CAP],b[256];
    uint64_t resident;
    char *cursor,*end;
    int field;
    long page=sysconf(_SC_PAGESIZE);
    inv_status s;
    (void)in;
    (void)w;
    (void)q;
    inv_path(p,sizeof(p),c->paths.proc_root,"self/statm");
    s=inv_read_text(p,b,sizeof(b));
    if(s!=INV_OK)return s;
    cursor=b;
    resident=0;
    for(field=0; field<2; field++) {
        while(isspace((unsigned char)*cursor))cursor++;
        if(!isdigit((unsigned char)*cursor))return INV_INVALID_DATA;
        errno=0;
        resident=(uint64_t)strtoull(cursor,&end,10);
        if(errno||end==cursor||(*end&&!isspace((unsigned char)*end)))return INV_INVALID_DATA;
        cursor=end;
    }
    if(page<=0||resident>UINT64_MAX/(uint64_t)page)return INV_INVALID_DATA;
    *q|=INV_QUALITY_ESTIMATED;
    v->as.u64=resident*(uint64_t)page;
    return INV_OK;
}
const inv_provider_desc inv_process_provider= {
    8,"linux.self","/proc/self/statm resident pages (approximate)",NULL,proc_discover,NULL,NULL
};
