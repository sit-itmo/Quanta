/* SPDX-License-Identifier: MIT */
#include "internal.h"
#define INV_METRIC(symbol, ...) static const inv_metric_desc symbol={__VA_ARGS__};
#include "selected.def"
#undef INV_METRIC
static const inv_metric_desc *const records[]= {
#define INV_METRIC(symbol, ...) &symbol,
#include "selected.def"
#undef INV_METRIC
};
const inv_metric_desc *const *inv_registry(size_t *count) {
    if(count)*count=INV_ARRAY_SIZE(records);
    return records;
}
const inv_metric_desc *inv_metric_find(const char *key) {
    size_t i;
    for(i=0; i<INV_ARRAY_SIZE(records); i++)if(!strcmp(key,records[i]->key))return records[i];
    return NULL;
}
inv_status inv_registry_validate(char *e,size_t cap) {
    size_t i,j;
    for(i=0; i<INV_ARRAY_SIZE(records); i++) {
        const inv_metric_desc *m=records[i];
        if(!m->key||!m->title||!m->doc_path||!m->provider||!m->read||m->provider->index>=INV_PROVIDER_COUNT) {
            snprintf(e,cap,"invalid registry entry %zu",i);
            return INV_INVALID_DATA;
        }
        if(m->refresh!=INV_REFRESH_ONCE&&!m->min_poll_ns) {
            snprintf(e,cap,"missing poll floor: %s",m->key);
            return INV_INVALID_DATA;
        }
        for(j=0; j<i; j++) {
            if(!strcmp(m->key,records[j]->key)) {
                snprintf(e,cap,"duplicate metric: %s",m->key);
                return INV_INVALID_DATA;
            }
            if(m->provider!=records[j]->provider&&m->provider->index==records[j]->provider->index) {
                snprintf(e,cap,"provider index collision: %s",m->key);
                return INV_INVALID_DATA;
            }
        }
    }
    return INV_OK;
}
