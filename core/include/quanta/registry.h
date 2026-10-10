/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_REGISTRY_H
#define QUANTA_REGISTRY_H
#include "query.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define QUANTA_MAX_PROVIDERS 32u

/* A catalog is an immutable view. Distinct providers may implement the same
 * metric key. Order defines the default provider; explicit provider(...)
 * bypasses that default. Automatic runtime failover is intentionally not part
 * of this baseline. */
typedef struct quanta_catalog
{
    const quanta_metric_desc *const *metrics;
    size_t count;
} quanta_catalog;

quanta_status quanta_catalog_validate(const quanta_catalog *catalog, char *error, size_t capacity);
const quanta_metric_desc *quanta_catalog_find(const quanta_catalog *catalog, const char *key,
                                              const char *provider_id);
int quanta_catalog_selects(const quanta_catalog *catalog, const quanta_select *select,
                           const quanta_metric_desc *metric);
const char *quanta_metric_source(const quanta_metric_desc *metric);

#ifdef __cplusplus
}
#endif
#endif
