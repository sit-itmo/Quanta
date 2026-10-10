/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
static inv_status hw_discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    char root[INV_PATH_CAP];
    DIR *dir;
    struct dirent *de;
    inv_status result=INV_OK,incomplete=INV_OK;
    size_t count;
    const inv_metric_desc *const *r=inv_registry(&count);
    if(!inv_path(root,sizeof(root),c->paths.sys_root,"class/hwmon"))return INV_LIMIT;
    dir=opendir(root);
    if(!dir)return inv_errno_status(errno);
    while((de=readdir(dir))!=NULL) {
        char base[INV_PATH_CAP],path[INV_PATH_CAP],chip[INV_TEXT_CAP],real[PATH_MAX];
        DIR *channels;
        struct dirent *ch;
        struct stat ds;
        uint64_t interval_ms=0;
        if(strncmp(de->d_name,"hwmon",5))continue;
        if(!inv_path(base,sizeof(base),root,de->d_name)||!inv_path(path,sizeof(path),base,"name")) {
            result=INV_LIMIT;
            break;
        }
        if(inv_read_text(path,chip,sizeof(chip))!=INV_OK)inv_copy(chip,sizeof(chip),de->d_name);
        inv_trim(chip);
        if(!realpath(base,real)||stat(base,&ds)) {
            incomplete=inv_errno_status(errno);
            continue;
        }
        inv_path(path,sizeof(path),base,"update_interval");
        if(inv_read_u64(path,&interval_ms)!=INV_OK||interval_ms>604800000)interval_ms=0;
        channels=opendir(base);
        if(!channels) {
            incomplete=inv_errno_status(errno);
            continue;
        }
        while((ch=readdir(channels))!=NULL) {
            size_t j;
            for(j=0; j<count; j++)if(r[j]->provider==&inv_hwmon_provider&&inv_metric_wanted(c,r[j])) {
                const char *prefix=r[j]->arg,*num;
                size_t plen=strlen(prefix);
                char channel[48],label[INV_TEXT_CAP],attr[80];
                inv_instance in;
                if(strncmp(ch->d_name,prefix,plen))continue;
                num=ch->d_name+plen;
                if(!isdigit((unsigned char)*num))continue;
                while(isdigit((unsigned char)*num))num++;
                if(strcmp(num,"_input"))continue;
                if((size_t)(num-ch->d_name)>=sizeof(channel)) {
                    result=INV_LIMIT;
                    break;
                }
                memcpy(channel,ch->d_name,(size_t)(num-ch->d_name));
                channel[num-ch->d_name]=0;
                snprintf(attr,sizeof(attr),"%s_label",channel);
                inv_path(path,sizeof(path),base,attr);
                if(inv_read_text(path,label,sizeof(label))!=INV_OK)inv_copy(label,sizeof(label),channel);
                inv_trim(label);
                memset(&in,0,sizeof(in));
                in.metric=r[j];
                in.min_poll_ns=interval_ms*INV_NS_MS;
                if(snprintf(in.entity_key,sizeof(in.entity_key),"%s:%" PRIuMAX ":%s",real,(uintmax_t)ds.st_ino,channel)>=(int)sizeof(in.entity_key)|| !inv_path(in.resource,sizeof(in.resource),base,ch->d_name)|| !inv_label_set(in.labels,&in.label_count,"chip",chip)|| !inv_label_set(in.labels,&in.label_count,"channel",channel)|| !inv_label_set(in.labels,&in.label_count,"label",label)|| !inv_label_set(in.labels,&in.label_count,"device",real)) {
                    result=INV_LIMIT;
                    break;
                }
                result=sink->emit(sink->context,&in);
                if(result!=INV_OK)break;
            }
            if(result!=INV_OK)break;
        }
        closedir(channels);
        if(result!=INV_OK)break;
    }
    closedir(dir);
    return result==INV_OK?incomplete:result;
}
inv_status inv_hwmon_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    char b[128];
    int64_t x;
    inv_status s;
    char fault[INV_PATH_CAP];
    char *end;
    uint64_t f;
    (void)c;
    (void)w;
    (void)q;
    inv_copy(fault,sizeof(fault),in->resource);
    end=strrchr(fault,'_');
    if(end)inv_copy(end,sizeof(fault)-(size_t)(end-fault),"_fault");
    s=inv_read_u64(fault,&f);
    if(s==INV_OK&&f)return INV_NO_DATA;
    if(s!=INV_OK&&s!=INV_NOT_PRESENT)return s;
    s=inv_read_text(in->resource,b,sizeof(b));
    if(s!=INV_OK)return s;
    if(!inv_parse_s64(b,&x))return INV_INVALID_DATA;
    if(v->type==INV_F64)v->as.f64=(double)x/1000.0;
    else {
        if(x<0)return INV_INVALID_DATA;
        v->as.u64=(uint64_t)x;
    }
    return INV_OK;
}
const inv_provider_desc inv_hwmon_provider= {
    5,"linux.hwmon","Read-only standard hwmon temperature/fan channels; fault-aware",NULL,hw_discover,NULL,NULL
};
