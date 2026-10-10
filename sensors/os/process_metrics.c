/**
 * @file sensors/os/process_metrics.c
 *
 * Current-Quanta-process group callbacks: shared-snapshot lifetime and extraction of metric
 * fields.
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
#include "process.h"

quanta_status quanta_process_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_process_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_process_enumerate(quanta_metric_context *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, s);
}

quanta_status quanta_process_collect(quanta_metric_context *c)
{
    return quanta_native_process_refresh((quanta_process_snapshot *)c->private_data,
                                      quanta_sensor_snapshot_context(c));
}

void quanta_process_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_process_cleanup((quanta_process_snapshot *)c->private_data);
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_process_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                  uint64_t *w, uint32_t *q)
{
    const quanta_process_snapshot *d = (const quanta_process_snapshot *)c->private_data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (!quanta_strcmp(a, "rss"))
    {
        v->as.u64 = d->rss_bytes;
        return d->memory_status;
    }
    if (!quanta_strcmp(a, "private"))
    {
        v->as.u64 = d->private_bytes;
        return d->memory_status;
    }
    if (!quanta_strcmp(a, "pid"))
    {
        v->as.u64 = d->pid;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "handles"))
    {
        v->as.u64 = d->handles;
        return d->handle_status;
    }
    if (!quanta_strcmp(a, "read_bytes"))
    {
        v->as.u64 = d->read_bytes;
        return d->io_status;
    }
    if (!quanta_strcmp(a, "write_bytes"))
    {
        v->as.u64 = d->write_bytes;
        return d->io_status;
    }
    return QUANTA_UNSUPPORTED;
}

