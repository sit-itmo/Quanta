/* SPDX-License-Identifier: MIT
 * Reconcile metric instances into deduplicated acquisition channels and query
 * bindings. */
#include "engine_internal.h"

quanta_status quanta_engine_announce(quanta_engine *e, quanta_binding *b)
{
    quanta_status s = QUANTA_OK;
    if (e->sink && e->sink->upsert_series)
    {
        s = e->sink->upsert_series(e->sink->context, &b->meta);
    }
    if (s == QUANTA_OK && e->sink)
    {
        b->announced = 1;
    }
    return s;
}

static quanta_status remove_binding(quanta_engine *e, quanta_binding *b)
{
    quanta_status s = QUANTA_OK;
    if (b->announced && e->sink && e->sink->remove_series)
    {
        s = e->sink->remove_series(e->sink->context, b->meta.id);
    }
    b->active = 0;
    b->announced = 0;
    return s;
}

static quanta_status reserve_channels(quanta_engine *e)
{
    if (e->channel_count == e->channel_cap)
    {
        size_t cap = e->channel_cap ? e->channel_cap * 2 : QUANTA_INITIAL_CAPACITY;
        quanta_channel *p;
        if (e->channel_count >= QUANTA_MAX_CHANNELS)
        {
            return QUANTA_LIMIT;
        }
        if (cap > QUANTA_MAX_CHANNELS)
        {
            cap = QUANTA_MAX_CHANNELS;
        }
        p = (quanta_channel *)realloc(e->channels, cap * sizeof(*p));
        if (!p)
        {
            return QUANTA_NO_MEMORY;
        }
        memset(p + e->channel_cap, 0, (cap - e->channel_cap) * sizeof(*p));
        e->channels = p;
        e->channel_cap = cap;
    }
    return QUANTA_OK;
}

static quanta_status reserve_bindings(quanta_engine *e)
{
    if (e->binding_count == e->binding_cap)
    {
        size_t cap = e->binding_cap ? e->binding_cap * 2 : QUANTA_INITIAL_CAPACITY;
        quanta_binding *p;
        if (e->binding_count >= QUANTA_MAX_SERIES)
        {
            return QUANTA_LIMIT;
        }
        if (cap > QUANTA_MAX_SERIES)
        {
            cap = QUANTA_MAX_SERIES;
        }
        p = (quanta_binding *)realloc(e->bindings, cap * sizeof(*p));
        if (!p)
        {
            return QUANTA_NO_MEMORY;
        }
        memset(p + e->binding_cap, 0, (cap - e->binding_cap) * sizeof(*p));
        e->bindings = p;
        e->binding_cap = cap;
    }
    return QUANTA_OK;
}

