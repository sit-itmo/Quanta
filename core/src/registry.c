/* SPDX-License-Identifier: MIT */
#include "core_internal.h"

static int invalid(char *error, size_t capacity, const char *reason, size_t index)
{
    if (error && capacity)
    {
        snprintf(error, capacity, "%s: entry %zu", reason, index);
    }
    return QUANTA_INVALID_DATA;
}

quanta_status quanta_catalog_validate(const quanta_catalog *catalog, char *error, size_t capacity)
{
    size_t i, j;
    if (error && capacity)
    {
        error[0] = '\0';
    }
    if (!catalog || (catalog->count && !catalog->metrics) || catalog->count > QUANTA_MAX_CHANNELS)
    {
        return (quanta_status)invalid(error, capacity, "invalid catalog", 0);
    }
    for (i = 0; i < catalog->count; ++i)
    {
        const quanta_metric_desc *m = catalog->metrics[i];
        if (!m || !m->key || !m->key[0] || strlen(m->key) >= QUANTA_KEY_CAP || !m->title ||
            !m->description || !m->doc_path || !m->unit || !m->label_schema || !m->dependencies ||
            !m->arg || !m->provider || !m->read || !m->provider->id || !m->provider->id[0] ||
            strlen(m->provider->id) >= QUANTA_NAME_CAP ||
            m->provider->index >= QUANTA_MAX_PROVIDERS || (unsigned)m->type > QUANTA_BYTES ||
            (unsigned)m->kind > QUANTA_COUNTER || (unsigned)m->refresh > QUANTA_REFRESH_ON_EVENT)
        {
            return (quanta_status)invalid(error, capacity, "invalid descriptor", i);
        }
        if (m->refresh != QUANTA_REFRESH_ONCE && !m->min_poll_ns)
        {
            return (quanta_status)invalid(error, capacity, "missing polling floor", i);
        }
        for (j = 0; j < i; ++j)
        {
            const quanta_metric_desc *p = catalog->metrics[j];
            if (m->provider != p->provider && (m->provider->index == p->provider->index ||
                                               !strcmp(m->provider->id, p->provider->id)))
            {
                return (quanta_status)invalid(error, capacity, "provider identity collision", i);
            }
            if (!strcmp(m->key, p->key))
            {
                if (m->provider == p->provider)
                {
                    return (quanta_status)invalid(error, capacity, "duplicate key/provider", i);
                }
                if (m->type != p->type || m->kind != p->kind || strcmp(m->unit, p->unit) ||
                    strcmp(m->label_schema, p->label_schema) ||
                    m->semantic_version != p->semantic_version)
                {
                    return (quanta_status)invalid(error, capacity,
                                                  "incompatible provider semantics", i);
                }
            }
        }
    }
    return QUANTA_OK;
}

const quanta_metric_desc *quanta_catalog_find(const quanta_catalog *catalog, const char *key,
                                              const char *provider_id)
{
    size_t i;
    if (!catalog || !key)
    {
        return NULL;
    }
    for (i = 0; i < catalog->count; ++i)
    {
        const quanta_metric_desc *m = catalog->metrics[i];
        if (!strcmp(m->key, key) &&
            (!provider_id || !*provider_id || !strcmp(m->provider->id, provider_id)))
        {
            return m;
        }
    }
    return NULL;
}

int quanta_catalog_selects(const quanta_catalog *catalog, const quanta_select *select,
                           const quanta_metric_desc *metric)
{
    if (!quanta_select_matches_metric(select, metric))
    {
        return 0;
    }
    return select->provider[0] || quanta_catalog_find(catalog, metric->key, NULL) == metric;
}

const char *quanta_metric_source(const quanta_metric_desc *metric)
{
    return metric && metric->provider ? metric->provider->id : "unknown";
}
