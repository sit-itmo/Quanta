#include "collector_internal.h"
#include "filesystem.h"

static quanta_status fs_open(quanta_read_ctx *c)
{
    c->data = calloc(1, sizeof(quanta_filesystem_snapshot));
    return c->data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

static quanta_status fs_discover(quanta_read_ctx *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, &quanta_filesystem_provider, s);
}

static quanta_status fs_refresh(quanta_read_ctx *c)
{
    return quanta_hal_fs_refresh((quanta_filesystem_snapshot *)c->data,
                                 quanta_sensor_hal_context(c));
}

static void fs_close(quanta_read_ctx *c)
{
    if (!c->data)
    {
        return;
    }
    quanta_hal_fs_cleanup((quanta_filesystem_snapshot *)c->data);
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_filesystem_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v,
                                     uint64_t *w, uint32_t *q)
{
    quanta_filesystem_snapshot *d = (quanta_filesystem_snapshot *)c->data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (d->status != QUANTA_OK)
    {
        return d->status;
    }
    if (!strcmp(a, "total_bytes"))
    {
        v->as.u64 = d->total;
    }
    else if (!strcmp(a, "available_bytes"))
    {
        v->as.u64 = d->available;
    }
    else if (!strcmp(a, "used_bytes"))
    {
        v->as.u64 = d->total - d->free;
    }
    else if (!strcmp(a, "utilization"))
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

const quanta_provider_desc quanta_filesystem_provider = {
    7,
    "filesystem",
    "Root filesystem space: statvfs('/') on Linux, GetDiskFreeSpaceExW on the "
    "Windows install "
    "volume",
    fs_open,
    fs_discover,
    fs_refresh,
    fs_close};
