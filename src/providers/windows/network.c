/* SPDX-License-Identifier: MIT */
#include "windows_internal.h"
#include <iphlpapi.h> /* Includes netioapi.h; do not include netioapi.h directly. */
typedef struct network_data { MIB_IF_TABLE2 *table; inv_status status; } network_data;
static void guid_text(char *out,size_t cap,const GUID *g) {
    snprintf(out,cap,"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
       (unsigned long)g->Data1,(unsigned)g->Data2,(unsigned)g->Data3,
       (unsigned)g->Data4[0],(unsigned)g->Data4[1],(unsigned)g->Data4[2],(unsigned)g->Data4[3],
       (unsigned)g->Data4[4],(unsigned)g->Data4[5],(unsigned)g->Data4[6],(unsigned)g->Data4[7]);
}
static inv_status open_provider(inv_read_ctx *c) { c->data=calloc(1,sizeof(network_data)); return c->data?INV_OK:INV_NO_MEMORY; }
static inv_status refresh(inv_read_ctx *c) {
    network_data *p=(network_data *)c->data;
    MIB_IF_TABLE2 *table=NULL;
    DWORD error=GetIfTable2(&table);
    p->status=inv_win_status(error);
    if(error!=NO_ERROR) { inv_win_error(c,error,"GetIfTable2"); return p->status; }
    if(!table || table->NumEntries>4096) { if(table)FreeMibTable(table); p->status=INV_LIMIT; return p->status; }
    if(p->table)FreeMibTable(p->table);
    p->table=table; return INV_OK;
}
static inv_status discover(inv_read_ctx *c,inv_discovery_sink *sink) {
    network_data *p=(network_data *)c->data;
    const inv_metric_desc *const *registry;
    size_t count,i;
    ULONG j;
    inv_status st=refresh(c);
    if(st!=INV_OK)return st;
    registry=inv_registry(&count);
    for(j=0;j<p->table->NumEntries;j++) {
        const MIB_IF_ROW2 *row=&p->table->Table[j];
        /* GetIfTable2 also reports NDIS filter-module interfaces (WFP, Npcap,
         * VirtualBox filters, etc.). They are implementation layers, not
         * user-facing adapters, and would duplicate the same NIC many times. */
        if(row->InterfaceAndOperStatusFlags.FilterInterface)continue;
        char guid[64],name[INV_TEXT_CAP],index[32];
        guid_text(guid,sizeof(guid),&row->InterfaceGuid);
        if(!inv_win_utf8(name,sizeof(name),row->Alias))return INV_LIMIT;
        snprintf(index,sizeof(index),"%lu",(unsigned long)row->InterfaceIndex);
        if(!name[0])snprintf(name,sizeof(name),"ifindex-%lu",(unsigned long)row->InterfaceIndex);
        for(i=0;i<count;i++)if(registry[i]->provider==&inv_network_provider && inv_metric_wanted(c,registry[i])) {
            inv_instance in;
            memset(&in,0,sizeof(in)); in.metric=registry[i];
            /* A rename updates labels; recreation with another LUID starts a new series. */
            snprintf(in.entity_key,sizeof(in.entity_key),"net:%s:%" PRIu64,guid,(uint64_t)row->InterfaceLuid.Value);
            snprintf(in.resource,sizeof(in.resource),"%" PRIu64,(uint64_t)row->InterfaceLuid.Value);
            if(!inv_label_set(in.labels,&in.label_count,"ifname",name)||
               !inv_label_set(in.labels,&in.label_count,"ifindex",index)||
               !inv_label_set(in.labels,&in.label_count,"guid",guid))return INV_LIMIT;
            st=sink->emit(sink->context,&in); if(st!=INV_OK)return st;
        }
    }
    return INV_OK;
}
static void close_provider(inv_read_ctx *c) {
    network_data *p=(network_data *)c->data;
    if(p && p->table)FreeMibTable(p->table);
    free(p); c->data=NULL;
}
static const char *state(IF_OPER_STATUS value) {
    switch(value) {
    case IfOperStatusUp:return "up";
    case IfOperStatusDown:return "down";
    case IfOperStatusTesting:return "testing";
    case IfOperStatusDormant:return "dormant";
    case IfOperStatusNotPresent:return "notpresent";
    case IfOperStatusLowerLayerDown:return "lowerlayerdown";
    default:return "unknown";
    }
}
inv_status inv_network_read(inv_read_ctx *c,inv_instance *in,inv_value *v,uint64_t *w,uint32_t *q) {
    network_data *p=(network_data *)c->data;
    const MIB_IF_ROW2 *r=NULL;
    const char *a=in->metric->arg;
    uint64_t luid;
    ULONG i;
    (void)w; (void)q;
    if(p->status!=INV_OK)return p->status;
    if(!inv_parse_u64(in->resource,&luid))return INV_INVALID_DATA;
    for(i=0;i<p->table->NumEntries;i++)if((uint64_t)p->table->Table[i].InterfaceLuid.Value==luid) { r=&p->table->Table[i]; break; }
    if(!r)return INV_GONE;
    if(!strcmp(a,"address")) {
        size_t n=0;
        if(!r->PhysicalAddressLength)return INV_NO_DATA;
        if(r->PhysicalAddressLength>IF_MAX_PHYS_ADDRESS_LENGTH)return INV_INVALID_DATA;
        for(i=0;i<r->PhysicalAddressLength;i++) {
            int len=snprintf(v->as.string+n,sizeof(v->as.string)-n,"%s%02x",i?":":"",(unsigned)r->PhysicalAddress[i]);
            if(len<0 || (size_t)len>=sizeof(v->as.string)-n)return INV_LIMIT;
            n+=(size_t)len;
        }
    } else if(!strcmp(a,"operstate"))return inv_copy(v->as.string,sizeof(v->as.string),state(r->OperStatus))?INV_OK:INV_LIMIT;
    else if(!strcmp(a,"rx_bytes"))v->as.u64=r->InOctets;
    else if(!strcmp(a,"tx_bytes"))v->as.u64=r->OutOctets;
    else if(!strcmp(a,"rx_packets")) {
        if(r->InUcastPkts>UINT64_MAX-r->InNUcastPkts)return INV_INVALID_DATA;
        v->as.u64=r->InUcastPkts+r->InNUcastPkts;
    } else if(!strcmp(a,"tx_packets")) {
        if(r->OutUcastPkts>UINT64_MAX-r->OutNUcastPkts)return INV_INVALID_DATA;
        v->as.u64=r->OutUcastPkts+r->OutNUcastPkts;
    } else if(!strcmp(a,"rx_errors"))v->as.u64=r->InErrors;
    else if(!strcmp(a,"tx_errors"))v->as.u64=r->OutErrors;
    else if(!strcmp(a,"rx_link"))v->as.u64=r->ReceiveLinkSpeed;
    else if(!strcmp(a,"tx_link"))v->as.u64=r->TransmitLinkSpeed;
    else return INV_UNSUPPORTED;
    return INV_OK;
}
const inv_provider_desc inv_network_provider={4,"windows.network","One GetIfTable2 snapshot for user-facing interfaces; NDIS filter-module interfaces are excluded; 64-bit counters",open_provider,discover,refresh,close_provider};
