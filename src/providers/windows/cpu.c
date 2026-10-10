/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include "windows_math.h"
#include "pdh_set.h"
typedef struct cpu_data {
    inv_pdh_set pdh;
    inv_status pdh_status,times_status;
    uint64_t total,idle,times_ns;
    WORD groups;
    int use_pdh;
} cpu_data;
static inv_status open_provider(inv_read_ctx *c) {
    cpu_data *p=(cpu_data *)calloc(1,sizeof(*p));
    size_t count,i;
    const inv_metric_desc *const *registry=inv_registry(&count);
    if(!p)return INV_NO_MEMORY;
    c->data=p; p->groups=GetActiveProcessorGroupCount();
    p->use_pdh=p->groups>1;
    for(i=0;i<count;i++)if(!strcmp(registry[i]->key,"cpu.core.utilization") && inv_metric_wanted(c,registry[i]))p->use_pdh=1;
    p->pdh_status=INV_UNSUPPORTED;
    if(p->use_pdh)p->pdh_status=inv_pdh_open(&p->pdh,L"\\Processor Information(*)\\% Processor Time",1);
    return INV_OK;
}
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    cpu_data *p=(cpu_data *)c->data;
    size_t count,i,j;
    const inv_metric_desc *const *registry=inv_registry(&count);
    if(!p->use_pdh && GetActiveProcessorGroupCount()>1) {
        p->use_pdh=1;
        p->pdh_status=inv_pdh_open(&p->pdh,L"\\Processor Information(*)\\% Processor Time",1);
    }
    if(p->use_pdh && p->pdh.query)p->pdh_status=inv_pdh_discover(&p->pdh);
    for(i=0;i<count;i++)if(registry[i]->provider==&inv_cpu_provider && inv_metric_wanted(c,registry[i])) {
        inv_instance in;
        inv_status st;
        memset(&in,0,sizeof(in)); in.metric=registry[i];
        if(strncmp(registry[i]->key,"cpu.core.",9)) {
            inv_copy(in.entity_key,sizeof(in.entity_key),"system");
            st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
        } else if(p->pdh_status==INV_OK) {
            for(j=0;j<p->pdh.count;j++) {
                unsigned group=0,number=0;
                char g[32],n[32];
                const inv_pdh_item *item=&p->pdh.items[j];
                if(inv_win_cpu_instance(item->name,&group,&number)!=1)continue;
                memset(&in,0,sizeof(in)); in.metric=registry[i];
                snprintf(in.entity_key,sizeof(in.entity_key),"cpu:%u:%u",group,number);
                if(!inv_copy(in.resource,sizeof(in.resource),item->name))return INV_LIMIT;
                snprintf(g,sizeof(g),"%u",group); snprintf(n,sizeof(n),"%u",number);
                if(!inv_label_set(in.labels,&in.label_count,"cpu",item->name)||
                   !inv_label_set(in.labels,&in.label_count,"group",g)||
                   !inv_label_set(in.labels,&in.label_count,"number",n))return INV_LIMIT;
                st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
            }
        }
    }
    return p->use_pdh?p->pdh_status:INV_OK;
}
static inv_status refresh(inv_read_ctx *c) {
    cpu_data *p=(cpu_data *)c->data;
    FILETIME idle,kernel,user;
    p->groups=GetActiveProcessorGroupCount();
    if(p->groups>1)p->times_status=INV_UNSUPPORTED;
    else if(GetSystemTimes(&idle,&kernel,&user)) {
        uint64_t k=inv_win_filetime(kernel),u=inv_win_filetime(user);
        p->idle=inv_win_filetime(idle); p->times_ns=inv_now_ns();
        p->times_status=k<=UINT64_MAX-u?INV_OK:INV_INVALID_DATA;
        p->total=k+u;
    } else p->times_status=inv_win_status(GetLastError());
    if(p->use_pdh && p->pdh.query)(void)inv_pdh_collect(&p->pdh);
    return INV_OK;
}
static void close_provider(inv_read_ctx *c) {
    cpu_data *p=(cpu_data *)c->data;
    if(p)inv_pdh_close(&p->pdh);
    free(p); c->data=NULL;
}
static inv_status pdh_value(const cpu_data *p,const char *name,inv_value *v,uint64_t *w) {
    const inv_pdh_item *item=inv_pdh_find(&p->pdh,name);
    if(!item)return p->pdh_status==INV_OK?INV_GONE:p->pdh_status;
    if(item->status!=INV_OK)return item->status;
    if(item->value<0 || item->value>100.000001)return INV_INVALID_DATA;
    v->as.f64=INV_MIN(item->value/100.0,1.0); *w=item->window_ns;
    return INV_OK;
}
inv_status inv_cpu_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    cpu_data *p=(cpu_data *)c->data;
    const char *a=in->metric->arg;
    inv_status st;
    (void)q;
    if(!strcmp(a,"tick_frequency")) { v->as.u64=UINT64_C(10000000); return INV_OK; }
    if(in->resource[0])return pdh_value(p,in->resource,v,w);
    if(!strcmp(a,"utilization") && p->groups>1){in->reader_state[0]=0;return pdh_value(p,"_Total",v,w);}
    if(in->reader_state[4]!=p->groups){in->reader_state[0]=0;in->reader_state[4]=p->groups;}
    if(p->times_status!=INV_OK) { in->reader_state[0]=0; return p->times_status; }
    if(!strcmp(a,"total")) { v->as.u64=p->total; return INV_OK; }
    if(!strcmp(a,"idle")) { v->as.u64=p->idle; return INV_OK; }
    if(strcmp(a,"utilization"))return INV_UNSUPPORTED;
    st=in->reader_state[0]?inv_win_cpu_fraction(in->reader_state[1],in->reader_state[2],p->total,p->idle,&v->as.f64):INV_WARMUP;
    if(st==INV_OK && p->times_ns>in->reader_state[3])*w=p->times_ns-in->reader_state[3];
    in->reader_state[0]=1; in->reader_state[1]=p->total; in->reader_state[2]=p->idle; in->reader_state[3]=p->times_ns;
    return st;
}
const inv_provider_desc inv_cpu_provider={2,"windows.cpu","GetSystemTimes aggregate and processor-group-aware PDH core utilization",open_provider,discover,refresh,close_provider};
