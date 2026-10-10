/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
typedef struct cpu_row {
    char name[24];
    uint64_t t[8];
} cpu_row;
typedef struct cpu_data {
    cpu_row *rows;
    size_t count,cap;
    uint64_t time;
} cpu_data;
inv_status inv_cpu_fraction(const uint64_t p[8],const uint64_t n[8],double *out) {
    uint64_t total=0,idle=0;
    size_t i;
    for(i=0; i<8; i++) {
        uint64_t diff;
        if(n[i]<p[i])return INV_WARMUP;
        diff=n[i]-p[i];
        if(UINT64_MAX-total<diff)return INV_INVALID_DATA;
        total+=diff;
        if(i==3||i==4) {
            if(UINT64_MAX-idle<diff)return INV_INVALID_DATA;
            idle+=diff;
        }
    }
    if(!total)return INV_WARMUP;
    *out=(double)(total-idle)/(double)total;
    return INV_OK;
}
static inv_status cpu_refresh(inv_read_ctx *c) {
    cpu_data *d=(cpu_data *)c->data;
    char path[INV_PATH_CAP],*buf,*p;
    inv_status st;
    if(!inv_path(path,sizeof(path),c->paths.proc_root,"stat"))return INV_LIMIT;
    buf=(char *)malloc(1024*1024);
    if(!buf)return INV_NO_MEMORY;
    st=inv_read_text(path,buf,1024*1024);
    if(st!=INV_OK) {
        free(buf);
        return st;
    }
    d->count=0;
    p=buf;
    while(*p) {
        char *end=strchr(p,'\n');
        if(end)*end=0;
        if(!strncmp(p,"cpu",3)&&(isspace((unsigned char)p[3])||isdigit((unsigned char)p[3]))) {
            cpu_row row;
            char *s=p,*e;
            size_t len=0,k;
            memset(&row,0,sizeof(row));
            while(s[len]&&!isspace((unsigned char)s[len]))len++;
            if(len>=sizeof(row.name)) {
                st=INV_INVALID_DATA;
                break;
            }
            memcpy(row.name,s,len);
            s+=len;
            for(k=0; k<8; k++) {
                unsigned long long x;
                while(isspace((unsigned char)*s))s++;
                if(!*s) {
                    if(k<4)st=INV_INVALID_DATA;
                    break;
                }
                if(!isdigit((unsigned char)*s)) {
                    st=INV_INVALID_DATA;
                    break;
                }
                errno=0;
                x=strtoull(s,&e,10);
                if(errno||e==s||(*e&&!isspace((unsigned char)*e))) {
                    st=INV_INVALID_DATA;
                    break;
                }
                row.t[k]=(uint64_t)x;
                s=e;
            }
            if(st!=INV_OK)break;
            if(d->count==d->cap) {
                size_t cap=d->cap?d->cap*2:64;
                cpu_row *r;
                if(cap>8192) {
                    st=INV_LIMIT;
                    break;
                }
                r=(cpu_row *)realloc(d->rows,cap*sizeof(*r));
                if(!r) {
                    st=INV_NO_MEMORY;
                    break;
                }
                d->rows=r;
                d->cap=cap;
            }
            d->rows[d->count++]=row;
        }
        if(!end)break;
        p=end+1;
    }
    d->time=inv_now_ns();
    free(buf);
    return st==INV_OK&&!d->count?INV_NO_DATA:st;
}
static inv_status cpu_open(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(cpu_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status cpu_discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    cpu_data *d=(cpu_data *)c->data;
    size_t i,j,n;
    inv_status s=cpu_refresh(c);
    const inv_metric_desc *const *r=inv_registry(&n);
    if(s!=INV_OK)return s;
    for(i=0; i<d->count; i++)for(j=0; j<n; j++)if(r[j]->provider==&inv_cpu_provider&&inv_metric_wanted(c,r[j])) {
        int core=!strncmp(r[j]->key,"cpu.core.",9);
        inv_instance in;
        if(core==!strcmp(d->rows[i].name,"cpu"))continue;
        memset(&in,0,sizeof(in));
        in.metric=r[j];
        inv_copy(in.resource,sizeof(in.resource),d->rows[i].name);
        inv_copy(in.entity_key,sizeof(in.entity_key),d->rows[i].name);
        if(core)inv_label_set(in.labels,&in.label_count,"cpu",d->rows[i].name+3);
        s=sink->emit(sink->context,&in);
        if(s!=INV_OK)return s;
    }
    return INV_OK;
}
static void cpu_close(inv_read_ctx *c) {
    cpu_data *d=(cpu_data *)c->data;
    if(d) {
        free(d->rows);
        free(d);
    }
    c->data=NULL;
}
inv_status inv_cpu_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    cpu_data *d=(cpu_data *)c->data;
    cpu_row *r=NULL;
    size_t i,k;
    const char *a=in->metric->arg;
    (void)q;
    for(i=0; i<d->count; i++)if(!strcmp(in->resource,d->rows[i].name)) {
        r=&d->rows[i];
        break;
    }
    if(!r)return INV_GONE;
    if(!strcmp(a,"utilization")) {
        inv_status s=in->reader_state[8]?inv_cpu_fraction(in->reader_state,r->t,&v->as.f64):INV_WARMUP;
        if(in->reader_state[8]&&d->time>in->reader_state[9])*w=d->time-in->reader_state[9];
        memcpy(in->reader_state,r->t,8*sizeof(uint64_t));
        in->reader_state[8]=1;
        in->reader_state[9]=d->time;
        return s;
    }
    if(!strcmp(a,"idle")) {
        v->as.u64=r->t[3];
        return INV_OK;
    }
    if(!strcmp(a,"total")) {
        uint64_t sum=0;
        for(k=0; k<8; k++) {
            if(UINT64_MAX-sum<r->t[k])return INV_INVALID_DATA;
            sum+=r->t[k];
        }
        v->as.u64=sum;
        return INV_OK;
    }
    return INV_UNSUPPORTED;
}
const inv_provider_desc inv_cpu_provider= {
    2,"linux.procstat","One /proc/stat snapshot; non-overlapping CPU accounting fields",cpu_open,cpu_discover,cpu_refresh,cpu_close
};
