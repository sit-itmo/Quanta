/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_REGISTRY_H
#define QUANTA_REGISTRY_H
#include "query.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Immutable unique-name catalog. Alternative measurement methods must have
 * distinct metric keys; registration order cannot choose an implementation. */
typedef struct quanta_catalog
{
    const quanta_metric_desc *const *metrics;
    size_t count;
} quanta_catalog;

quanta_status quanta_catalog_validate(const quanta_catalog *catalog, char *error, size_t capacity);
const quanta_metric_desc *quanta_catalog_find(const quanta_catalog *catalog, const char *key);
int quanta_catalog_selects(const quanta_catalog *catalog, const quanta_select *select,
                           const quanta_metric_desc *metric);
const char *quanta_metric_source(const quanta_metric_desc *metric);

#ifdef __cplusplus
}
#endif
#endif
