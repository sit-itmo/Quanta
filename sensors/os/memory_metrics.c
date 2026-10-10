/**
 * @file sensors/os/memory_metrics.c
 *
 * Memory group callbacks: shared-snapshot lifetime and extraction of metric fields.
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
#include "memory.h"
#include "normalize.h"
#include "windows_cpu.h"

quanta_status quanta_memory_init(quanta_metric_context *c)
{
    c->private_data = quanta_calloc(1, sizeof(quanta_memory_snapshot));
    return c->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

quanta_status quanta_memory_enumerate(quanta_metric_context *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, s);
}

quanta_status quanta_memory_collect(quanta_metric_context *c)
{
    return quanta_native_memory_refresh((quanta_memory_snapshot *)c->private_data,
                                     quanta_sensor_snapshot_context(c));
}

void quanta_memory_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_memory_cleanup((quanta_memory_snapshot *)c->private_data);
    quanta_free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_memory_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                 uint64_t *w, uint32_t *q)
{
    const quanta_memory_snapshot *d = (const quanta_memory_snapshot *)c->private_data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;

    /* Windows-only fields use their own acquisition status. */
    if (!quanta_strcmp(a, "process_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->process_count;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "thread_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->thread_count;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "handle_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->handle_count;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "commit_used"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_used;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "commit_limit"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_limit;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "commit_peak"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_peak;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "cache"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->cache;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "kernel_paged"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->kernel_paged;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "kernel_nonpaged"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->kernel_nonpaged;
        return QUANTA_OK;
    }

    /* Physical memory and swap share one source status. */
    if (d->memory_status != QUANTA_OK)
    {
        return d->memory_status;
    }
    if (!quanta_strcmp(a, "total"))
    {
        if (!(d->found & 1))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->total;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "available"))
    {
        if (!(d->found & 2))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->available;
        *q |= QUANTA_QUALITY_ESTIMATED;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "free"))
    {
        if (!(d->found & 4))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->free;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "used") || !quanta_strcmp(a, "utilization"))
    {
        uint64_t used;
        double fraction;
        quanta_status st;
        if ((d->found & 3) != 3)
        {
            return QUANTA_NO_DATA;
        }
        st = quanta_memory_used(d->total, d->available, &used, &fraction);
        if (st != QUANTA_OK)
        {
            return st;
        }
        if (!quanta_strcmp(a, "used"))
        {
            v->as.u64 = used;
        }
        else
        {
            v->as.f64 = fraction;
        }
        *q |= QUANTA_QUALITY_ESTIMATED;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "swap_total"))
    {
        if (!(d->found & 8))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->swap_total;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(a, "swap_used"))
    {
        if ((d->found & 24) != 24)
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->swap_total - d->swap_free;
        return QUANTA_OK;
    }
    return QUANTA_UNSUPPORTED;
}

