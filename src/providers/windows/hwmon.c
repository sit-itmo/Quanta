/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include "windows_math.h"
#include "pdh_set.h"
typedef struct thermal_data { inv_pdh_set pdh; inv_status status; } thermal_data;
static inv_status open_provider(inv_read_ctx *c) {
    thermal_data *p=(thermal_data *)calloc(1,sizeof(*p));
    if(!p)return INV_NO_MEMORY;
    c->data=p;
    p->status=inv_pdh_open(&p->pdh,L"\\Thermal Zone Information(*)\\Temperature",0);
    return INV_OK;
}
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    thermal_data *p=(thermal_data *)c->data;
    const inv_metric_desc *metric=inv_metric_find("sensor.temperature");
    size_t i;
    if(!metric || !inv_metric_wanted(c,metric))return INV_OK;
    if(p->pdh.query)p->status=inv_pdh_discover(&p->pdh);
    if(p->status!=INV_OK)return p->status;
    for(i=0;i<p->pdh.count;i++) {
        inv_instance in;
        inv_status st;
        const char *name=p->pdh.items[i].name;
        if(!strcmp(name,"_Total"))continue;
        memset(&in,0,sizeof(in)); in.metric=metric; in.min_poll_ns=INV_NS_S;
        if(!inv_copy(in.resource,sizeof(in.resource),name))return INV_LIMIT;
        if(snprintf(in.entity_key,sizeof(in.entity_key),"thermal:%s",name)>=(int)sizeof(in.entity_key))return INV_LIMIT;
        if(!inv_label_set(in.labels,&in.label_count,"device",name)||
           !inv_label_set(in.labels,&in.label_count,"chip","acpi_thermal_zone")||
           !inv_label_set(in.labels,&in.label_count,"channel","temperature")||
           !inv_label_set(in.labels,&in.label_count,"label",name))return INV_LIMIT;
        st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
    }
    return INV_OK;
}
static inv_status refresh(inv_read_ctx *c) {
    thermal_data *p=(thermal_data *)c->data;
    if(p->pdh.query)return inv_pdh_collect(&p->pdh);
    return p->status;
}
static void close_provider(inv_read_ctx *c) {
    thermal_data *p=(thermal_data *)c->data;
    if(p)inv_pdh_close(&p->pdh);
    free(p); c->data=NULL;
}
inv_status inv_hwmon_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    thermal_data *p=(thermal_data *)c->data;
    const inv_pdh_item *item=inv_pdh_find(&p->pdh,in->resource);
    (void)w; (void)q;
    if(!item)return INV_GONE;
    if(item->status!=INV_OK)return item->status;
    return inv_win_kelvin_celsius(item->value,&v->as.f64);
}
const inv_provider_desc inv_hwmon_provider={5,"windows.thermal","Optional Windows thermal-zone counters; no hardware driver and no CPU-package inference",open_provider,discover,refresh,close_provider};