static quanta_status discovered(void *context, const quanta_instance *in)
{
    quanta_engine *e = (quanta_engine *)context;
    size_t i, j, ci = SIZE_MAX;
    quanta_channel *c;
    int any = 0;
    quanta_status s;
    if (!in || !in->metric || in->label_count > QUANTA_LABEL_CAP ||
        !memchr(in->entity_key, 0, sizeof(in->entity_key)) ||
        !memchr(in->resource, 0, sizeof(in->resource)))
    {
        return QUANTA_INVALID_DATA;
    }
    for (i = 0; i < e->catalog.count; ++i)
    {
        if (e->catalog.metrics[i] == in->metric)
        {
            break;
        }
    }
    if (i == e->catalog.count || !quanta_metric_wanted(&e->acquisitions[e->discovering_acquisition].context, in->metric) ||
        !in->entity_key[0])
    {
        return QUANTA_INVALID_DATA;
    }
    for (i = 0; i < in->label_count; ++i)
    {
        if (!memchr(in->labels[i].key, 0, sizeof(in->labels[i].key)) ||
            !memchr(in->labels[i].value, 0, sizeof(in->labels[i].value)) || !in->labels[i].key[0])
        {
            return QUANTA_INVALID_DATA;
        }
        for (j = 0; j < i; ++j)
        {
            if (!strcmp(in->labels[j].key, in->labels[i].key))
            {
                return QUANTA_INVALID_DATA;
            }
        }
        {
            const char *schema = in->metric->label_schema;
            size_t length = strlen(in->labels[i].key);
            int allowed = 0;
            while (*schema)
            {
                const char *end = strchr(schema, ',');
                size_t part = end ? (size_t)(end - schema) : strlen(schema);
                if (part == length && !strncmp(schema, in->labels[i].key, length))
                {
                    allowed = 1;
                }
                if (!end)
                {
                    break;
                }
                schema = end + 1;
            }
            if (!allowed)
            {
                return QUANTA_INVALID_DATA;
            }
        }
    }
    for (i = 0; i < e->plan.select_count; i++)
    {
        if (quanta_catalog_selects(&e->catalog, &e->plan.selects[i], in->metric) &&
            quanta_select_matches(&e->plan.selects[i], in))
        {
            any = 1;
            break;
        }
    }
    if (!any)
    {
        return QUANTA_OK;
    }
    for (i = 0; i < e->channel_count; i++)
    {
        if (e->channels[i].active && e->channels[i].in.metric == in->metric &&
            !strcmp(e->channels[i].in.entity_key, in->entity_key))
        {
            ci = i;
            break;
        }
    }
    if (ci == SIZE_MAX)
    {
        for (i = 0; i < e->channel_count; i++)
        {
            if (!e->channels[i].active)
            {
                ci = i;
                break;
            }
        }
        if (ci == SIZE_MAX)
        {
            s = reserve_channels(e);
            if (s != QUANTA_OK)
            {
                return s;
            }
            ci = e->channel_count++;
        }
        c = &e->channels[ci];
        memset(c, 0, sizeof(*c));
        c->in = *in;
        c->acquisition_index = e->discovering_acquisition;
        c->in.series_id = ++e->next_id;
        c->in.generation = (uint32_t)e->next_id;
        c->active = 1;
        c->last.status = QUANTA_NO_DATA;
    }
    else
    {
        c = &e->channels[ci];
        quanta_copy(c->in.resource, sizeof(c->in.resource), in->resource);
        c->in.min_poll_ns = in->min_poll_ns;
        c->in.label_count = in->label_count;
        memcpy(c->in.labels, in->labels, sizeof(in->labels));
    }
    c->seen = e->discovery_epoch;
    c->period = UINT64_MAX;
    for (i = 0; i < e->plan.select_count; i++)
    {
        quanta_select *sel = &e->plan.selects[i];
        quanta_binding *b = NULL;
        uint64_t period;
        int match =
            quanta_catalog_selects(&e->catalog, sel, in->metric) && quanta_select_matches(sel, in);
        for (j = 0; j < e->binding_count; j++)
        {
            if (e->bindings[j].active && e->bindings[j].channel_index == ci &&
                e->bindings[j].meta.selection_index == i)
            {
                b = &e->bindings[j];
                break;
            }
        }
        if (!match)
        {
            if (b)
            {
                s = remove_binding(e, b);
                if (s != QUANTA_OK)
                {
                    return s;
                }
            }
            continue;
        }
        period = QUANTA_MAX(sel->period_ns, QUANTA_MAX(in->metric->min_poll_ns, in->min_poll_ns));
        if (e->plan.strict_poll && in->metric->refresh != QUANTA_REFRESH_ONCE &&
            sel->period_ns < period)
        {
            return QUANTA_BAD_QUERY;
        }
        if (in->metric->refresh == QUANTA_REFRESH_ON_EVENT)
        {
            period = QUANTA_MAX(period, 5 * QUANTA_NS_S);
        }
        if (period < c->period)
        {
            c->period = period;
        }
        if (!b)
        {
            for (j = 0; j < e->binding_count; j++)
            {
                if (!e->bindings[j].active)
                {
                    b = &e->bindings[j];
                    break;
                }
            }
            if (!b)
            {
                s = reserve_bindings(e);
                if (s != QUANTA_OK)
                {
                    return s;
                }
                b = &e->bindings[e->binding_count++];
            }
            memset(b, 0, sizeof(*b));
            b->active = 1;
            b->channel_index = ci;
            b->previous.status = QUANTA_NO_DATA;
            b->meta.id = ++e->next_id;
            b->meta.source_id = c->in.series_id;
            b->meta.generation = c->in.generation;
            b->meta.metric = in->metric;
            b->meta.selection_index = (unsigned)i;
            b->meta.type = sel->transform == QUANTA_RATE ? QUANTA_F64 : in->metric->type;
            b->meta.transform = sel->transform;
            b->meta.view = sel->view;
            b->meta.effective_period_ns = period;
            if (sel->transform == QUANTA_RAW)
            {
                quanta_copy(b->meta.key, sizeof(b->meta.key), in->metric->key);
            }
            else
            {
                snprintf(b->meta.key, sizeof(b->meta.key), "%s(%s)",
                         sel->transform == QUANTA_RATE ? "rate" : "delta", in->metric->key);
            }
            snprintf(b->meta.unit, sizeof(b->meta.unit), "%s%s", in->metric->unit,
                     sel->transform == QUANTA_RATE ? "/s" : "");
            quanta_copy(b->meta.entity_key, sizeof(b->meta.entity_key), in->entity_key);
            b->meta.label_count = in->label_count;
            memcpy(b->meta.labels, in->labels, sizeof(in->labels));
            s = quanta_engine_announce(e, b);
            if (s != QUANTA_OK)
            {
                return s;
            }
        }
        else if (b->meta.effective_period_ns != period || b->meta.label_count != in->label_count ||
                 memcmp(b->meta.labels, in->labels, sizeof(in->labels)))
        {
            b->meta.effective_period_ns = period;
            b->meta.label_count = in->label_count;
            memcpy(b->meta.labels, in->labels, sizeof(in->labels));
            s = quanta_engine_announce(e, b);
            if (s != QUANTA_OK)
            {
                return s;
            }
        }
    }
    if (c->initialized && c->next != UINT64_MAX &&
        c->next < quanta_deadline_after(c->last.measured_at_ns, c->period))
    {
        c->next = quanta_deadline_after(c->last.measured_at_ns, c->period);
    }
    return QUANTA_OK;
}

