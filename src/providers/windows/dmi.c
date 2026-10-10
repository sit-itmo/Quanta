/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include "smbios.h"
typedef struct dmi_data { inv_smbios table; inv_status status; } dmi_data;
static inv_status refresh(inv_read_ctx *c) {
    dmi_data *p=(dmi_data *)c->data;
    UINT size=0,got=0;
    unsigned retry;
    const DWORD signature=UINT32_C(0x52534d42); /* 'RSMB', without implementation-defined multichar literals */
    uint8_t *bytes=NULL;
    p->status=INV_NO_DATA;
    for(retry=0;retry<3;retry++) {
        size=GetSystemFirmwareTable(signature,0,NULL,0);
        if(!size) { DWORD err=GetLastError(); p->status=err?inv_win_status(err):INV_NO_DATA; break; }
        if(size>16u*1024u*1024u) { p->status=INV_LIMIT; break; }
        bytes=(uint8_t *)malloc(size);
        if(!bytes) { p->status=INV_NO_MEMORY; break; }
        got=GetSystemFirmwareTable(signature,0,bytes,size);
        if(!got) { p->status=inv_win_status(GetLastError()); if(p->status==INV_OK)p->status=INV_NO_DATA; }
        else if(got<=size)p->status=inv_smbios_parse_windows(bytes,got,&p->table);
        free(bytes); bytes=NULL;
        if(got<=size)break;
        p->status=INV_LIMIT;
    }
    return INV_OK;
}
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(dmi_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    dmi_data *p=(dmi_data *)c->data;
    const inv_metric_desc *const *registry;
    size_t count,i,j;
    (void)refresh(c);
    registry=inv_registry(&count);
    for(i=0;i<count;i++)if(registry[i]->provider==&inv_dmi_provider && inv_metric_wanted(c,registry[i])) {
        const inv_metric_desc *m=registry[i];
        inv_instance in;
        inv_status st;
        memset(&in,0,sizeof(in)); in.metric=m;
        if(strncmp(m->arg,"dimm_",5)) {
            inv_copy(in.entity_key,sizeof(in.entity_key),"system");
            st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
        } else if(p->status==INV_OK) {
            for(j=0;j<p->table.dimm_count;j++) {
                inv_dmi_dimm *d=&p->table.dimms[j];
                char handle[32];
                memset(&in,0,sizeof(in)); in.metric=m;
                snprintf(in.entity_key,sizeof(in.entity_key),"smbios:memory:%04x",(unsigned)d->handle);
                snprintf(in.resource,sizeof(in.resource),"%u",(unsigned)d->handle);
                snprintf(handle,sizeof(handle),"0x%04x",(unsigned)d->handle);
                if(!inv_label_set(in.labels,&in.label_count,"handle",handle))return INV_LIMIT;
                if(d->locator.status==INV_OK && !inv_label_set(in.labels,&in.label_count,"locator",d->locator.text))return INV_LIMIT;
                if(d->bank.status==INV_OK && !inv_label_set(in.labels,&in.label_count,"bank",d->bank.text))return INV_LIMIT;
                st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
            }
        }
    }
    return p->status; /* Failed discovery must not be mistaken for unplugged DIMMs. */
}
static void close_provider(inv_read_ctx *c) { free(c->data); c->data=NULL; }
static inv_status copy_field(const inv_dmi_string *s,inv_value *v) {
    if(s->status!=INV_OK)return s->status;
    return inv_copy(v->as.string,sizeof(v->as.string),s->text)?INV_OK:INV_LIMIT;
}
inv_status inv_dmi_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    dmi_data *p=(dmi_data *)c->data;
    const char *a=in->metric->arg;
    static const char *const fields[]={"bios_vendor","bios_version","bios_date","board_vendor","board_name","board_serial","system_vendor","system_model","system_serial"};
    size_t i;
    uint64_t handle;
    (void)w; (void)q;
    if(p->status!=INV_OK)return p->status;
    for(i=0;i<INV_ARRAY_SIZE(fields);i++)if(!strcmp(a,fields[i]))return copy_field(&p->table.fields[i],v);
    if(!strcmp(a,"system_uuid"))return copy_field(&p->table.uuid,v);
    if(strncmp(a,"dimm_",5) || !inv_parse_u64(in->resource,&handle))return INV_UNSUPPORTED;
    for(i=0;i<p->table.dimm_count;i++)if(p->table.dimms[i].handle==handle) {
        inv_dmi_dimm *d=&p->table.dimms[i];
        if(!strcmp(a,"dimm_size")) { v->as.u64=d->size_bytes; return d->size_status; }
        if(!strcmp(a,"dimm_speed")) { v->as.u64=d->speed_mt_s; return d->speed_status; }
        if(!strcmp(a,"dimm_vendor"))return copy_field(&d->vendor,v);
        if(!strcmp(a,"dimm_serial"))return copy_field(&d->serial,v);
        if(!strcmp(a,"dimm_part"))return copy_field(&d->part,v);
        return INV_UNSUPPORTED;
    }
    return INV_GONE;
}
const inv_provider_desc inv_dmi_provider={3,"windows.smbios","Read-only raw SMBIOS: system, baseboard, firmware, memory modules",open_provider,discover,refresh,close_provider};
