// providers/memory.c
#include "collector_internal.h"
#include "memory.h"
#include "windows_math.h"

static quanta_status mem_open(quanta_read_ctx *c)
{
    c->data = calloc(1, sizeof(quanta_memory_snapshot));
    return c->data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

static quanta_status mem_discover(quanta_read_ctx *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, &quanta_memory_provider, s);
}

static quanta_status mem_refresh(quanta_read_ctx *c)
{
    return quanta_hal_memory_refresh((quanta_memory_snapshot *)c->data,
                                     quanta_sensor_hal_context(c));
}

static void mem_close(quanta_read_ctx *c)
{
    if (!c->data)
    {
        return;
    }
    quanta_hal_memory_cleanup((quanta_memory_snapshot *)c->data);
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_memory_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v,
                                 uint64_t *w, uint32_t *q)
{
    const quanta_memory_snapshot *d = (const quanta_memory_snapshot *)c->data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;

    /* Windows-only fields use their own acquisition status. */
    if (!strcmp(a, "process_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->process_count;
        return QUANTA_OK;
    }
    if (!strcmp(a, "thread_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->thread_count;
        return QUANTA_OK;
    }
    if (!strcmp(a, "handle_count"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->handle_count;
        return QUANTA_OK;
    }
    if (!strcmp(a, "commit_used"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_used;
        return QUANTA_OK;
    }
    if (!strcmp(a, "commit_limit"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_limit;
        return QUANTA_OK;
    }
    if (!strcmp(a, "commit_peak"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->commit_peak;
        return QUANTA_OK;
    }
    if (!strcmp(a, "cache"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->cache;
        return QUANTA_OK;
    }
    if (!strcmp(a, "kernel_paged"))
    {
        if (d->performance_status != QUANTA_OK)
        {
            return d->performance_status;
        }
        v->as.u64 = d->kernel_paged;
        return QUANTA_OK;
    }
    if (!strcmp(a, "kernel_nonpaged"))
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
    if (!strcmp(a, "total"))
    {
        if (!(d->found & 1))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->total;
        return QUANTA_OK;
    }
    if (!strcmp(a, "available"))
    {
        if (!(d->found & 2))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->available;
        *q |= QUANTA_QUALITY_ESTIMATED;
        return QUANTA_OK;
    }
    if (!strcmp(a, "free"))
    {
        if (!(d->found & 4))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->free;
        return QUANTA_OK;
    }
    if (!strcmp(a, "used") || !strcmp(a, "utilization"))
    {
        uint64_t used;
        double fraction;
        quanta_status st;
        if ((d->found & 3) != 3)
        {
            return QUANTA_NO_DATA;
        }
        st = quanta_win_memory_used(d->total, d->available, &used, &fraction);
        if (st != QUANTA_OK)
        {
            return st;
        }
        if (!strcmp(a, "used"))
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
    if (!strcmp(a, "swap_total"))
    {
        if (!(d->found & 8))
        {
            return QUANTA_NO_DATA;
        }
        v->as.u64 = d->swap_total;
        return QUANTA_OK;
    }
    if (!strcmp(a, "swap_used"))
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

const quanta_provider_desc quanta_memory_provider = {
    1,
    "memory",
    "One /proc/meminfo snapshot; Windows: GlobalMemoryStatusEx + "
    "GetPerformanceInfo",
    mem_open,
    mem_discover,
    mem_refresh,
    mem_close};
