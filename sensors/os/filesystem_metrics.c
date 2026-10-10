/**
 * @file sensors/os/filesystem_metrics.c
 *
 * Filesystem group callbacks: shared-snapshot lifetime and extraction of metric fields.
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
#include "filesystem.h"

quanta_status quanta_filesystem_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_filesystem_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_filesystem_enumerate(quanta_metric_context *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, s);
}

quanta_status quanta_filesystem_collect(quanta_metric_context *c)
{
    return quanta_native_fs_refresh((quanta_filesystem_snapshot *)c->private_data,
                                 quanta_sensor_snapshot_context(c));
}

void quanta_filesystem_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_fs_cleanup((quanta_filesystem_snapshot *)c->private_data);
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_filesystem_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                     uint64_t *w, uint32_t *q)
{
    quanta_filesystem_snapshot *d = (quanta_filesystem_snapshot *)c->private_data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (d->status != QUANTA_OK)
    {
        return d->status;
    }
    if (!quanta_strcmp(a, "total_bytes"))
    {
        v->as.u64 = d->total;
    }
    else if (!quanta_strcmp(a, "available_bytes"))
    {
        v->as.u64 = d->available;
    }
    else if (!quanta_strcmp(a, "used_bytes"))
    {
        v->as.u64 = d->total - d->free;
    }
    else if (!quanta_strcmp(a, "utilization"))
    {
        if (!d->total)
        {
            return QUANTA_NO_DATA;
        }
        v->as.f64 = (double)(d->total - d->free) / (double)d->total;
    }
    else
    {
        return QUANTA_UNSUPPORTED;
    }
    return QUANTA_OK;
}

