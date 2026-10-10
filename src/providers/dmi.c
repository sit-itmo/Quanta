/* SPDX-License-Identifier: MIT */
#include "internal.h"
static inv_status dmi_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_dmi_provider,s);
}
inv_status inv_dmi_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    char rel[256],p[INV_PATH_CAP];
    inv_status s;
    (void)w;
    (void)q;
    snprintf(rel,sizeof(rel),"class/dmi/id/%s",in->metric->arg);
    if(!inv_path(p,sizeof(p),c->paths.sys_root,rel))return INV_LIMIT;
    s=inv_read_text(p,v->as.string,sizeof(v->as.string));
    if(s!=INV_OK)return s;
    inv_trim(v->as.string);
    return v->as.string[0]?INV_OK:INV_NO_DATA;
}
const inv_provider_desc inv_dmi_provider= {
    3,"linux.dmi","/sys/class/dmi/id; read-only; never elevates privileges",NULL,dmi_discover,NULL,NULL
};
