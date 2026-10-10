#include "collector_internal.h"
#include "process.h"

static quanta_status proc_open(quanta_read_ctx *c)
{
    c->data = calloc(1, sizeof(quanta_process_snapshot));
    return c->data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

static quanta_status proc_discover(quanta_read_ctx *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, &quanta_process_provider, s);
}

static quanta_status proc_refresh(quanta_read_ctx *c)
{
    return quanta_hal_process_refresh((quanta_process_snapshot *)c->data,
                                      quanta_sensor_hal_context(c));
}

static void proc_close(quanta_read_ctx *c)
{
    if (!c->data)
    {
        return;
    }
    quanta_hal_process_cleanup((quanta_process_snapshot *)c->data);
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_process_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v,
                                  uint64_t *w, uint32_t *q)
{
    const quanta_process_snapshot *d = (const quanta_process_snapshot *)c->data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (!strcmp(a, "rss"))
    {
        v->as.u64 = d->rss_bytes;
        return d->memory_status;
    }
    if (!strcmp(a, "private"))
    {
        v->as.u64 = d->private_bytes;
        return d->memory_status;
    }
    if (!strcmp(a, "pid"))
    {
        v->as.u64 = d->pid;
        return QUANTA_OK;
    }
    if (!strcmp(a, "handles"))
    {
        v->as.u64 = d->handles;
        return d->handle_status;
    }
    if (!strcmp(a, "read_bytes"))
    {
        v->as.u64 = d->read_bytes;
        return d->io_status;
    }
    if (!strcmp(a, "write_bytes"))
    {
        v->as.u64 = d->write_bytes;
        return d->io_status;
    }
    return QUANTA_UNSUPPORTED;
}

const quanta_provider_desc quanta_process_provider = {
    8,
    "process",
    "Current process: Linux statm; Windows process memory, handles and I/O "
    "counters",
    proc_open,
    proc_discover,
    proc_refresh,
    proc_close};
