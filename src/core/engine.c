/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <math.h>
typedef struct channel {
    inv_instance in;
    inv_sample last;
    uint64_t period,next;
    unsigned seen;
    int active,updated,initialized;
} channel;
typedef struct binding {
    inv_series meta;
    inv_sample previous,last;
    size_t channel_index;
    uint64_t next;
    int active,announced,dirty,has_last;
} binding;
struct inv_engine {
    inv_plan plan;
    inv_paths paths;
    char roots[3][INV_PATH_CAP];
    inv_read_ctx ctx[INV_PROVIDER_COUNT];
    const inv_provider_desc *providers[INV_PROVIDER_COUNT];
    inv_status open_status[INV_PROVIDER_COUNT],discovery_status[INV_PROVIDER_COUNT];
    uint64_t refresh_count[INV_PROVIDER_COUNT];
    channel *channels;
    size_t channel_count,channel_cap;
    binding *bindings;
    size_t binding_count,binding_cap;
    uint64_t next_id,frame_id,discovery_epoch;
    inv_mutex control_mutex;
    int stopped,force,has_errors;
    inv_sink *sink;
};
static inv_status diagnostic(inv_engine *e,inv_status s,const char *m) {
    return e->sink&&e->sink->emit_diagnostic?e->sink->emit_diagnostic(e->sink->context,s,m):INV_OK;
}
static int stopping(inv_engine *e) {
    int stop;
    inv_mutex_lock(&e->control_mutex);
    stop=e->stopped;
    inv_mutex_unlock(&e->control_mutex);
    return stop||inv_interrupted();
}
void inv_engine_stop(inv_engine *e) {
    inv_mutex_lock(&e->control_mutex);
    e->stopped=1;
    inv_mutex_unlock(&e->control_mutex);
}
void inv_engine_refresh(inv_engine *e) {
    inv_mutex_lock(&e->control_mutex);
    e->force=1;
    inv_mutex_unlock(&e->control_mutex);
}
static int consume_refresh(inv_engine *e) {
    int force;
    inv_mutex_lock(&e->control_mutex);
    force=e->force;
    e->force=0;
    inv_mutex_unlock(&e->control_mutex);
    return force;
}
static inv_status announce(inv_engine *e,binding *b) {
    inv_status s=INV_OK;
    if(e->sink&&e->sink->upsert_series)s=e->sink->upsert_series(e->sink->context,&b->meta);
    if(s==INV_OK&&e->sink)b->announced=1;
    return s;
}
static inv_status remove_binding(inv_engine *e,binding *b) {
    inv_status s=INV_OK;
    if(b->announced&&e->sink&&e->sink->remove_series)s=e->sink->remove_series(e->sink->context,b->meta.id);
    b->active=0;
    b->announced=0;
    return s;
}
static inv_status reserve_channels(inv_engine *e) {
    if(e->channel_count==e->channel_cap) {
        size_t cap=e->channel_cap?e->channel_cap*2:32;
        channel *p;
        if(cap>INV_MAX_CHANNELS)return INV_LIMIT;
        p=(channel *)realloc(e->channels,cap*sizeof(*p));
        if(!p)return INV_NO_MEMORY;
        memset(p+e->channel_cap,0,(cap-e->channel_cap)*sizeof(*p));
        e->channels=p;
        e->channel_cap=cap;
    }
    return INV_OK;
}
static inv_status reserve_bindings(inv_engine *e) {
    if(e->binding_count==e->binding_cap) {
        size_t cap=e->binding_cap?e->binding_cap*2:32;
        binding *p;
        if(cap>INV_MAX_SERIES)return INV_LIMIT;
        p=(binding *)realloc(e->bindings,cap*sizeof(*p));
        if(!p)return INV_NO_MEMORY;
        memset(p+e->binding_cap,0,(cap-e->binding_cap)*sizeof(*p));
        e->bindings=p;
        e->binding_cap=cap;
    }
    return INV_OK;
}
static inv_status discovered(void *context,const inv_instance *in) {
    inv_engine *e=(inv_engine *)context;
    size_t i,j,ci=SIZE_MAX;
    channel *c;
    int any=0;
    inv_status s;
    for(i=0; i<e->plan.select_count; i++)if(inv_select_matches(&e->plan.selects[i],in)) {
        any=1;
        break;
    }
    if(!any)return INV_OK;
    for(i=0; i<e->channel_count; i++)if(e->channels[i].active&&e->channels[i].in.metric==in->metric&& !strcmp(e->channels[i].in.entity_key,in->entity_key)) {
        ci=i;
        break;
    }
    if(ci==SIZE_MAX) {
        for(i=0; i<e->channel_count; i++)if(!e->channels[i].active) {
            ci=i;
            break;
        }
        if(ci==SIZE_MAX) {
            s=reserve_channels(e);
            if(s!=INV_OK)return s;
            ci=e->channel_count++;
        }
        c=&e->channels[ci];
        memset(c,0,sizeof(*c));
        c->in=*in;
        c->in.series_id=++e->next_id;
        c->in.generation=(uint32_t)e->next_id;
        c->active=1;
        c->last.status=INV_NO_DATA;
    }
    else {
        c=&e->channels[ci];
        inv_copy(c->in.resource,sizeof(c->in.resource),in->resource);
        c->in.min_poll_ns=in->min_poll_ns;
        c->in.label_count=in->label_count;
        memcpy(c->in.labels,in->labels,sizeof(in->labels));
    }
    c->seen=(unsigned)e->discovery_epoch;
    c->period=UINT64_MAX;
    for(i=0; i<e->plan.select_count; i++) {
        inv_select *sel=&e->plan.selects[i];
        binding *b=NULL;
        uint64_t period;
        int match=inv_select_matches(sel,in);
        for(j=0; j<e->binding_count; j++)if(e->bindings[j].active&&e->bindings[j].channel_index==ci&&e->bindings[j].meta.selection_index==i) {
            b=&e->bindings[j];
            break;
        }
        if(!match) {
            if(b) {
                s=remove_binding(e,b);
                if(s!=INV_OK)return s;
            }
            continue;
        }
        period=INV_MAX(sel->period_ns,INV_MAX(in->metric->min_poll_ns,in->min_poll_ns));
        if(e->plan.strict_poll && in->metric->refresh!=INV_REFRESH_ONCE && sel->period_ns<period)return INV_BAD_QUERY;
        if(in->metric->refresh==INV_REFRESH_ON_EVENT)period=INV_MAX(period,5*INV_NS_S);
        if(period<c->period)c->period=period;
        if(!b) {
            for(j=0; j<e->binding_count; j++)if(!e->bindings[j].active) {
                b=&e->bindings[j];
                break;
            }
            if(!b) {
                s=reserve_bindings(e);
                if(s!=INV_OK)return s;
                b=&e->bindings[e->binding_count++];
            }
            memset(b,0,sizeof(*b));
            b->active=1;
            b->channel_index=ci;
            b->previous.status=INV_NO_DATA;
            b->meta.id=++e->next_id;
            b->meta.source_id=c->in.series_id;
            b->meta.generation=c->in.generation;
            b->meta.metric=in->metric;
            b->meta.selection_index=(unsigned)i;
            b->meta.type=sel->transform==INV_RATE?INV_F64:in->metric->type;
            b->meta.transform=sel->transform;
            b->meta.view=sel->view;
            b->meta.effective_period_ns=period;
            if(sel->transform==INV_RAW)inv_copy(b->meta.key,sizeof(b->meta.key),in->metric->key);
            else snprintf(b->meta.key,sizeof(b->meta.key),"%s(%s)",sel->transform==INV_RATE?"rate":"delta",in->metric->key);
            snprintf(b->meta.unit,sizeof(b->meta.unit),"%s%s",in->metric->unit,sel->transform==INV_RATE?"/s":"");
            inv_copy(b->meta.entity_key,sizeof(b->meta.entity_key),in->entity_key);
            b->meta.label_count=in->label_count;
            memcpy(b->meta.labels,in->labels,sizeof(in->labels));
            s=announce(e,b);
            if(s!=INV_OK)return s;
        }
        else if(b->meta.effective_period_ns!=period||b->meta.label_count!=in->label_count||memcmp(b->meta.labels,in->labels,sizeof(in->labels))) {
            b->meta.effective_period_ns=period;
            b->meta.label_count=in->label_count;
            memcpy(b->meta.labels,in->labels,sizeof(in->labels));
            s=announce(e,b);
            if(s!=INV_OK)return s;
        }
    }
    if(c->initialized&&c->next!=UINT64_MAX&&c->next<c->last.measured_at_ns+c->period)c->next=c->last.measured_at_ns+c->period;
    return INV_OK;
}
static inv_status discover(inv_engine *e) {
    unsigned p;
    size_t i,j;
    inv_discovery_sink sink;
    e->discovery_epoch++;
    sink.context=e;
    sink.emit=discovered;
    for(p=0; p<INV_PROVIDER_COUNT; p++)if(e->providers[p]&&e->open_status[p]==INV_OK) {
        inv_status s=e->providers[p]->discover?e->providers[p]->discover(&e->ctx[p],&sink):INV_OK;
        if(s==INV_NO_MEMORY||s==INV_LIMIT||s==INV_BAD_QUERY)return s;
        if(s!=INV_OK&&s!=e->discovery_status[p]) {
            char msg[256];
            snprintf(msg,sizeof(msg),"discovery: %s: %s",e->providers[p]->id,inv_status_name(s));
            if(diagnostic(e,s,msg)!=INV_OK)return INV_IO_ERROR;
        }
        e->discovery_status[p]=s;
        /* Failed/partial enumeration is not evidence that devices disappeared. */ if(s==INV_OK)for(i=0; i<e->channel_count; i++)if(e->channels[i].active&&e->channels[i].in.metric->provider->index==p&&e->channels[i].seen!=(unsigned)e->discovery_epoch) {
            for(j=0; j<e->binding_count; j++)if(e->bindings[j].active&&e->bindings[j].channel_index==i) {
                s=remove_binding(e,&e->bindings[j]);
                if(s!=INV_OK)return s;
            }
            e->channels[i].active=0;
        }
    }
    return INV_OK;
}
inv_status inv_engine_create(const inv_plan *plan,const inv_paths *paths,inv_engine **out,char *err,size_t cap) {
    inv_engine *e;
    size_t n,i,j;
    const inv_metric_desc *const *r=inv_registry(&n);
    inv_status s;
    *out=NULL;
    e=(inv_engine *)calloc(1,sizeof(*e));
    if(!e)return INV_NO_MEMORY;
    if(inv_mutex_init(&e->control_mutex)) {
        free(e);
        return INV_IO_ERROR;
    }
    e->plan=*plan;
    if(!inv_copy(e->roots[0],sizeof(e->roots[0]),paths&&paths->proc_root?paths->proc_root:"/proc")|| !inv_copy(e->roots[1],sizeof(e->roots[1]),paths&&paths->sys_root?paths->sys_root:"/sys")|| !inv_copy(e->roots[2],sizeof(e->roots[2]),paths&&paths->etc_root?paths->etc_root:"/etc")) {
        s=INV_LIMIT;
        goto error;
    }
    e->paths.proc_root=e->roots[0];
    e->paths.sys_root=e->roots[1];
    e->paths.etc_root=e->roots[2];
    for(i=0; i<n; i++)for(j=0; j<plan->select_count; j++)if(inv_select_matches_metric(&plan->selects[j],r[i]))e->providers[r[i]->provider->index]=r[i]->provider;
    for(i=0; i<INV_PROVIDER_COUNT; i++)if(e->providers[i]) {
        e->ctx[i].plan=&e->plan;
        e->ctx[i].paths=e->paths;
        e->open_status[i]=e->providers[i]->open?e->providers[i]->open(&e->ctx[i]):INV_OK;
        if(e->open_status[i]==INV_NO_MEMORY) {
            s=INV_NO_MEMORY;
            goto error;
        }
    }
    s=discover(e);
    if(s!=INV_OK)goto error;
    for(i=0; i<plan->select_count; i++) {
        int found=0;
        for(j=0; j<e->binding_count; j++)if(e->bindings[j].active&&e->bindings[j].meta.selection_index==i) {
            found=1;
            break;
        }
        if(!found&&!plan->selects[i].optional) {
            snprintf(err,cap,"no discoverable instances for '%s'; use optional for absent hardware",plan->selects[i].pattern);
            s=INV_NOT_PRESENT;
            goto error_keep;
        }
    }
    *out=e;
    return INV_OK;
    error: snprintf(err,cap,"engine initialization: %s%s",inv_status_name(s),s==INV_BAD_QUERY?" (strict polling is below a discovered device minimum)":"");
    error_keep: inv_engine_destroy(e);
    return s;
}
void inv_engine_destroy(inv_engine *e) {
    unsigned i;
    if(!e)return;
    for(i=0; i<INV_PROVIDER_COUNT; i++)if(e->providers[i]&&e->providers[i]->close)e->providers[i]->close(&e->ctx[i]);
    free(e->channels);
    free(e->bindings);
    inv_mutex_destroy(&e->control_mutex);
    free(e);
}
int inv_engine_has_sample_errors(const inv_engine *e) {
    return e->has_errors;
}
static inv_status collect(inv_engine *e,uint64_t now,int force,int *changed) {
    unsigned refreshed[INV_PROVIDER_COUNT]= {
        0
    };
    inv_status status[INV_PROVIDER_COUNT];
    size_t i,j;
    *changed=0;
    memset(status,0,sizeof(status));
    for(i=0; i<e->channel_count; i++) {
        channel *c=&e->channels[i];
        const inv_metric_desc *m=c->in.metric;
        inv_read_ctx *ctx;
        inv_sample sample;
        unsigned p;
        uint64_t start;
        c->updated=0;
        if(!c->active)continue;
        if(force&&m->refresh==INV_REFRESH_ONCE)c->next=0;
        if(c->initialized&&now<c->next)continue;
        p=m->provider->index;
        ctx=&e->ctx[p];
        if(!refreshed[p]) {
            ctx->now_ns=inv_now_ns();
            ctx->error[0]=0;
            status[p]=e->open_status[p]!=INV_OK?e->open_status[p]:m->provider->refresh?m->provider->refresh(ctx):INV_OK;
            ctx->acquisition_ns=inv_now_ns()-ctx->now_ns;
            ctx->now_ns+=ctx->acquisition_ns;
            refreshed[p]=1;
            e->refresh_count[p]++;
        }
        memset(&sample,0,sizeof(sample));
        sample.series_id=c->in.series_id;
        sample.sequence=c->last.sequence+1;
        sample.value.type=m->type;
        sample.counter_epoch=c->in.generation;
        start=inv_now_ns();
        sample.status=status[p];
        if(sample.status==INV_OK)sample.status=m->read(ctx,&c->in,&sample.value,&sample.window_ns,&sample.quality_flags);
        sample.measured_at_ns=inv_now_ns();
        sample.read_duration_ns=sample.measured_at_ns-start+(m->provider->refresh?ctx->acquisition_ns:0);
        /* Group-backed fields use the same acquisition time. */ if(m->provider->refresh)sample.measured_at_ns=ctx->now_ns;
        if(sample.status==INV_OK) {
            if(sample.value.type!=m->type || (sample.value.type==INV_F64&&!isfinite(sample.value.as.f64)) || (sample.value.type==INV_STRING&&!memchr(sample.value.as.string,0,sizeof(sample.value.as.string))) || (sample.value.type==INV_BYTES&&sample.value.as.bytes.size>sizeof(sample.value.as.bytes.data)))sample.status=INV_INVALID_DATA;
        }
        if(sample.status!=INV_OK) {
            inv_copy(sample.message,sizeof(sample.message),ctx->error[0]?ctx->error:inv_status_name(sample.status));
            if(sample.status!=INV_WARMUP)e->has_errors=1;
        }
        c->last=sample;
        c->initialized=1;
        c->updated=1;
        c->next=m->refresh==INV_REFRESH_ONCE?UINT64_MAX:inv_now_ns()+c->period;
    }
    for(j=0; j<e->binding_count; j++) {
        binding *b=&e->bindings[j];
        channel *c;
        inv_sample result;
        if(!b->active)continue;
        c=&e->channels[b->channel_index];
        if(!c->updated|| (b->has_last&&now<b->next&&!(force&&c->in.metric->refresh==INV_REFRESH_ONCE)))continue;
        result=c->last;
        result.series_id=b->meta.id;
        result.sequence=b->has_last?b->last.sequence+1:1;
        if(b->meta.transform!=INV_RAW) {
            result.status=inv_counter_derive(b->meta.transform,&b->previous,&c->last,&result.value,&result.window_ns);
            b->previous=c->last;
            result.value.type=b->meta.type;
            if(result.status!=INV_OK)inv_copy(result.message,sizeof(result.message),inv_status_name(result.status));
        }
        b->last=result;
        b->has_last=1;
        b->dirty=1;
        b->next=now+b->meta.effective_period_ns;
        *changed=1;
    }
    return INV_OK;
}
static inv_status emit_frame(inv_engine *e,int all) {
    inv_frame_info f;
    size_t i;
    inv_status s=INV_OK;
    f.id=++e->frame_id;
    f.monotonic_ns=inv_now_ns();
    f.realtime_ns=inv_realtime_ns();
    if(e->sink->begin_frame&&(s=e->sink->begin_frame(e->sink->context,&f))!=INV_OK)return s;
    for(i=0; i<e->binding_count; i++)if(e->bindings[i].active&&e->bindings[i].has_last&&(all||e->bindings[i].dirty)) {
        if(e->sink->emit_sample&&(s=e->sink->emit_sample(e->sink->context,&e->bindings[i].last))!=INV_OK)return s;
        e->bindings[i].dirty=0;
    }
    return e->sink->end_frame?e->sink->end_frame(e->sink->context):INV_OK;
}
inv_status inv_engine_run(inv_engine *e,inv_sink *sink) {
    inv_status result=INV_OK;
    uint64_t start=inv_now_ns(),next_discovery=start+5*INV_NS_S,next_heartbeat=start;
    uint64_t warm_until=e->plan.once?start+e->plan.warmup_ns:0;
    size_t i,j;
    int changed;
    e->sink=sink;
    if(sink->api_version!=1||sink->struct_size<sizeof(*sink)) {
        e->sink=NULL;
        return INV_UNSUPPORTED;
    }
    if(sink->begin_session&&(result=sink->begin_session(sink->context,&e->plan))!=INV_OK)goto end;
    for(i=0; i<e->binding_count; i++)if(e->bindings[i].active&&(result=announce(e,&e->bindings[i]))!=INV_OK)goto end;
    for(i=0; i<e->plan.select_count; i++) {
        int found=0;
        for(j=0; j<e->binding_count; j++)if(e->bindings[j].active&&e->bindings[j].meta.selection_index==i)found=1;
        if(!found) {
            char msg[256];
            snprintf(msg,sizeof(msg),"optional selector '%s' has no available instances",e->plan.selects[i].pattern);
            if((result=diagnostic(e,INV_NOT_PRESENT,msg))!=INV_OK)goto end;
        }
    }
    for(i=0; i<e->binding_count; i++)if(e->bindings[i].active) {
        binding *b=&e->bindings[i];
        uint64_t requested=e->plan.selects[b->meta.selection_index].period_ns;
        if(b->meta.metric->refresh==INV_REFRESH_POLL&&b->meta.effective_period_ns>requested) {
            char msg[256];
            snprintf(msg,sizeof(msg),"%s: polling clamped from %.3fs to %.3fs",b->meta.key,(double)requested/1e9,(double)b->meta.effective_period_ns/1e9);
            if((result=diagnostic(e,INV_OK,msg))!=INV_OK)goto end;
        }
    }
    while(!stopping(e)) {
        uint64_t now=inv_now_ns(),next=now+50*INV_NS_MS;
        int force=consume_refresh(e);
        if(e->plan.duration_ns&&now-start>=e->plan.duration_ns)break;
        if(now>=next_discovery||force) {
            result=discover(e);
            if(result!=INV_OK)break;
            next_discovery=now+5*INV_NS_S;
        }
        result=collect(e,now,force,&changed);
        if(result!=INV_OK)break;
        if(e->plan.once) {
            if(now>=warm_until) {
                result=emit_frame(e,1);
                break;
            }
        }
        else if(changed||now>=next_heartbeat) {
            result=emit_frame(e,0);
            if(result!=INV_OK)break;
            next_heartbeat=now+e->plan.period_ns;
            if(e->plan.count&&e->frame_id>=e->plan.count)break;
        }
        for(i=0; i<e->channel_count; i++)if(e->channels[i].active&&e->channels[i].next<next)next=e->channels[i].next;
        if(e->plan.once&&warm_until<next)next=warm_until;
        now=inv_now_ns();
        if(next>now)inv_sleep_ns(next-now);
    }
    if(stopping(e)&&result==INV_OK)result=INV_CANCELLED;
    end: if(sink->end_session) {
        inv_status s=sink->end_session(sink->context,result);
        if(result==INV_OK&&s!=INV_OK)result=s;
    }
    e->sink=NULL;
    return result;
}
void inv_engine_explain(const inv_engine *e,FILE *out) {
    size_t i;
    fprintf(out,"INVQ plan | mode=%s | %s | selections=%zu\n",e->plan.mode==INV_MODE_GUI?"gui":e->plan.mode==INV_MODE_TUI?"tui":"file",e->plan.once?"once":"watch",e->plan.select_count);
    fprintf(out,"Acquisition channels: %zu; output series/views: %zu\n",e->channel_count,e->binding_count);
    for(i=0; i<e->binding_count; i++)if(e->bindings[i].active) {
        const binding *b=&e->bindings[i];
        char name[1024];
        inv_series_name(name,sizeof(name),&b->meta);
        fprintf(out,"  %s\n    output=%" PRIu64 " source=%" PRIu64 " provider=%s type=%s unit=%s period=%.3fs policy=%s\n",name,b->meta.id,b->meta.source_id,b->meta.metric->provider->id,inv_type_name(b->meta.type),b->meta.unit,(double)b->meta.effective_period_ns/1e9,b->meta.metric->refresh==INV_REFRESH_ONCE?"once":b->meta.metric->refresh==INV_REFRESH_ON_EVENT?"rediscovery":"poll");
    }
}
uint64_t inv_engine_provider_refreshes(const inv_engine *e,const char *name) {
    unsigned i;
    for(i=0; i<INV_PROVIDER_COUNT; i++)if(e->providers[i]&&!strcmp(e->providers[i]->id,name))return e->refresh_count[i];
    return 0;
}
