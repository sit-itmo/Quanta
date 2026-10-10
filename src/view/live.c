/* SPDX-License-Identifier: MIT
* Bounded, lossless event queue. Backpressure delays acquisition rather than
* inventing samples or dropping metadata. Only the UI thread calls its sink.
*/
#include "internal.h"
#include "inv/view.h"
#define QUEUE_CAP 256
typedef enum event_kind {
    E_BEGIN,E_UPSERT,E_REMOVE,E_FRAME,E_SAMPLE,E_END_FRAME,E_DIAG,E_PROGRESS,E_END
} event_kind;
typedef struct event {
    event_kind kind;
    union {
        inv_series series;
        inv_sample sample;
        inv_frame_info frame;
        uint64_t id;
        struct {
            inv_status status;
            char text[256];
            uint32_t index,total;
        }
        diag;
    }
    u;
} event;
struct inv_live {
    inv_engine *engine;
    const inv_plan *plan;
    inv_thread thread;
    inv_mutex mutex;
    inv_condition space;
    event *events;
    size_t head,count;
    int closed,done;
    inv_status result;
};
static inv_status push(inv_live *l,const event *e) {
    inv_mutex_lock(&l->mutex);
    while(l->count==QUEUE_CAP&&!l->closed)inv_condition_wait(&l->space,&l->mutex);
    if(l->closed) {
        inv_mutex_unlock(&l->mutex);
        return INV_CANCELLED;
    }
    l->events[(l->head+l->count)%QUEUE_CAP]=*e;
    l->count++;
    inv_mutex_unlock(&l->mutex);
    return INV_OK;
}
static inv_status qbegin(void *c,const inv_plan *p) {
    event e;
    memset(&e,0,sizeof(e));
    (void)p;
    e.kind=E_BEGIN;
    return push((inv_live *)c,&e);
}
static inv_status qupsert(void *c,const inv_series *s) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_UPSERT;
    e.u.series=*s;
    return push((inv_live *)c,&e);
}
static inv_status qremove(void *c,uint64_t id) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_REMOVE;
    e.u.id=id;
    return push((inv_live *)c,&e);
}
static inv_status qframe(void *c,const inv_frame_info *f) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_FRAME;
    e.u.frame=*f;
    return push((inv_live *)c,&e);
}
static inv_status qsample(void *c,const inv_sample *s) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_SAMPLE;
    e.u.sample=*s;
    return push((inv_live *)c,&e);
}
static inv_status qendframe(void *c) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_END_FRAME;
    return push((inv_live *)c,&e);
}
static inv_status qdiag(void *c,inv_status s,const char *text) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_DIAG;
    e.u.diag.status=s;
    inv_safe_text(e.u.diag.text,sizeof(e.u.diag.text),text);
    return push((inv_live *)c,&e);
}
static inv_status qprogress(void *c,uint32_t i,uint32_t n,const char *text) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_PROGRESS;
    e.u.diag.index=i;
    e.u.diag.total=n;
    inv_safe_text(e.u.diag.text,sizeof(e.u.diag.text),text);
    return push((inv_live *)c,&e);
}
static inv_status qend(void *c,inv_status s) {
    event e;
    memset(&e,0,sizeof(e));
    e.kind=E_END;
    e.u.diag.status=s;
    return push((inv_live *)c,&e);
}
static void *worker(void *p) {
    inv_live *l=(inv_live *)p;
    inv_status result;
    inv_sink s;
    memset(&s,0,sizeof(s));
    s.struct_size=sizeof(s);
    s.api_version=1;
    s.context=l;
    s.begin_session=qbegin;
    s.upsert_series=qupsert;
    s.remove_series=qremove;
    s.begin_frame=qframe;
    s.emit_sample=qsample;
    s.end_frame=qendframe;
    s.emit_diagnostic=qdiag;
    s.emit_progress=qprogress;
    s.end_session=qend;
    result=inv_engine_run(l->engine,&s);
    inv_mutex_lock(&l->mutex);
    l->result=result;
    l->done=1;
    inv_mutex_unlock(&l->mutex);
    return NULL;
}
inv_status inv_live_start(inv_engine *e,const inv_plan *p,inv_live **out) {
    inv_live *l=(inv_live *)calloc(1,sizeof(*l));
    *out=NULL;
    if(!l)return INV_NO_MEMORY;
    l->engine=e;
    l->plan=p;
    l->events=(event *)calloc(QUEUE_CAP,sizeof(event));
    if(!l->events) {
        free(l);
        return INV_NO_MEMORY;
    }
    if(inv_mutex_init(&l->mutex)) {
        free(l->events);
        free(l);
        return INV_IO_ERROR;
    }
    if(inv_condition_init(&l->space)) {
        inv_mutex_destroy(&l->mutex);
        free(l->events);
        free(l);
        return INV_IO_ERROR;
    }
    if(inv_thread_start(&l->thread,worker,l)) {
        inv_condition_destroy(&l->space);
        inv_mutex_destroy(&l->mutex);
        free(l->events);
        free(l);
        return INV_IO_ERROR;
    }
    *out=l;
    return INV_OK;
}
inv_status inv_live_pump(inv_live *l,inv_sink *s) {
    size_t processed=0;
    while(processed++<QUEUE_CAP*4u) {
        event e;
        inv_status st=INV_OK;
        inv_mutex_lock(&l->mutex);
        if(!l->count) {
            inv_mutex_unlock(&l->mutex);
            break;
        }
        e=l->events[l->head];
        l->head=(l->head+1)%QUEUE_CAP;
        l->count--;
        inv_condition_signal(&l->space);
        inv_mutex_unlock(&l->mutex);
        switch(e.kind) {
            case E_BEGIN:if(s->begin_session)st=s->begin_session(s->context,l->plan);
            break;
            case E_UPSERT:if(s->upsert_series)st=s->upsert_series(s->context,&e.u.series);
            break;
            case E_REMOVE:if(s->remove_series)st=s->remove_series(s->context,e.u.id);
            break;
            case E_FRAME:if(s->begin_frame)st=s->begin_frame(s->context,&e.u.frame);
            break;
            case E_SAMPLE:if(s->emit_sample)st=s->emit_sample(s->context,&e.u.sample);
            break;
            case E_END_FRAME:if(s->end_frame)st=s->end_frame(s->context);
            break;
            case E_DIAG:if(s->emit_diagnostic)st=s->emit_diagnostic(s->context,e.u.diag.status,e.u.diag.text);
            break;
            case E_PROGRESS:if(s->emit_progress)st=s->emit_progress(s->context,e.u.diag.index,e.u.diag.total,e.u.diag.text);
            break;
            case E_END:if(s->end_session)st=s->end_session(s->context,e.u.diag.status);
            break;
        }
        if(st!=INV_OK)return st;
    }
    return INV_OK;
}
int inv_live_done(inv_live *l) {
    int d;
    inv_mutex_lock(&l->mutex);
    d=l->done&&!l->count;
    inv_mutex_unlock(&l->mutex);
    return d;
}
inv_status inv_live_finish(inv_live *l,int stop) {
    inv_status s;
    if(stop) {
        inv_engine_stop(l->engine);
        inv_mutex_lock(&l->mutex);
        l->closed=1;
        inv_condition_broadcast(&l->space);
        inv_mutex_unlock(&l->mutex);
    }
    inv_thread_join(&l->thread);
    s=l->result;
    inv_condition_destroy(&l->space);
    inv_mutex_destroy(&l->mutex);
    free(l->events);
    free(l);
    return s;
}
