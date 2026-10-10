/* SPDX-License-Identifier: MIT */
#include "pdh_set.h"
#include <math.h>
#define MAX_COUNTERS 4096u
#define MAX_PDH_BUFFER (4u*1024u*1024u)
inv_status inv_pdh_status(PDH_STATUS status) {
    if(status==ERROR_SUCCESS || status==PDH_CSTATUS_NEW_DATA)return INV_OK;
    if(status==PDH_CSTATUS_NO_OBJECT || status==PDH_CSTATUS_NO_COUNTER)return INV_UNSUPPORTED;
    if(status==PDH_CSTATUS_NO_INSTANCE)return INV_GONE;
    if(status==PDH_NO_DATA)return INV_NO_DATA;
    if(status==PDH_INVALID_DATA)return INV_WARMUP;
    if(status==PDH_CSTATUS_INVALID_DATA)return INV_INVALID_DATA;
#ifdef PDH_ACCESS_DENIED
    if(status==PDH_ACCESS_DENIED)return INV_PERMISSION_DENIED;
#endif
#ifdef PDH_CSTATUS_NO_MACHINE
    if(status==PDH_CSTATUS_NO_MACHINE)return INV_NOT_PRESENT;
#endif
    if(status==PDH_MEMORY_ALLOCATION_FAILURE)return INV_NO_MEMORY;
    if(status==PDH_MORE_DATA)return INV_LIMIT;
    return inv_win_status((DWORD)status);
}
inv_status inv_pdh_open(inv_pdh_set *s,const wchar_t *path,int delta) {
    PDH_STATUS status;
    memset(s,0,sizeof(*s)); s->english_path=path; s->delta_counter=delta;
    status=PdhOpenQueryW(NULL,0,&s->query);
    return inv_pdh_status(status);
}
static inv_status instance_name(const wchar_t *path,char *out,size_t cap) {
    PDH_COUNTER_PATH_ELEMENTS_W *elements;
    DWORD size=0;
    PDH_STATUS st=PdhParseCounterPathW(path,NULL,&size,0);
    inv_status result=INV_OK;
    if(st!=PDH_MORE_DATA)return inv_pdh_status(st);
    if(size>MAX_PDH_BUFFER || size<sizeof(*elements))return INV_LIMIT;
    elements=(PDH_COUNTER_PATH_ELEMENTS_W *)malloc(size);
    if(!elements)return INV_NO_MEMORY;
    st=PdhParseCounterPathW(path,elements,&size,0);
    if(st!=ERROR_SUCCESS)result=inv_pdh_status(st);
    else if(!elements->szInstanceName)result=INV_NO_DATA;
    else if(!inv_win_utf8(out,cap,elements->szInstanceName))result=INV_LIMIT;
    else if(elements->dwInstanceIndex && elements->dwInstanceIndex!=((DWORD)~(DWORD)0)) {
        size_t n=strlen(out);
        int written=snprintf(out+n,cap-n,"#%lu",(unsigned long)elements->dwInstanceIndex);
        if(written<0 || (size_t)written>=cap-n)result=INV_LIMIT;
    }
    free(elements); return result;
}
static inv_status add_path(inv_pdh_set *s,const wchar_t *path) {
    size_t i,chars;
    inv_pdh_item *item;
    inv_status status;
    PDH_STATUS st;
    for(i=0;i<s->count;i++)if(!wcscmp(s->items[i].path,path)) { s->items[i].seen=1; return INV_OK; }
    if(s->count==MAX_COUNTERS)return INV_LIMIT;
    if(s->count==s->capacity) {
        size_t capacity=s->capacity?s->capacity*2:16;
        inv_pdh_item *items=(inv_pdh_item *)realloc(s->items,capacity*sizeof(*items));
        if(!items)return INV_NO_MEMORY;
        s->items=items; s->capacity=capacity;
    }
    item=&s->items[s->count]; memset(item,0,sizeof(*item));
    status=instance_name(path,item->name,sizeof(item->name));
    if(status!=INV_OK)return status;
    chars=wcslen(path)+1;
    item->path=(wchar_t *)malloc(chars*sizeof(wchar_t));
    if(!item->path)return INV_NO_MEMORY;
    memcpy(item->path,path,chars*sizeof(wchar_t));
    st=PdhAddCounterW(s->query,path,0,&item->counter);
    if(st!=ERROR_SUCCESS) { free(item->path); item->path=NULL; return inv_pdh_status(st); }
    item->seen=1; item->status=INV_NO_DATA; s->count++;
    return INV_OK;
}
inv_status inv_pdh_discover(inv_pdh_set *s) {
    PDH_HCOUNTER probe=NULL;
    PDH_COUNTER_INFO_W *info=NULL;
    wchar_t *expanded=NULL;
    DWORD size=0,chars=0;
    PDH_STATUS st;
    inv_status result=INV_OK;
    size_t i,offset;
    unsigned retry;
    if(!s->query)return INV_UNSUPPORTED;
    /* The Microsoft-documented localization + wildcard expansion sequence.
       This works on non-English Windows without translated counter names. */
    st=PdhAddEnglishCounterW(s->query,s->english_path,0,&probe);
    if(st!=ERROR_SUCCESS)return inv_pdh_status(st);
    st=PdhGetCounterInfoW(probe,FALSE,&size,NULL);
    if(st!=PDH_MORE_DATA) { result=inv_pdh_status(st); goto done; }
    if(size<sizeof(*info) || size>MAX_PDH_BUFFER) { result=INV_LIMIT; goto done; }
    info=(PDH_COUNTER_INFO_W *)malloc(size);
    if(!info) { result=INV_NO_MEMORY; goto done; }
    st=PdhGetCounterInfoW(probe,FALSE,&size,info);
    if(st!=ERROR_SUCCESS) { result=inv_pdh_status(st); goto done; }
    for(retry=0;retry<3;retry++) {
        chars=0;
        st=PdhExpandWildCardPathW(NULL,info->szFullPath,NULL,&chars,PDH_REFRESHCOUNTERS);
        if(st!=PDH_MORE_DATA) { result=inv_pdh_status(st); goto done; }
        if(chars<2 || chars>MAX_PDH_BUFFER/sizeof(wchar_t)) { result=INV_LIMIT; goto done; }
        expanded=(wchar_t *)calloc(chars,sizeof(wchar_t));
        if(!expanded) { result=INV_NO_MEMORY; goto done; }
        st=PdhExpandWildCardPathW(NULL,info->szFullPath,expanded,&chars,PDH_REFRESHCOUNTERS);
        if(st!=PDH_MORE_DATA)break;
        free(expanded); expanded=NULL;
    }
    if(st!=ERROR_SUCCESS) { result=inv_pdh_status(st); goto done; }
    if(!expanded) { result=INV_NO_DATA; goto done; }
    for(i=0;i<s->count;i++)s->items[i].seen=0;
    offset=0;
    while(offset<chars && expanded[offset]) {
        const wchar_t *end=wmemchr(expanded+offset,0,(size_t)chars-offset);
        if(!end) { result=INV_INVALID_DATA; goto done; }
        result=add_path(s,expanded+offset);
        if(result!=INV_OK)goto done;
        offset=(size_t)(end-expanded)+1;
    }
    /* Remove only after a complete enumeration, preserving live baselines. */
    for(i=0;i<s->count;) {
        if(s->items[i].seen) { i++; continue; }
        PdhRemoveCounter(s->items[i].counter); free(s->items[i].path);
        memmove(s->items+i,s->items+i+1,(s->count-i-1)*sizeof(*s->items)); s->count--;
    }
done:
    if(probe)PdhRemoveCounter(probe);
    free(info); free(expanded);
    return result;
}
inv_status inv_pdh_collect(inv_pdh_set *s) {
    PDH_STATUS st;
    uint64_t now;
    size_t i;
    if(!s->query)return INV_UNSUPPORTED;
    if(!s->count)return INV_NO_DATA;
    st=PdhCollectQueryData(s->query); now=inv_now_ns();
    for(i=0;i<s->count;i++) {
        inv_pdh_item *item=&s->items[i];
        PDH_FMT_COUNTERVALUE value;
        PDH_STATUS status=st;
        item->window_ns=item->collected_ns && now>item->collected_ns?now-item->collected_ns:0;
        memset(&value,0,sizeof(value));
        if(status==ERROR_SUCCESS)status=PdhGetFormattedCounterValue(item->counter,
            PDH_FMT_DOUBLE|PDH_FMT_NOSCALE|PDH_FMT_NOCAP100,NULL,&value);
        item->status=inv_pdh_status(status);
        if(item->status==INV_OK)item->status=inv_pdh_status((PDH_STATUS)value.CStatus);
        if(st==ERROR_SUCCESS && !item->samples && s->delta_counter)item->status=INV_WARMUP;
        if(item->status==INV_OK) {
            item->value=value.doubleValue;
            if(!isfinite(item->value))item->status=INV_INVALID_DATA;
        }
        if(st==ERROR_SUCCESS) { if(item->samples<UINT_MAX)item->samples++; item->collected_ns=now; }
        else { item->samples=0; item->collected_ns=0; }
    }
    return inv_pdh_status(st);
}
const inv_pdh_item *inv_pdh_find(const inv_pdh_set *s,const char *name) {
    size_t i;
    for(i=0;i<s->count;i++)if(!strcmp(s->items[i].name,name))return &s->items[i];
    return NULL;
}
void inv_pdh_close(inv_pdh_set *s) {
    size_t i;
    if(s->query)PdhCloseQuery(s->query);
    for(i=0;i<s->count;i++)free(s->items[i].path);
    free(s->items); memset(s,0,sizeof(*s));
}