quanta_status quanta_engine_discover(quanta_engine *e)
{
    size_t p;
    size_t i, j;
    quanta_discovery_sink sink;
    e->discovery_epoch++;
    sink.context = e;
    sink.emit = discovered;
    for (p = 0; p < e->acquisition_count; p++)
    {
        if (e->acquisitions[p].init_status == QUANTA_OK)
        {
            quanta_acquisition *a = &e->acquisitions[p];
            const quanta_metric_desc *m = a->representative;
            e->discovering_acquisition = p;
            quanta_status s = m->enumerate
                                  ? m->enumerate(&a->context, &sink)
                                  : quanta_discover_single(&a->context, &sink);
            if (s == QUANTA_NO_MEMORY || s == QUANTA_LIMIT || s == QUANTA_BAD_QUERY ||
                s == QUANTA_INVALID_DATA)
            {
                return s;
            }
            if (s != QUANTA_OK && s != a->discovery_status)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "discovery: %s: %s", m->key,
                         quanta_status_name(s));
                if (quanta_engine_diagnostic(e, s, msg) != QUANTA_OK)
                {
                    return QUANTA_IO_ERROR;
                }
            }
            a->discovery_status = s;
            /* Failed/partial enumeration is not evidence that devices disappeared. */
            if (s == QUANTA_OK)
            {
                for (i = 0; i < e->channel_count; i++)
                {
                    if (e->channels[i].active && e->channels[i].acquisition_index == p &&
                        e->channels[i].seen != e->discovery_epoch)
                    {
                        for (j = 0; j < e->binding_count; j++)
                        {
                            if (e->bindings[j].active && e->bindings[j].channel_index == i)
                            {
                                s = remove_binding(e, &e->bindings[j]);
                                if (s != QUANTA_OK)
                                {
                                    return s;
                                }
                            }
                        }
                        e->channels[i].active = 0;
                    }
                }
            }
        }
    }
    return QUANTA_OK;
}
