/* SPDX-License-Identifier: MIT */
#include "collector_internal.h"
#include "cpu.h"
#include <ctype.h>

quanta_status quanta_cpu_fraction(const uint64_t p[8], const uint64_t n[8], double *out)
{
    uint64_t total = 0, idle = 0;
    size_t i;
    for (i = 0; i < 8; i++)
    {
        uint64_t diff;
        if (n[i] < p[i])
        {
            return QUANTA_WARMUP;
        }
        diff = n[i] - p[i];
        if (UINT64_MAX - total < diff)
        {
            return QUANTA_INVALID_DATA;
        }
        total += diff;
        if (i == CPU_IDLE || i == CPU_IOWAIT)
        {
            if (UINT64_MAX - idle < diff)
            {
                return QUANTA_INVALID_DATA;
            }
            idle += diff;
        }
    }
    if (!total)
    {
        return QUANTA_WARMUP;
    }
    *out = (double)(total - idle) / (double)total;
    return QUANTA_OK;
}

quanta_status quanta_cpu_init(quanta_metric_context *c)
{
    c->private_data = calloc(1, sizeof(quanta_cpu_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

/* Acquire one native snapshot shared by the selected CPU fields. */
quanta_status quanta_cpu_collect(quanta_metric_context *c)
{
    quanta_cpu_snapshot *d = (quanta_cpu_snapshot *)c->private_data;
    return quanta_native_cpu_refresh(d, quanta_sensor_snapshot_context(c));
}

quanta_status quanta_cpu_enumerate(quanta_metric_context *c, quanta_discovery_sink *sink)
{
    quanta_cpu_snapshot *d = (quanta_cpu_snapshot *)c->private_data;
    size_t i, j, n;
    quanta_status s = quanta_cpu_collect(c);
    const quanta_metric_desc *const *r = (n = c->metric_count, c->metrics);
    if (s != QUANTA_OK)
    {
        return s;
    }
    for (i = 0; i < d->count; i++)
    {
        for (j = 0; j < n; j++)
        {
            if (r[j]->init == quanta_cpu_init && quanta_metric_wanted(c, r[j]))
            {
                int core = !strncmp(r[j]->key, "cpu.core.", 9);
                quanta_instance in;
                if (core == !strcmp(d->rows[i].name, "cpu"))
                {
                    continue;
                }
                memset(&in, 0, sizeof(in));
                in.metric = r[j];
                quanta_copy(in.resource, sizeof(in.resource), d->rows[i].name);
                quanta_copy(in.entity_key, sizeof(in.entity_key), d->rows[i].name);
                if (core)
                {
                    quanta_label_set(in.labels, &in.label_count, "cpu", d->rows[i].name + 3);
#if QUANTA_OS_WINDOWS
                    char group[24], number[24];
                    snprintf(group, sizeof(group), "%u", d->rows[i].group);
                    snprintf(number, sizeof(number), "%u", d->rows[i].number);
                    quanta_label_set(in.labels, &in.label_count, "group", group);
                    quanta_label_set(in.labels, &in.label_count, "number", number);
#endif
                }
                s = sink->emit(sink->context, &in);
                if (s != QUANTA_OK)
                {
                    return s;
                }
            }
        }
    }
    return QUANTA_OK;
}

void quanta_cpu_destroy(quanta_metric_context *c)
{
    quanta_cpu_snapshot *d = (quanta_cpu_snapshot *)c->private_data;
    if (d)
    {
        quanta_native_cpu_cleanup(d);
        free(d);
        c->private_data = NULL;
    }
}

quanta_status quanta_cpu_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v, uint64_t *w,
                              uint32_t *q)
{
    quanta_cpu_snapshot *d = (quanta_cpu_snapshot *)c->private_data;
    quanta_cpu_row *r = NULL;
    size_t i, k;
    const char *a = in->metric->arg;
    (void)q;
    for (i = 0; i < d->count; i++)
    {
        if (!strcmp(in->resource, d->rows[i].name))
        {
            r = &d->rows[i];
            break;
        }
    }
    if (!r)
    {
        return QUANTA_GONE;
    }
    if (!strcmp(a, "tick_frequency"))
    {
        v->as.u64 = d->tick_frequency_hz;
        return d->tick_frequency_hz ? QUANTA_OK : QUANTA_NO_DATA;
    }
    if (!strcmp(a, "utilization"))
    {
        if (r->direct_utilization)
        {
            v->as.f64 = r->fraction;
            *w = r->window_ns;
            return r->utilization_status;
        }
        if (r->accounting_status != QUANTA_OK)
        {
            return r->accounting_status;
        }
        quanta_status s = in->reader_state[8]
                              ? quanta_cpu_fraction(in->reader_state, r->t, &v->as.f64)
                              : QUANTA_WARMUP;
        if (in->reader_state[8] && d->time_ns > in->reader_state[9])
        {
            *w = d->time_ns - in->reader_state[9];
        }
        memcpy(in->reader_state, r->t, 8 * sizeof(uint64_t));
        in->reader_state[8] = 1;
        in->reader_state[9] = d->time_ns;
        return s;
    }
    if (r->accounting_status != QUANTA_OK)
    {
        return r->accounting_status;
    }
    if (!strcmp(a, "idle"))
    {
        v->as.u64 = r->t[3];
        return QUANTA_OK;
    }
    if (!strcmp(a, "total"))
    {
        uint64_t sum = 0;
        for (k = 0; k < 8; k++)
        {
            if (UINT64_MAX - sum < r->t[k])
            {
                return QUANTA_INVALID_DATA;
            }
            sum += r->t[k];
        }
        v->as.u64 = sum;
        return QUANTA_OK;
    }
    return QUANTA_UNSUPPORTED;
}

