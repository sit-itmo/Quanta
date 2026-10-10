/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
typedef struct net_row {
    char name[128];
    uint64_t fields[16];
} net_row;
typedef struct net_data {
    net_row *rows;
    size_t count,cap;
    inv_status status;
} net_data;
static inv_status net_open(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(net_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status net_refresh(inv_read_ctx *c) {
    net_data *d=(net_data *)c->data;
    char path[INV_PATH_CAP],*b,*p;
    inv_status st;
    d->count=0;
    if(!inv_path(path,sizeof(path),c->paths.proc_root,"net/dev"))return INV_LIMIT;
    b=(char *)malloc(1024*1024);
    if(!b)return INV_NO_MEMORY;
    st=inv_read_text(path,b,1024*1024);
    if(st!=INV_OK) {
        free(b);
        return st;
    }
    p=b;
    while(*p) {
        char *end=strchr(p,'\n'),*colon;
        if(end)*end=0;
        colon=strrchr(p,':');
        if(colon) {
            net_row row;
            char *s,*e;
            size_t k,n;
            memset(&row,0,sizeof(row));
            *colon=0;
            inv_trim(p);
            n=strlen(p);
            if(!n||n>=sizeof(row.name)) {
                st=INV_INVALID_DATA;
                break;
            }
            memcpy(row.name,p,n+1);
            s=colon+1;
            for(k=0; k<16; k++) {
                while(isspace((unsigned char)*s))s++;
                if(!isdigit((unsigned char)*s)) {
                    st=INV_INVALID_DATA;
                    break;
                }
                errno=0;
                row.fields[k]=(uint64_t)strtoull(s,&e,10);
                if(errno||e==s||(*e&&!isspace((unsigned char)*e))) {
                    st=INV_INVALID_DATA;
                    break;
                }
                s=e;
            }
            if(st!=INV_OK)break;
            if(d->count==d->cap) {
                size_t cap=d->cap?d->cap*2:16;
                net_row *r;
                if(cap>4096) {
                    st=INV_LIMIT;
                    break;
                }
                r=(net_row *)realloc(d->rows,cap*sizeof(*r));
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
    free(b);
    return st;
}
static inv_status net_discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    char path[INV_PATH_CAP];
    DIR *dir;
    struct dirent *de;
    size_t n,j;
    const inv_metric_desc *const *r=inv_registry(&n);
    inv_status result=INV_OK, incomplete=INV_OK;
    if(!inv_path(path,sizeof(path),c->paths.sys_root,"class/net"))return INV_LIMIT;
    dir=opendir(path);
    if(!dir)return inv_errno_status(errno);
    while((de=readdir(dir))!=NULL) {
        char base[INV_PATH_CAP],f[INV_PATH_CAP],idxstr[32],identity[128];
        uint64_t idx;
        struct stat st;
        if(de->d_name[0]=='.')continue;
        if(!inv_path(base,sizeof(base),path,de->d_name)||!inv_path(f,sizeof(f),base,"ifindex")) {
            result=INV_LIMIT;
            break;
        }
        {
            inv_status probe=inv_read_u64(f,&idx);
            if(probe!=INV_OK) {
                incomplete=probe;
                continue;
            }
            if(stat(base,&st)) {
                incomplete=inv_errno_status(errno);
                continue;
            }
        }
        snprintf(identity,sizeof(identity),"net:%" PRIu64 ":%" PRIuMAX ":%" PRIuMAX,idx,(uintmax_t)st.st_dev,(uintmax_t)st.st_ino);
        snprintf(idxstr,sizeof(idxstr),"%" PRIu64,idx);
        for(j=0; j<n; j++)if(r[j]->provider==&inv_network_provider&&inv_metric_wanted(c,r[j])) {
            inv_instance in;
            memset(&in,0,sizeof(in));
            in.metric=r[j];
            inv_copy(in.entity_key,sizeof(in.entity_key),identity);
            inv_copy(in.resource,sizeof(in.resource),base);
            inv_label_set(in.labels,&in.label_count,"ifname",de->d_name);
            inv_label_set(in.labels,&in.label_count,"ifindex",idxstr);
            result=sink->emit(sink->context,&in);
            if(result!=INV_OK)break;
        }
        if(result!=INV_OK)break;
    }
    closedir(dir);
    return result==INV_OK?incomplete:result;
}
static void net_close(inv_read_ctx *c) {
    net_data *d=(net_data *)c->data;
    if(d) {
        free(d->rows);
        free(d);
    }
    c->data=NULL;
}
inv_status inv_network_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    net_data *d=(net_data *)c->data;
    const char *a=in->metric->arg;
    size_t i;
    int field=-1;
    (void)w;
    (void)q;
    if(v->type==INV_STRING) {
        char p[INV_PATH_CAP];
        inv_status s;
        if(!inv_path(p,sizeof(p),in->resource,a))return INV_LIMIT;
        s=inv_read_text(p,v->as.string,sizeof(v->as.string));
        if(s==INV_OK)inv_trim(v->as.string);
        return s;
    }
    if(!strcmp(a,"rx_bytes"))field=0;
    else if(!strcmp(a,"rx_packets"))field=1;
    else if(!strcmp(a,"rx_errors"))field=2;
    else if(!strcmp(a,"tx_bytes"))field=8;
    else if(!strcmp(a,"tx_packets"))field=9;
    else if(!strcmp(a,"tx_errors"))field=10;
    if(field<0)return INV_UNSUPPORTED;
    for(i=0; i<d->count; i++)if(!strcmp(d->rows[i].name,inv_label_get(in->labels,in->label_count,"ifname"))) {
        v->as.u64=d->rows[i].fields[field];
        return INV_OK;
    }
    return INV_GONE;
}
const inv_provider_desc inv_network_provider= {
    4,"linux.net","/sys/class/net identity; one /proc/net/dev counters snapshot",net_open,net_discover,net_refresh,net_close
};
