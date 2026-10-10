/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
#include <math.h>
#include <time.h>
const char *inv_status_name(inv_status s) {
    static const char *const n[]= {
        "OK","UNSUPPORTED","NOT_PRESENT","PERMISSION_DENIED", "NO_DATA","WARMUP","GONE","INVALID_DATA","IO_ERROR","TIMEOUT","CANCELLED", "NO_MEMORY","LIMIT","BAD_QUERY"
    };
    return (unsigned)s<INV_ARRAY_SIZE(n)?n[s]:"UNKNOWN";
}
const char *inv_type_name(inv_value_type t) {
    static const char *const n[]= {
        "u64","s64","f64","bool","string","bytes"
    };
    return (unsigned)t<INV_ARRAY_SIZE(n)?n[t]:"unknown";
}
const char *inv_kind_name(inv_metric_kind k) {
    return k==INV_PROPERTY?"property":k==INV_GAUGE?"gauge":"counter";
}
int inv_copy(char *d,size_t cap,const char *s) {
    size_t n=strlen(s);
    if(!cap)return 0;
    if(n>=cap) {
        d[0]='\0';
        return 0;
    }
    memcpy(d,s,n+1);
    return 1;
}
void inv_trim(char *s) {
    size_t n=strlen(s),start=0;
    while(n&&isspace((unsigned char)s[n-1]))s[--n]=0;
    while(s[start]&&isspace((unsigned char)s[start]))start++;
    if(start)memmove(s,s+start,strlen(s+start)+1);
}
/* Keep complete valid UTF-8 sequences; neutralize terminal controls and bad bytes. */
void inv_safe_text(char *d,size_t cap,const char *s) {
    size_t used=0;
    if(!cap)return;
    while(*s && used+1<cap) {
        unsigned char c=(unsigned char)*s;
        unsigned code=0, min=0;
        size_t count=1,j;
        int valid=1;
        if(c<0x80) { d[used++]=(c<32||c==127)?' ':(char)c; s++; continue; }
        if(c>=0xc2 && c<=0xdf) {count=2; code=c&31; min=0x80;}
        else if(c>=0xe0 && c<=0xef) {count=3; code=c&15; min=0x800;}
        else if(c>=0xf0 && c<=0xf4) {count=4; code=c&7; min=0x10000;}
        else valid=0;
        if(valid)for(j=1;j<count;j++) {
            unsigned char next=(unsigned char)s[j];
            if(!next || (next&0xc0)!=0x80) {valid=0; break;}
            code=(code<<6)|(next&63);
        }
        if(valid && (code<min || code>0x10ffff || (code>=0xd800 && code<=0xdfff)))valid=0;
        if(!valid) {d[used++]='?'; s++; continue;}
        if((code>=0x80 && code<=0x9f) || (code>=0x202a && code<=0x202e) || (code>=0x2066 && code<=0x2069)) {
            d[used++]=' '; s+=count; continue;
        }
        if(used+count>=cap)break;
        memcpy(d+used,s,count); used+=count; s+=count;
    }
    d[used]=0;
}
const char *inv_label_get(const inv_label *l,size_t n,const char *key) {
    size_t i;
    for(i=0; i<n; i++)if(!strcmp(l[i].key,key))return l[i].value;
    return NULL;
}
int inv_label_set(inv_label *l,size_t *n,const char *k,const char *v) {
    size_t i;
    for(i=0; i<*n; i++)if(!strcmp(l[i].key,k))break;
    if(i>=INV_LABEL_CAP)return 0;
    if(!inv_copy(l[i].key,sizeof(l[i].key),k)||!inv_copy(l[i].value,sizeof(l[i].value),v))return 0;
    if(i==*n)(*n)++;
    return 1;
}
/* Iterative glob: only '*' and '?' have special meaning; no recursive stack. */ int inv_glob_match(const char *p,const char *s) {
    const char *star=NULL,*retry=NULL;
    while(*s) {
        if(*p=='?'||*p==*s) {
            p++;
            s++;
        }
        else if(*p=='*') {
            star=p++;
            retry=s;
        }
        else if(star) {
            p=star+1;
            s=++retry;
        }
        else return 0;
    }
    while(*p=='*')p++;
    return *p==0;
}
int inv_select_matches_metric(const inv_select *s,const inv_metric_desc *m) {
    return inv_glob_match(s->pattern,m->key);
}
int inv_select_matches(const inv_select *s,const inv_instance *in) {
    size_t i;
    if(!inv_select_matches_metric(s,in->metric))return 0;
    for(i=0; i<s->label_count; i++) {
        const char *v=inv_label_get(in->labels,in->label_count,s->labels[i].key);
        if(!v||!inv_glob_match(s->labels[i].value,v))return 0;
    }
    return 1;
}
int inv_metric_wanted(const inv_read_ctx *c,const inv_metric_desc *m) {
    size_t i;
    for(i=0; i<c->plan->select_count; i++)if(inv_select_matches_metric(&c->plan->selects[i],m))return 1;
    return 0;
}
inv_status inv_discover_single(inv_read_ctx *c,const inv_provider_desc *p,inv_discovery_sink *sink) {
    size_t n,i;
    const inv_metric_desc *const *r=inv_registry(&n);
    for(i=0; i<n; i++)if(r[i]->provider==p&&inv_metric_wanted(c,r[i])) {
        inv_instance in;
        inv_status s;
        memset(&in,0,sizeof(in));
        in.metric=r[i];
        inv_copy(in.entity_key,sizeof(in.entity_key),"system");
        s=sink->emit(sink->context,&in);
        if(s!=INV_OK)return s;
    }
    return INV_OK;
}
inv_status inv_errno_status(int e) {
    if(e==EACCES||e==EPERM)return INV_PERMISSION_DENIED;
    if(e==ENOENT||e==ENODEV||e==ENXIO)return INV_NOT_PRESENT;
    if(e==ENOMEM)return INV_NO_MEMORY;
    #ifdef ENODATA
    if(e==ENODATA)return INV_NO_DATA;
    #endif
    #ifdef ETIMEDOUT
    if(e==ETIMEDOUT)return INV_TIMEOUT;
    #endif
    #ifdef EOPNOTSUPP
    if(e==EOPNOTSUPP)return INV_UNSUPPORTED;
    #endif
    return INV_IO_ERROR;
}
int inv_parse_u64(const char *s,uint64_t *out) {
    unsigned long long v;
    char *e;
    while(isspace((unsigned char)*s))s++;
    if(!isdigit((unsigned char)*s))return 0;
    errno=0;
    v=strtoull(s,&e,10);
    if(errno||e==s)return 0;
    while(isspace((unsigned char)*e))e++;
    if(*e)return 0;
    *out=(uint64_t)v;
    return 1;
}
int inv_parse_s64(const char *s,int64_t *out) {
    long long v;
    char *e;
    errno=0;
    v=strtoll(s,&e,10);
    if(errno||e==s)return 0;
    while(isspace((unsigned char)*e))e++;
    if(*e)return 0;
    *out=(int64_t)v;
    return 1;
}
int inv_value_number(const inv_value *v,double *n) {
    switch(v->type) {
        case INV_U64:*n=(double)v->as.u64;
        break;
        case INV_S64:*n=(double)v->as.s64;
        break;
        case INV_F64:*n=v->as.f64;
        break;
        case INV_BOOL:*n=v->as.boolean?1:0;
        break;
        default:return 0;
    }
    return isfinite(*n);
}
int inv_unit_scale(const char *c,const char *d,double *s) {
    *s=1;
    if(!*d||!strcmp(c,d))return 1;
    if(!strcmp(c,"1")&&!strcmp(d,"%")) {
        *s=100;
        return 1;
    }
    if(!strcmp(c,"B")||!strcmp(c,"B/s")) {
        int rate=!strcmp(c,"B/s");
        const char *u[]= {
            "KiB","MiB","GiB","TiB","KiB/s","MiB/s","GiB/s","TiB/s"
        };
        size_t i;
        for(i=0; i<4; i++)if(!strcmp(d,u[i+(rate?4:0)])) {
            *s=1.0/pow(1024.0,(double)i+1);
            return 1;
        }
    }
    if(!strcmp(c,"s")&&!strcmp(d,"ms")) {
        *s=1000;
        return 1;
    }
    return 0;
}
void inv_format_value(char *out,size_t cap,const inv_value *v,const char *unit,const inv_view_spec *view,int machine) {
    double n,scale=1;
    const char *du=view?view->unit:"";
    int prec=view?view->precision:2;
    if(v->type==INV_STRING) {
        inv_safe_text(out,cap,v->as.string);
        return;
    }
    if(v->type==INV_BOOL) {
        snprintf(out,cap,"%s",v->as.boolean?"true":"false");
        return;
    }
    if(v->type==INV_BYTES) {
        size_t i,off=0;
        out[0]=0;
        for(i=0; i<v->as.bytes.size&&off+3<cap; i++)off+=(size_t)snprintf(out+off,cap-off,"%02x",v->as.bytes.data[i]);
        return;
    }
    if(machine||!view) {
        switch(v->type) {
            case INV_U64:snprintf(out,cap,"%" PRIu64,v->as.u64);
            return;
            case INV_S64:snprintf(out,cap,"%" PRId64,v->as.s64);
            return;
            default:snprintf(out,cap,"%.17g",v->as.f64);
            return;
        }
    }
    if(!inv_value_number(v,&n)) {
        snprintf(out,cap,"invalid");
        return;
    }
    if(*du)inv_unit_scale(unit,du,&scale);
    else if(!strcmp(unit,"1")) {
        du="%";
        scale=100;
    }
    else if(!strcmp(unit,"B")||!strcmp(unit,"B/s")) {
        static const char *const u[]= {
            "B","KiB","MiB","GiB","TiB"
        };
        static const char *const r[]= {
            "B/s","KiB/s","MiB/s","GiB/s","TiB/s"
        };
        unsigned k=0;
        while(fabs(n*scale)>=1024&&k<4) {
            k++;
            scale/=1024;
        }
        du=!strcmp(unit,"B")?u[k]:r[k];
    }
    else du=unit;
    if(prec<0)prec=2;
    if(prec>9)prec=9;
    if(scale==1&&v->type==INV_U64)snprintf(out,cap,"%" PRIu64 "%s%s",v->as.u64,*du?" ":"",du);
    else if(scale==1&&v->type==INV_S64)snprintf(out,cap,"%" PRId64 "%s%s",v->as.s64,*du?" ":"",du);
    else snprintf(out,cap,"%.*f%s%s",prec,n*scale,*du?" ":"",du);
}
void inv_series_name(char *d,size_t cap,const inv_series *s) {
    size_t i,off;
    int x=snprintf(d,cap,"%s",s->key);
    if(x<0)return;
    off=(size_t)x;
    for(i=0; i<s->label_count&&off<cap; i++) {
        x=snprintf(d+off,cap-off,"%s%s=%s",i?",":"{",s->labels[i].key,s->labels[i].value);
        if(x<0)return;
        off+=(size_t)x;
    }
    if(s->label_count&&off<cap)snprintf(d+off,cap-off,"}");
}
void inv_utc_string(uint64_t ns,char *out,size_t cap) {
    time_t t=(time_t)(ns/INV_NS_S);
    struct tm tm;
    char b[32];
#ifdef _WIN32
    if(gmtime_s(&tm,&t)!=0) {
#else
    if(!gmtime_r(&t,&tm)) {
#endif
        inv_copy(out,cap,"invalid-time");
        return;
    }
    strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%S",&tm);
    snprintf(out,cap,"%s.%03uZ",b,(unsigned)((ns%INV_NS_S)/INV_NS_MS));
}
inv_status inv_counter_derive(inv_transform op,const inv_sample *prev,const inv_sample *cur,inv_value *v,uint64_t *window) {
    uint64_t diff;
    *window=0;
    if(cur->status!=INV_OK)return cur->status;
    if(prev->status!=INV_OK||prev->value.type!=INV_U64||cur->value.type!=INV_U64|| prev->counter_epoch!=cur->counter_epoch||cur->measured_at_ns<=prev->measured_at_ns|| cur->value.as.u64<prev->value.as.u64)return INV_WARMUP;
    diff=cur->value.as.u64-prev->value.as.u64;
    *window=cur->measured_at_ns-prev->measured_at_ns;
    if(op==INV_DELTA) {
        v->type=INV_U64;
        v->as.u64=diff;
    }
    else {
        v->type=INV_F64;
        v->as.f64=(double)diff/((double)*window/(double)INV_NS_S);
    }
    return INV_OK;
}
