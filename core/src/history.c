/**
 * @file core/src/history.c
 *
 * Consumer data model: update series, track the latest value and maintain bounded history.
 *
 * History: bounded storage for a series of samples. The latest sample and the latest successful
 * sample may differ.
 *
 * Series: a particular metric instance with labels and an identity. Replacing a device may start
 * a new series even when its name is unchanged.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "quanta/history.h"
#include "core_internal.h"

void quanta_model_init(quanta_model *m, const quanta_plan *p)
{
    if (!m || !p)
    {
        return;
    }
    quanta_memset(m, 0, sizeof(*m));
    m->history_ns = p->history_ns;
    m->redact = p->redact;
}

void quanta_model_destroy(quanta_model *m)
{
    size_t i;
    if (!m)
    {
        return;
    }
    for (i = 0; i < m->count; i++)
    {
        quanta_free(m->items[i].history);
    }
    quanta_free(m->items);
    quanta_memset(m, 0, sizeof(*m));
}

quanta_item *quanta_model_find(quanta_model *m, uint64_t id)
{
    size_t i;
    if (!m)
    {
        return NULL;
    }
    for (i = 0; i < m->count; i++)
    {
        if (m->items[i].active && m->items[i].meta.id == id)
        {
            return &m->items[i];
        }
    }
    return NULL;
}

static quanta_status model_begin(void *c, const quanta_plan *p)
{
    (void)c;
    (void)p;
    return QUANTA_OK;
}

static quanta_status resize_history(quanta_model *model, quanta_item *item,
                                    const quanta_series *series)
{
    uint64_t points = 0;
    size_t capacity, bytes, old_bytes, retained;
    quanta_point *history = NULL;
    if (series->view.widget == QUANTA_WIDGET_PLOT)
    {
        points = model->history_ns / QUANTA_MAX(series->effective_period_ns, QUANTA_NS_MS) + 2;
    }
    capacity = (size_t)QUANTA_MIN(points, QUANTA_HISTORY_MAX);
    if (capacity == item->history_cap)
    {
        return QUANTA_OK;
    }
    old_bytes = item->history_cap * sizeof(quanta_point);
    bytes = capacity * sizeof(quanta_point);
    if (model->history_bytes < old_bytes ||
        model->history_bytes - old_bytes > QUANTA_HISTORY_BUDGET ||
        bytes > QUANTA_HISTORY_BUDGET - (model->history_bytes - old_bytes))
    {
        return QUANTA_LIMIT;
    }
    if (capacity)
    {
        history = (quanta_point *)quanta_calloc(capacity, sizeof(*history));
        if (!history)
        {
            return QUANTA_NO_MEMORY;
        }
    }
    retained = QUANTA_MIN(item->history_count, capacity);
    for (size_t i = 0; i < retained; ++i)
    {
        history[i] = *quanta_history_at(item, item->history_count - retained + i);
    }
    quanta_free(item->history);
    item->history = history;
    item->history_cap = capacity;
    item->history_count = retained;
    item->history_head = 0;
    model->history_bytes = model->history_bytes - old_bytes + bytes;
    if (points > QUANTA_HISTORY_MAX)
    {
        quanta_snprintf(model->diagnostic, sizeof(model->diagnostic),
                 "History capped at %u points per plot", QUANTA_HISTORY_MAX);
    }
    return QUANTA_OK;
}

static quanta_status model_upsert(void *context, const quanta_series *series)
{
    quanta_model *model = (quanta_model *)context;
    quanta_item *item;
    quanta_status status;
    if (!model || !series || !series->metric || series->label_count > QUANTA_LABEL_CAP ||
        (unsigned)series->type > QUANTA_BYTES ||
        (unsigned)series->view.widget > QUANTA_WIDGET_TABLE)
    {
        return QUANTA_INVALID_DATA;
    }
    item = quanta_model_find(model, series->id);
    if (item)
    {
        if (item->meta.type != series->type || item->meta.metric != series->metric)
        {
            return QUANTA_INVALID_DATA;
        }
        status = resize_history(model, item, series);
        if (status == QUANTA_OK)
        {
            item->meta = *series;
        }
        return status;
    }
    for (size_t i = 0; i < model->count; ++i)
    {
        if (!model->items[i].active)
        {
            item = &model->items[i];
            break;
        }
    }
    if (!item)
    {
        if (model->count == model->cap)
        {
            size_t capacity = model->cap ? model->cap * 2 : 32;
            quanta_item *items;
            if (model->count >= QUANTA_MAX_SERIES)
            {
                return QUANTA_LIMIT;
            }
            if (capacity > QUANTA_MAX_SERIES)
            {
                capacity = QUANTA_MAX_SERIES;
            }
            items = (quanta_item *)quanta_realloc(model->items, capacity * sizeof(*items));
            if (!items)
            {
                return QUANTA_NO_MEMORY;
            }
            quanta_memset(items + model->cap, 0, (capacity - model->cap) * sizeof(*items));
            model->items = items;
            model->cap = capacity;
        }
        item = &model->items[model->count++];
    }
    status = resize_history(model, item, series);
    if (status != QUANTA_OK)
    {
        return status;
    }
    item->active = 1;
    item->meta = *series;
    item->last.status = QUANTA_NO_DATA;
    return QUANTA_OK;
}

static quanta_status model_remove(void *c, uint64_t id)
{
    quanta_model *m = (quanta_model *)c;
    quanta_item *p = quanta_model_find(m, id);
    if (p)
    {
        m->history_bytes -= p->history_cap * sizeof(quanta_point);
        quanta_free(p->history);
        quanta_memset(p, 0, sizeof(*p));
    }
    return QUANTA_OK;
}

static quanta_status model_frame(void *c, const quanta_frame_info *f)
{
    quanta_model *m = (quanta_model *)c;
    m->frame = *f;
    m->frames++;
    return QUANTA_OK;
}

static quanta_status model_sample(void *c, const quanta_sample *s)
{
    quanta_model *m = (quanta_model *)c;
    quanta_item *p;
    double n = 0;
    if (!m || !s)
    {
        return QUANTA_INVALID_DATA;
    }
    p = quanta_model_find(m, s->series_id);
    if (!p || !s || (unsigned)s->status > QUANTA_BAD_QUERY)
    {
        return QUANTA_INVALID_DATA;
    }
    if (s->status == QUANTA_OK &&
        (s->value.type != p->meta.type || quanta_value_validate(&s->value) != QUANTA_OK))
    {
        return QUANTA_INVALID_DATA;
    }
    if (p->has_last && s->sequence > p->last.sequence && s->measured_at_ns < p->last.measured_at_ns)
    {
        return QUANTA_INVALID_DATA;
    }
    if (p->has_last && s->sequence <= p->last.sequence)
    {
        return QUANTA_OK;
    }
    p->last = *s;
    p->has_last = 1;
    m->samples++;
    if (s->status == QUANTA_OK)
    {
        p->good = *s;
        p->has_good = 1;
    }
    if (p->history_cap)
    {
        quanta_point point;
        size_t at;
        point.time_ns = s->measured_at_ns;
        point.status = s->status;
        if (s->status == QUANTA_OK && !quanta_value_number(&s->value, &n))
        {
            point.status = QUANTA_INVALID_DATA;
        }
        point.value = n;
        at = (p->history_head + p->history_count) % p->history_cap;
        p->history[at] = point;
        if (p->history_count < p->history_cap)
        {
            p->history_count++;
        }
        else
        {
            p->history_head = (p->history_head + 1) % p->history_cap;
        }
        while (p->history_count > 1 && point.time_ns >= p->history[p->history_head].time_ns &&
               point.time_ns - p->history[p->history_head].time_ns > m->history_ns)
        {
            p->history_head = (p->history_head + 1) % p->history_cap;
            p->history_count--;
        }
    }
    return QUANTA_OK;
}

static quanta_status model_end_frame(void *c)
{
    (void)c;
    return QUANTA_OK;
}

static quanta_status model_diag(void *c, quanta_status s, const char *text)
{
    quanta_model *m = (quanta_model *)c;
    (void)s;
    quanta_safe_text(m->diagnostic, sizeof(m->diagnostic), text);
    return QUANTA_OK;
}

static quanta_status model_progress(void *c, uint32_t a, uint32_t b, const char *text)
{
    quanta_model *m = (quanta_model *)c;
    quanta_snprintf(m->diagnostic, sizeof(m->diagnostic), "%u/%u %s", a, b, text);
    return QUANTA_OK;
}

static quanta_status model_end(void *c, quanta_status status)
{
    quanta_model *m = (quanta_model *)c;
    m->done = 1;
    m->end_status = status;
    return QUANTA_OK;
}

quanta_sink quanta_model_sink(quanta_model *m)
{
    quanta_sink s;
    quanta_memset(&s, 0, sizeof(s));
    s.struct_size = sizeof(s);
    s.api_version = 1;
    s.context = m;
    s.begin_session = model_begin;
    s.upsert_series = model_upsert;
    s.remove_series = model_remove;
    s.begin_frame = model_frame;
    s.emit_sample = model_sample;
    s.end_frame = model_end_frame;
    s.emit_diagnostic = model_diag;
    s.emit_progress = model_progress;
    s.end_session = model_end;
    return s;
}

const quanta_point *quanta_history_at(const quanta_item *p, size_t index)
{
    if (!p || !p->history_cap || index >= p->history_count)
    {
        return NULL;
    }
    return &p->history[(p->history_head + index) % p->history_cap];
}

quanta_status quanta_model_clone(quanta_model *dst, const quanta_model *src)
{
    size_t i;
    quanta_model out;
    if (!dst || !src)
    {
        return QUANTA_INVALID_DATA;
    }
    if (dst == src)
    {
        return QUANTA_OK;
    }
    out = *src;
    out.items = NULL;
    out.cap = src->count;
    if (src->count)
    {
        out.items = (quanta_item *)quanta_calloc(src->count, sizeof(quanta_item));
        if (!out.items)
        {
            return QUANTA_NO_MEMORY;
        }
    }
    for (i = 0; i < src->count; i++)
    {
        out.items[i] = src->items[i];
        out.items[i].history = NULL;
        if (src->items[i].history_cap)
        {
            size_t bytes = src->items[i].history_cap * sizeof(quanta_point);
            out.items[i].history = (quanta_point *)quanta_malloc(bytes);
            if (!out.items[i].history)
            {
                out.count = i + 1;
                quanta_model_destroy(&out);
                return QUANTA_NO_MEMORY;
            }
            quanta_memcpy(out.items[i].history, src->items[i].history, bytes);
        }
    }
    quanta_model_destroy(dst);
    *dst = out;
    return QUANTA_OK;
}
