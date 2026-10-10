/**
 * @file sensors/os/os_metrics.c
 *
 * OS-information group callbacks: shared-snapshot lifetime and extraction of metric fields.
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
#include "os.h"

quanta_status quanta_os_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_os_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_os_enumerate(quanta_metric_context *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, s);
}

quanta_status quanta_os_collect(quanta_metric_context *c)
{
    return quanta_native_os_refresh((quanta_os_snapshot *)c->private_data, quanta_sensor_snapshot_context(c));
}

void quanta_os_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_os_cleanup((quanta_os_snapshot *)c->private_data);
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_os_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v, uint64_t *w,
                             uint32_t *q)
{
    const quanta_os_snapshot *d = (const quanta_os_snapshot *)c->private_data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (!quanta_strcmp(a, "hostname"))
    {
        if (d->hostname_status != QUANTA_OK)
        {
            return d->hostname_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->hostname) ? QUANTA_OK
                                                                            : QUANTA_LIMIT;
    }
    if ((!quanta_strcmp(a, "release") || !quanta_strcmp(a, "kernel")))
    {
        if (d->kernel_status != QUANTA_OK)
        {
            return d->kernel_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->kernel) ? QUANTA_OK
                                                                          : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "name"))
    {
        if (!d->name[0])
        {
            return QUANTA_UNSUPPORTED;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->name) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "version"))
    {
        if (!d->version[0])
        {
            return QUANTA_UNSUPPORTED;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->version) ? QUANTA_OK
                                                                           : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "uptime"))
    {
        if (d->uptime_status != QUANTA_OK)
        {
            return d->uptime_status;
        }
        v->as.f64 = d->uptime;
        return QUANTA_OK;
    }
    if ((!quanta_strcmp(a, "load1") || !quanta_strcmp(a, "load_1m")))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[0];
        return QUANTA_OK;
    }
    if ((!quanta_strcmp(a, "load5") || !quanta_strcmp(a, "load_5m")))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[1];
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "load_15m"))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[2];
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "arch"))
    {
        if (!d->arch[0])
        {
            return d->kernel_status == QUANTA_OK ? QUANTA_NO_DATA : d->kernel_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->arch) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!quanta_strcmp(a, "online"))
    {
        v->as.u64 = d->online_cpus;
        return d->online_status;
    }
    return QUANTA_UNSUPPORTED;
}

