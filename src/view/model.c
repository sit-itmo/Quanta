/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include "inv/view.h"
#include <math.h>
const char *inv_metric_source(const inv_metric_desc *m) {
    return m->provider->id;
}
void inv_model_init(inv_model *m,const inv_plan *p) {
    memset(m,0,sizeof(*m));
    m->history_ns=p->history_ns;
    m->redact=p->redact;
}
void inv_model_destroy(inv_model *m) {
    size_t i;
    for(i=0; i<m->count; i++)free(m->items[i].history);
    free(m->items);
    memset(m,0,sizeof(*m));
}
inv_item *inv_model_find(inv_model *m,uint64_t id) {
    size_t i;
    for(i=0; i<m->count; i++)if(m->items[i].active&&m->items[i].meta.id==id)return &m->items[i];
    return NULL;
}
static inv_status model_begin(void *c,const inv_plan *p) {
    (void)c;
    (void)p;
    return INV_OK;
}
static inv_status model_upsert(void *c,const inv_series *s) {
    inv_model *m=(inv_model *)c;
    inv_item *item=inv_model_find(m,s->id);
    size_t i;
    if(item) {
        item->meta=*s;
        return INV_OK;
    }
    for(i=0; i<m->count; i++)if(!m->items[i].active) {
        item=&m->items[i];
        break;
    }
    if(!item) {
        if(m->count==m->cap) {
            size_t cap=m->cap?m->cap*2:32;
            inv_item *p;
            if(cap>INV_MAX_SERIES)return INV_LIMIT;
            p=(inv_item *)realloc(m->items,cap*sizeof(*p));
            if(!p)return INV_NO_MEMORY;
            memset(p+m->cap,0,(cap-m->cap)*sizeof(*p));
            m->items=p;
            m->cap=cap;
        }
        item=&m->items[m->count++];
    }
    memset(item,0,sizeof(*item));
    item->active=1;
    item->meta=*s;
    item->last.status=INV_NO_DATA;
    if(s->view.widget==INV_WIDGET_PLOT) {
        uint64_t points=m->history_ns/INV_MAX(s->effective_period_ns,INV_NS_MS)+2;
        size_t bytes;
        item->history_cap=(size_t)INV_MIN(points,INV_HISTORY_MAX);
        bytes=item->history_cap*sizeof(inv_point);
        if(bytes>INV_HISTORY_BUDGET-m->history_bytes) {
            item->active=0;
            return INV_LIMIT;
        }
        item->history=(inv_point *)calloc(item->history_cap,sizeof(inv_point));
        if(!item->history) {
            item->active=0;
            return INV_NO_MEMORY;
        }
        m->history_bytes+=bytes;
        if(points>INV_HISTORY_MAX)snprintf(m->diagnostic,sizeof(m->diagnostic),"History capped at %u points per plot; timestamps preserve the retained time range",INV_HISTORY_MAX);
    }
    return INV_OK;
}
static inv_status model_remove(void *c,uint64_t id) {
    inv_model *m=(inv_model *)c;
    inv_item *p=inv_model_find(m,id);
    if(p) {
        m->history_bytes-=p->history_cap*sizeof(inv_point);
        free(p->history);
        memset(p,0,sizeof(*p));
    }
    return INV_OK;
}
static inv_status model_frame(void *c,const inv_frame_info *f) {
    inv_model *m=(inv_model *)c;
    m->frame=*f;
    m->frames++;
    return INV_OK;
}
static inv_status model_sample(void *c,const inv_sample *s) {
    inv_model *m=(inv_model *)c;
    inv_item *p=inv_model_find(m,s->series_id);
    double n=0;
    if(!p)return INV_INVALID_DATA;
    if(p->has_last&&s->sequence<=p->last.sequence)return INV_OK;
    p->last=*s;
    p->has_last=1;
    m->samples++;
    if(s->status==INV_OK) {
        p->good=*s;
        p->has_good=1;
    }
    if(p->history_cap) {
        inv_point point;
        size_t at;
        point.time_ns=s->measured_at_ns;
        point.status=s->status;
        if(s->status==INV_OK&&!inv_value_number(&s->value,&n))point.status=INV_INVALID_DATA;
        point.value=n;
        at=(p->history_head+p->history_count)%p->history_cap;
        p->history[at]=point;
        if(p->history_count<p->history_cap)p->history_count++;
        else p->history_head=(p->history_head+1)%p->history_cap;
        while(p->history_count>1&&point.time_ns-p->history[p->history_head].time_ns>m->history_ns) {
            p->history_head=(p->history_head+1)%p->history_cap;
            p->history_count--;
        }
    }
    return INV_OK;
}
static inv_status model_end_frame(void *c) {
    (void)c;
    return INV_OK;
}
static inv_status model_diag(void *c,inv_status s,const char *text) {
    inv_model *m=(inv_model *)c;
    (void)s;
    inv_safe_text(m->diagnostic,sizeof(m->diagnostic),text);
    return INV_OK;
}
static inv_status model_progress(void *c,uint32_t a,uint32_t b,const char *text) {
    inv_model *m=(inv_model *)c;
    snprintf(m->diagnostic,sizeof(m->diagnostic),"%u/%u %s",a,b,text);
    return INV_OK;
}
static inv_status model_end(void *c,inv_status status) {
    inv_model *m=(inv_model *)c;
    m->done=1;
    m->end_status=status;
    return INV_OK;
}
inv_sink inv_model_sink(inv_model *m) {
    inv_sink s;
    memset(&s,0,sizeof(s));
    s.struct_size=sizeof(s);
    s.api_version=1;
    s.context=m;
    s.begin_session=model_begin;
    s.upsert_series=model_upsert;
    s.remove_series=model_remove;
    s.begin_frame=model_frame;
    s.emit_sample=model_sample;
    s.end_frame=model_end_frame;
    s.emit_diagnostic=model_diag;
    s.emit_progress=model_progress;
    s.end_session=model_end;
    return s;
}
const inv_point *inv_history_at(const inv_item *p,size_t index) {
    if(index>=p->history_count)return NULL;
    return &p->history[(p->history_head+index)%p->history_cap];
}
void inv_item_value(char *out,size_t cap,const inv_item *p,int redact) {
    if(redact&&(p->meta.metric->flags&INV_SENSITIVE)) {
        inv_copy(out,cap,"[redacted]");
        return;
    }
    if(p->has_last&&p->last.status==INV_OK)inv_format_value(out,cap,&p->last.value,p->meta.unit,&p->meta.view,0);
    else if(p->has_good) {
        char b[INV_TEXT_CAP+64];
        inv_format_value(b,sizeof(b),&p->good.value,p->meta.unit,&p->meta.view,0);
        snprintf(out,cap,"%s (last good)",b);
    }
    else inv_copy(out,cap,"--");
}
void inv_item_status(char *out,size_t cap,const inv_item *p,uint64_t now) {
    inv_status s=p->has_last?p->last.status:INV_NO_DATA;
    uint64_t age=p->has_good&&now>p->good.measured_at_ns?now-p->good.measured_at_ns:0;
    int stale=p->has_good&&p->meta.metric->refresh!=INV_REFRESH_ONCE&&age>INV_MAX(p->meta.metric->stale_after_ns,3*p->meta.effective_period_ns);
    if(s!=INV_OK&&p->has_good)snprintf(out,cap,"%s / last good %.1fs ago%s",inv_status_name(s),(double)age/1e9,stale?" / STALE":"");
    else snprintf(out,cap,"%s%s%s",inv_status_name(s),stale?" / STALE":"",p->has_last&&(p->last.quality_flags&INV_QUALITY_ESTIMATED)?" / estimate":"");
}
inv_status inv_model_clone(inv_model *dst,const inv_model *src) {
    size_t i;
    inv_model out=*src;
    out.items=NULL;
    out.cap=src->count;
    if(src->count) {
        out.items=(inv_item *)calloc(src->count,sizeof(inv_item));
        if(!out.items)return INV_NO_MEMORY;
    }
    for(i=0; i<src->count; i++) {
        out.items[i]=src->items[i];
        out.items[i].history=NULL;
        if(src->items[i].history_cap) {
            size_t bytes=src->items[i].history_cap*sizeof(inv_point);
            out.items[i].history=(inv_point *)malloc(bytes);
            if(!out.items[i].history) {
                out.count=i+1;
                inv_model_destroy(&out);
                return INV_NO_MEMORY;
            }
            memcpy(out.items[i].history,src->items[i].history,bytes);
        }
    }
    inv_model_destroy(dst);
    *dst=out;
    return INV_OK;
}
