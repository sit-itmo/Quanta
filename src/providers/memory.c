/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
inv_status inv_parse_meminfo(const char *text,inv_mem_data *d) {
    const char *p=text;
    memset(d,0,sizeof(*d));
    while(*p) {
        const char *end=strchr(p,'\n'),*colon;
        size_t n=end?(size_t)(end-p):strlen(p);
        unsigned bit=0;
        uint64_t *field=NULL;
        colon=(const char *)memchr(p,':',n);
        if(colon) {
            size_t keylen=(size_t)(colon-p);
#define FIELD(name,b,f) if(keylen==sizeof(name)-1&&!memcmp(p,name,sizeof(name)-1)){bit=b;field=&d->f;}
            FIELD("MemTotal",1,total) else FIELD("MemAvailable",2,available) else FIELD("MemFree",4,free) else FIELD("SwapTotal",8,swap_total) else FIELD("SwapFree",16,swap_free)
#undef FIELD
            if(field) {
                char number[64];
                const char *s=colon+1,*e;
                char *tail;
                unsigned long long x;
                size_t len;
                if(d->found&bit)return INV_INVALID_DATA;
                while(s<p+n&&isspace((unsigned char)*s))s++;
                e=s;
                while(e<p+n&&isdigit((unsigned char)*e))e++;
                len=(size_t)(e-s);
                if(!len||len>=sizeof(number))return INV_INVALID_DATA;
                memcpy(number,s,len);
                number[len]=0;
                errno=0;
                x=strtoull(number,&tail,10);
                if(errno||*tail||x>UINT64_MAX/1024)return INV_INVALID_DATA;
                while(e<p+n&&isspace((unsigned char)*e))e++;
                if(p+n-e<2||memcmp(e,"kB",2))return INV_INVALID_DATA;
                e+=2;
                while(e<p+n&&isspace((unsigned char)*e))e++;
                if(e!=p+n)return INV_INVALID_DATA;
                *field=(uint64_t)x*1024;
                d->found|=bit;
            }
        }
        if(!end)break;
        p=end+1;
    }
    if((d->found&3)==3&&d->available>d->total)return INV_INVALID_DATA;
    if((d->found&24)==24&&d->swap_free>d->swap_total)return INV_INVALID_DATA;
    return (d->found&1)?INV_OK:INV_NO_DATA;
}
static inv_status mem_open(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(inv_mem_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status mem_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_memory_provider,s);
}
static inv_status mem_refresh(inv_read_ctx *c) {
    char path[INV_PATH_CAP],b[32768];
    inv_status s;
    if(!inv_path(path,sizeof(path),c->paths.proc_root,"meminfo"))return INV_LIMIT;
    s=inv_read_text(path,b,sizeof(b));
    return s==INV_OK?inv_parse_meminfo(b,(inv_mem_data *)c->data):s;
}
static void mem_close(inv_read_ctx *c) {
    free(c->data);
    c->data=NULL;
}
inv_status inv_memory_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    inv_mem_data *d=(inv_mem_data *)c->data;
    const char *a=in->metric->arg;
    (void)w;
    if(!strcmp(a,"total")) {
        if(!(d->found&1))return INV_NO_DATA;
        v->as.u64=d->total;
    }
    else if(!strcmp(a,"available")) {
        if(!(d->found&2))return INV_NO_DATA;
        v->as.u64=d->available;
        *q|=INV_QUALITY_ESTIMATED;
    }
    else if(!strcmp(a,"free")) {
        if(!(d->found&4))return INV_NO_DATA;
        v->as.u64=d->free;
    }
    else if(!strcmp(a,"used")||!strcmp(a,"utilization")) {
        if((d->found&3)!=3)return INV_NO_DATA;
        *q|=INV_QUALITY_ESTIMATED;
        if(!strcmp(a,"used"))v->as.u64=d->total-d->available;
        else {
            if(!d->total)return INV_NO_DATA;
            v->as.f64=(double)(d->total-d->available)/(double)d->total;
        }
    }
    else if(!strcmp(a,"swap_total")) {
        if(!(d->found&8))return INV_NO_DATA;
        v->as.u64=d->swap_total;
    }
    else if(!strcmp(a,"swap_used")) {
        if((d->found&24)!=24)return INV_NO_DATA;
        v->as.u64=d->swap_total-d->swap_free;
    }
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_memory_provider= {
    1,"linux.meminfo","One /proc/meminfo snapshot",mem_open,mem_discover,mem_refresh,mem_close
};
