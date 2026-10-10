/* SPDX-License-Identifier: MIT */
#include "collector_internal.h"
#define QUANTA_METRIC(symbol, ...) static const quanta_metric_desc symbol = {__VA_ARGS__};
#include "selected.def"
#undef QUANTA_METRIC
static const quanta_metric_desc *const records[] = {
#define QUANTA_METRIC(symbol, ...) &symbol,
#include "selected.def"
#undef QUANTA_METRIC
};
static const quanta_catalog builtin_catalog = {records, QUANTA_ARRAY_SIZE(records)};

const quanta_catalog *quanta_builtin_catalog(void)
{
    return &builtin_catalog;
}

const quanta_metric_desc *const *quanta_registry(size_t *count)
{
    if (count)
    {
        *count = builtin_catalog.count;
    }
    return records;
}

const quanta_metric_desc *quanta_metric_find(const char *key)
{
    return quanta_catalog_find(&builtin_catalog, key);
}

quanta_status quanta_registry_validate(char *error, size_t capacity)
{
    return quanta_catalog_validate(&builtin_catalog, error, capacity);
}

quanta_status quanta_plan_validate(quanta_plan *plan, char *error, size_t capacity)
{
    return quanta_plan_validate_with_catalog(plan, &builtin_catalog, error, capacity);
}

quanta_status quanta_engine_create(const quanta_plan *plan, const quanta_paths *paths,
                                   quanta_engine **out, char *error, size_t capacity)
{
    return quanta_engine_create_with_catalog(plan, paths, &builtin_catalog, out, error, capacity);
}
