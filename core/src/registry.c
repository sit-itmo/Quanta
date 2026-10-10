/**
 * @file core/src/registry.c
 *
 * Validate catalogs, look up metrics by name and match descriptors to select statements.
 *
 * Catalog: an immutable set of metric descriptors with unique names. It neither stores
 * measurement results nor selects providers.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "core_internal.h"

static quanta_status invalid(char *error, size_t capacity, const char *reason, size_t index)
{
    if (error && capacity) quanta_snprintf(error, capacity, "%s: entry %zu", reason, index);
    return QUANTA_INVALID_DATA;
}

quanta_status quanta_catalog_validate(const quanta_catalog *catalog, char *error, size_t capacity)
{
    size_t i, j;
    if (error && capacity) error[0] = '\0';
    if (!catalog || (catalog->count && !catalog->metrics) || catalog->count > QUANTA_MAX_CHANNELS)
        return invalid(error, capacity, "invalid catalog", 0);
    for (i = 0; i < catalog->count; ++i)
    {
        const quanta_metric_desc *m = catalog->metrics[i];
        if (!m || !m->key || !m->key[0] || quanta_strlen(m->key) >= QUANTA_KEY_CAP || !m->title ||
            !m->description || !m->doc_path || !m->unit || !m->label_schema || !m->dependencies ||
            !m->arg || !m->read || (unsigned)m->type > QUANTA_BYTES ||
            (unsigned)m->kind > QUANTA_COUNTER || (unsigned)m->refresh > QUANTA_REFRESH_ON_EVENT)
            return invalid(error, capacity, "invalid descriptor", i);
        if (m->refresh != QUANTA_REFRESH_ONCE && !m->min_poll_ns)
            return invalid(error, capacity, "missing polling floor", i);
        for (j = 0; j < i; ++j)
        {
            if (!quanta_strcmp(m->key, catalog->metrics[j]->key))
                return invalid(error, capacity, "duplicate metric name (use a distinct method-specific key)", i);
        }
    }
    return QUANTA_OK;
}

const quanta_metric_desc *quanta_catalog_find(const quanta_catalog *catalog, const char *key)
{
    size_t i;
    if (!catalog || !key) return NULL;
    for (i = 0; i < catalog->count; ++i)
        if (!quanta_strcmp(catalog->metrics[i]->key, key)) return catalog->metrics[i];
    return NULL;
}

int quanta_catalog_selects(const quanta_catalog *catalog, const quanta_select *select,
                          const quanta_metric_desc *metric)
{
    (void)catalog;
    return quanta_select_matches_metric(select, metric);
}

const char *quanta_metric_source(const quanta_metric_desc *metric)
{
    return metric && metric->dependencies ? metric->dependencies : "unknown";
}
