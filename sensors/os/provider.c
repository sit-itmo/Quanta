#include "collector_internal.h"
#include "os.h"

static quanta_status os_open(quanta_read_ctx *c)
{
    c->data = calloc(1, sizeof(quanta_os_snapshot));
    return c->data ? QUANTA_OK : QUANTA_NO_MEMORY;
}

static quanta_status os_discover(quanta_read_ctx *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, &quanta_os_provider, s);
}

static quanta_status os_refresh(quanta_read_ctx *c)
{
    return quanta_hal_os_refresh((quanta_os_snapshot *)c->data, quanta_sensor_hal_context(c));
}

static void os_close(quanta_read_ctx *c)
{
    if (!c->data)
    {
        return;
    }
    quanta_hal_os_cleanup((quanta_os_snapshot *)c->data);
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_os_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v, uint64_t *w,
                             uint32_t *q)
{
    const quanta_os_snapshot *d = (const quanta_os_snapshot *)c->data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (!strcmp(a, "hostname"))
    {
        if (d->hostname_status != QUANTA_OK)
        {
            return d->hostname_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->hostname) ? QUANTA_OK
                                                                            : QUANTA_LIMIT;
    }
    if ((!strcmp(a, "release") || !strcmp(a, "kernel")))
    {
        if (d->kernel_status != QUANTA_OK)
        {
            return d->kernel_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->kernel) ? QUANTA_OK
                                                                          : QUANTA_LIMIT;
    }
    if (!strcmp(a, "name"))
    {
        if (!d->name[0])
        {
            return QUANTA_UNSUPPORTED;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->name) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!strcmp(a, "version"))
    {
        if (!d->version[0])
        {
            return QUANTA_UNSUPPORTED;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->version) ? QUANTA_OK
                                                                           : QUANTA_LIMIT;
    }
    if (!strcmp(a, "uptime"))
    {
        if (d->uptime_status != QUANTA_OK)
        {
            return d->uptime_status;
        }
        v->as.f64 = d->uptime;
        return QUANTA_OK;
    }
    if ((!strcmp(a, "load1") || !strcmp(a, "load_1m")))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[0];
        return QUANTA_OK;
    }
    if ((!strcmp(a, "load5") || !strcmp(a, "load_5m")))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[1];
        return QUANTA_OK;
    }
    if (!strcmp(a, "load_15m"))
    {
        if (d->load_status != QUANTA_OK)
        {
            return d->load_status;
        }
        v->as.f64 = d->load[2];
        return QUANTA_OK;
    }
    if (!strcmp(a, "arch"))
    {
        if (!d->arch[0])
        {
            return d->kernel_status == QUANTA_OK ? QUANTA_NO_DATA : d->kernel_status;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), d->arch) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!strcmp(a, "online"))
    {
        v->as.u64 = d->online_cpus;
        return d->online_status;
    }
    return QUANTA_UNSUPPORTED;
}

const quanta_provider_desc quanta_os_provider = {
    0,
    "os",
    "System identity and load: gethostname/uname/proc on Linux, WinAPI on "
    "Windows",
    os_open,
    os_discover,
    os_refresh,
    os_close};
