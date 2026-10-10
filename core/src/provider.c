/* SPDX-License-Identifier: MIT */
#include "core_internal.h"
#include <ctype.h>
#include <math.h>
#include <time.h>

int quanta_select_matches_metric(const quanta_select *s, const quanta_metric_desc *m)
{
    return quanta_glob_match(s->pattern, m->key) &&
           (!s->provider[0] || !strcmp(s->provider, m->provider->id));
}

int quanta_select_matches(const quanta_select *s, const quanta_instance *in)
{
    size_t i;
    if (!quanta_select_matches_metric(s, in->metric))
    {
        return 0;
    }
    for (i = 0; i < s->label_count; i++)
    {
        const char *v = quanta_label_get(in->labels, in->label_count, s->labels[i].key);
        if (!v || !quanta_glob_match(s->labels[i].value, v))
        {
            return 0;
        }
    }
    return 1;
}

int quanta_metric_wanted(const quanta_read_ctx *c, const quanta_metric_desc *m)
{
    size_t i;
    for (i = 0; i < c->plan->select_count; i++)
    {
        if (quanta_catalog_selects(c->catalog, &c->plan->selects[i], m))
        {
            return 1;
        }
    }
    return 0;
}

quanta_status quanta_discover_single(quanta_read_ctx *c, const quanta_provider_desc *p,
                                     quanta_discovery_sink *sink)
{
    size_t n, i;
    const quanta_metric_desc *const *r = c->catalog->metrics;
    n = c->catalog->count;
    for (i = 0; i < n; i++)
    {
        if (r[i]->provider == p && quanta_metric_wanted(c, r[i]))
        {
            quanta_instance in;
            quanta_status s;
            memset(&in, 0, sizeof(in));
            in.metric = r[i];
            quanta_copy(in.entity_key, sizeof(in.entity_key), "system");
            s = sink->emit(sink->context, &in);
            if (s != QUANTA_OK)
            {
                return s;
            }
        }
    }
    return QUANTA_OK;
}
