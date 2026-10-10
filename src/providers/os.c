/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <sys/utsname.h>
#include <unistd.h>
#include <math.h>
typedef struct os_data {
    struct utsname un;
    int uname_ok;
    char name[INV_TEXT_CAP];
    double uptime,load[3];
    long online;
    inv_status uptime_status,load_status;
} os_data;
static void os_metadata(inv_read_ctx *c) {
    os_data *d=(os_data *)c->data;
    char path[INV_PATH_CAP],buf[16384];
    d->uname_ok=uname(&d->un)==0;
    inv_copy(d->name,sizeof(d->name),d->uname_ok?d->un.sysname:"Linux");
    if(inv_path(path,sizeof(path),c->paths.etc_root,"os-release")&&inv_read_text(path,buf,sizeof(buf))==INV_OK) {
        const char *p=buf;
        while(*p) {
            const char *e=strchr(p,'\n');
            size_t n=e?(size_t)(e-p):strlen(p);
            if(n>12&&!memcmp(p,"PRETTY_NAME=",12)) {
                const char *v=p+12;
                size_t len=n-12;
                if(len>=2&&((*v=='"'&&v[len-1]=='"')||(*v=='\''&&v[len-1]=='\''))) {
                    v++;
                    len-=2;
                }
                if(len<sizeof(d->name)) {
                    memcpy(d->name,v,len);
                    d->name[len]=0;
                }
                break;
            }
            if(!e)break;
            p=e+1;
        }
    }
}
static inv_status os_open(inv_read_ctx *c) {
    c->data=calloc(1,sizeof(os_data));
    return c->data?INV_OK:INV_NO_MEMORY;
}
static inv_status os_discover(inv_read_ctx *c,inv_discovery_sink *s) {
    return inv_discover_single(c,&inv_os_provider,s);
}
static inv_status os_refresh(inv_read_ctx *c) {
    os_data *d=(os_data *)c->data;
    char path[INV_PATH_CAP],b[1024];
    os_metadata(c);
    inv_path(path,sizeof(path),c->paths.proc_root,"uptime");
    d->uptime_status=inv_read_text(path,b,sizeof(b));
    if(d->uptime_status==INV_OK&&(sscanf(b,"%lf",&d->uptime)!=1||!isfinite(d->uptime)||d->uptime<0))d->uptime_status=INV_INVALID_DATA;
    inv_path(path,sizeof(path),c->paths.proc_root,"loadavg");
    d->load_status=inv_read_text(path,b,sizeof(b));
    if(d->load_status==INV_OK&&(sscanf(b,"%lf %lf %lf",&d->load[0],&d->load[1],&d->load[2])!=3||!isfinite(d->load[0])||!isfinite(d->load[1])||d->load[0]<0||d->load[1]<0))d->load_status=INV_INVALID_DATA;
    d->online=sysconf(_SC_NPROCESSORS_ONLN);
    return INV_OK;
}
static void os_close(inv_read_ctx *c) {
    free(c->data);
    c->data=NULL;
}
inv_status inv_os_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    os_data *d=(os_data *)c->data;
    const char *a=in->metric->arg,*s=NULL;
    (void)w;
    (void)q;
    if(!strcmp(a,"name"))s=d->name;
    else if(!strcmp(a,"release"))s=d->un.release;
    else if(!strcmp(a,"version"))s=d->un.version;
    else if(!strcmp(a,"arch"))s=d->un.machine;
    else if(!strcmp(a,"hostname"))s=d->un.nodename;
    if(s) {
        if(!d->uname_ok&&strcmp(a,"name"))return INV_IO_ERROR;
        return inv_copy(v->as.string,sizeof(v->as.string),s)?INV_OK:INV_LIMIT;
    }
    if(!strcmp(a,"uptime")) {
        v->as.f64=d->uptime;
        return d->uptime_status;
    }
    if(!strcmp(a,"load1")||!strcmp(a,"load5")) {
        v->as.f64=d->load[!strcmp(a,"load5")?1:0];
        return d->load_status;
    }
    if(!strcmp(a,"online")) {
        if(d->online<0)return INV_NO_DATA;
        v->as.u64=(uint64_t)d->online;
        return INV_OK;
    }
    return INV_UNSUPPORTED;
}
const inv_provider_desc inv_os_provider= {
    0,"linux.os","uname, /etc/os-release, /proc/uptime, /proc/loadavg",os_open,os_discover,os_refresh,os_close
};
