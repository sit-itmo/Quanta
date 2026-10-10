// providers/hwmon.c
#include "collector_internal.h"
#include "hwmon.h"
#include "windows_math.h"

static quanta_status hw_open(quanta_read_ctx *c)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)calloc(1, sizeof(*d));
    if (!d)
    {
        return QUANTA_NO_MEMORY;
    }
    c->data = d;
    return quanta_hal_hwmon_open(d, quanta_sensor_hal_context(c));
}

static quanta_status hw_refresh(quanta_read_ctx *c)
{
    return quanta_hal_hwmon_refresh((quanta_hwmon_snapshot *)c->data, quanta_sensor_hal_context(c));
}

static void hw_close(quanta_read_ctx *c)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->data;
    if (d)
    {
        quanta_hal_hwmon_cleanup(d);
        free(d);
        c->data = NULL;
    }
}

static int kind_for_arg(const char *arg, quanta_hwmon_kind *kind)
{
    if (!strcmp(arg, "temp"))
    {
        *kind = HWMON_TEMP;
        return 1;
    }
    if (!strcmp(arg, "fan"))
    {
        *kind = HWMON_FAN;
        return 1;
    }
    return 0;
}

static quanta_status hw_discover(quanta_read_ctx *c, quanta_discovery_sink *sink)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->data;
    const quanta_metric_desc *const *r;
    size_t n, i, j;
    quanta_status s = quanta_hal_hwmon_discover(d, quanta_sensor_hal_context(c));
    if (s != QUANTA_OK)
    {
        return s;
    }
    r = (n = c->catalog->count, c->catalog->metrics);
    for (i = 0; i < d->count; i++)
    {
        const quanta_hwmon_channel *ch = &d->ch[i];
        for (j = 0; j < n; j++)
        {
            quanta_hwmon_kind want;
            quanta_instance in;
            quanta_status st;
            if (r[j]->provider != &quanta_hwmon_provider || !quanta_metric_wanted(c, r[j]))
            {
                continue;
            }
            if (!kind_for_arg(r[j]->arg, &want) || want != ch->kind)
            {
                continue;
            }
            memset(&in, 0, sizeof(in));
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
        if (!strcmp(d->ch[i].entity_key, entity_key))
        {
            return &d->ch[i];
        }
    }
    return NULL;
}

quanta_status quanta_hwmon_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v,
                                uint64_t *w, uint32_t *q)
{
    quanta_hwmon_snapshot *d = (quanta_hwmon_snapshot *)c->data;
    const quanta_hwmon_channel *ch = find_channel(d, in->entity_key);
    double raw = 0;
    quanta_status fault = QUANTA_OK, s;
    (void)w;
    (void)q;
    if (!ch)
    {
        return QUANTA_GONE;
    }
    s = quanta_hal_hwmon_read(d, ch, &raw, &fault);
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
            return quanta_win_kelvin_celsius(raw, &v->as.f64);
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

const quanta_provider_desc quanta_hwmon_provider = {
    5,
    "sensors",
    "Read-only temperature/fan channels: sysfs hwmon on Linux, PDH thermal "
    "zones on Windows; "
    "fault-aware",
    hw_open,
    hw_discover,
    hw_refresh,
    hw_close};
