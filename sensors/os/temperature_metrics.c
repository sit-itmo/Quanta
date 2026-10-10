/**
 * @file sensors/os/temperature_metrics.c
 *
 * Temperature/fan group callbacks: shared-snapshot lifetime and extraction of metric fields.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 */
#include "quanta/osal.h"
#include "collector_internal.h"
#include "hwmon.h"
#include "normalize.h"
#include "windows_cpu.h"

quanta_status quanta_hwmon_init(quanta_metric_context *c)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)quanta_calloc(1, sizeof(*d));
    if (!d)
    {
        return QUANTA_NO_MEMORY;
    }
    c->private_data = d;
    return quanta_native_hwmon_open(d, quanta_sensor_snapshot_context(c));
}

quanta_status quanta_hwmon_collect(quanta_metric_context *c)
{
    return quanta_native_hwmon_refresh((quanta_hwmon_snapshot *)c->private_data, quanta_sensor_snapshot_context(c));
}

void quanta_hwmon_destroy(quanta_metric_context *c)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->private_data;
    if (d)
    {
        quanta_native_hwmon_cleanup(d);
        quanta_free(d);
        c->private_data = NULL;
    }
}

static int kind_for_arg(const char *arg, quanta_hwmon_kind *kind)
{
    if (!quanta_strcmp(arg, "temp"))
    {
        *kind = HWMON_TEMP;
        return 1;
    }
    if (!quanta_strcmp(arg, "fan"))
    {
        *kind = HWMON_FAN;
        return 1;
    }
    return 0;
}

quanta_status quanta_hwmon_enumerate(quanta_metric_context *c, quanta_discovery_sink *sink)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->private_data;
    const quanta_metric_desc *const *r;
    size_t n, i, j;
    quanta_status s = quanta_native_hwmon_discover(d, quanta_sensor_snapshot_context(c));
    if (s != QUANTA_OK)
    {
        return s;
    }
    r = (n = c->metric_count, c->metrics);
    for (i = 0; i < d->count; i++)
    {
        const quanta_hwmon_channel *ch = &d->ch[i];
        for (j = 0; j < n; j++)
        {
            quanta_hwmon_kind want;
            quanta_instance in;
            quanta_status st;
            if (r[j]->init != quanta_hwmon_init || !quanta_metric_wanted(c, r[j]))
            {
                continue;
            }
            if (!kind_for_arg(r[j]->arg, &want) || want != ch->kind)
            {
                continue;
            }
            quanta_memset(&in, 0, sizeof(in));
            in.metric = r[j];
            in.min_poll_ns = ch->min_poll_ns;
            quanta_copy(in.entity_key, sizeof(in.entity_key), ch->entity_key);
            quanta_copy(in.resource, sizeof(in.resource), ch->resource);
            if (!quanta_label_set(in.labels, &in.label_count, "chip", ch->chip) ||
                !quanta_label_set(in.labels, &in.label_count, "channel", ch->channel) ||
                !quanta_label_set(in.labels, &in.label_count, "label", ch->label) ||
                !quanta_label_set(in.labels, &in.label_count, "device", ch->device))
            {
                return QUANTA_LIMIT;
            }
            st = sink->emit(sink->context, &in);
            if (st != QUANTA_OK)
            {
                return st;
            }
        }
    }
    return QUANTA_OK;
}

static const quanta_hwmon_channel *find_channel(const quanta_hwmon_snapshot *d,
                                                const char *entity_key)
{
    size_t i;
    for (i = 0; i < d->count; i++)
    {
        if (!quanta_strcmp(d->ch[i].entity_key, entity_key))
        {
            return &d->ch[i];
        }
    }
    return NULL;
}

quanta_status quanta_hwmon_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                uint64_t *w, uint32_t *q)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->private_data;
    const quanta_hwmon_channel *ch = find_channel(d, in->entity_key);
    double raw = 0;
    quanta_status fault = QUANTA_OK, s;
    (void)w;
    (void)q;
    if (!ch)
    {
        return QUANTA_GONE;
    }
    s = quanta_native_hwmon_read(d, ch, &raw, &fault);
    if (s != QUANTA_OK)
    {
        return s;
    }
    if (fault != QUANTA_OK)
    {
        return fault;
    }
    switch (ch->unit)
    {
        case HWMON_MILLI_C:
            v->as.f64 = raw / 1000.0;
            return QUANTA_OK;
        case HWMON_KELVIN:
            return quanta_kelvin_celsius(raw, &v->as.f64);
        case HWMON_RPM:
            if (raw < 0)
            {
                return QUANTA_INVALID_DATA;
            }
            v->as.u64 = (uint64_t)raw;
            return QUANTA_OK;
    }
    return QUANTA_UNSUPPORTED;
}

