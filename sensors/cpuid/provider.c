// providers/cpuid.c
#include "collector_internal.h"
#include "quanta_hal_cpuid.h"

static quanta_status cpuid_open(quanta_read_ctx *c)
{
    quanta_cpuid_snapshot *p = (quanta_cpuid_snapshot *)calloc(1, sizeof(*p));
    if (!p)
    {
        return QUANTA_NO_MEMORY;
    }
    c->data = p;
    return quanta_hal_cpuid_probe(
        p, quanta_sensor_hal_context(c)); /* свойство ONCE: пробуем один раз при open */
}

static quanta_status cpuid_discover(quanta_read_ctx *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, &quanta_cpuid_provider, s);
}

static void cpuid_close(quanta_read_ctx *c)
{
    if (!c->data)
    {
        return;
    }
    quanta_hal_cpuid_cleanup((quanta_cpuid_snapshot *)c->data);
    free(c->data);
    c->data = NULL;
}

quanta_status quanta_cpuid_read(quanta_read_ctx *c, quanta_instance *in, quanta_value *v,
                                uint64_t *w, uint32_t *q)
{
    quanta_cpuid_snapshot *p = (quanta_cpuid_snapshot *)c->data;
    const char *a = in->metric->arg;
    (void)w;
    (void)q;
    if (p->status != QUANTA_OK)
    {
        return p->status;
    }
    if (!strcmp(a, "vendor"))
    {
        return quanta_copy(v->as.string, sizeof(v->as.string), p->vendor) ? QUANTA_OK
                                                                          : QUANTA_LIMIT;
    }
    if (!strcmp(a, "brand"))
    {
        if (!p->brand[0])
        {
            return QUANTA_NO_DATA;
        }
        return quanta_copy(v->as.string, sizeof(v->as.string), p->brand) ? QUANTA_OK : QUANTA_LIMIT;
    }
    if (!strcmp(a, "cpuid_reference"))
    {
        v->as.u64 = p->ref;
        return QUANTA_OK;
    }
    if (!strcmp(a, "vmx"))
    {
        v->as.boolean = !!(p->ecx1 & (1u << 5));
    }
    else if (!strcmp(a, "svm"))
    {
        v->as.boolean = !!(p->ecxext & (1u << 2));
    }
    else if (!strcmp(a, "sse2"))
    {
        v->as.boolean = !!(p->edx1 & (1u << 26));
    }
    else if (!strcmp(a, "avx"))
    {
        v->as.boolean = !!(p->ecx1 & (1u << 28));
    }
    else if (!strcmp(a, "avx2"))
    {
        v->as.boolean = !!(p->ebx7 & (1u << 5));
    }
    else if (!strcmp(a, "hypervisor"))
    {
        v->as.boolean = !!(p->ecx1 & (1u << 31));
    }
    else
    {
        return QUANTA_UNSUPPORTED;
    }
    return QUANTA_OK;
}

const quanta_provider_desc quanta_cpuid_provider = {
    6,          "x86.cpuid",    "Direct CPUID on a pinned, affinity-allowed reference CPU",
    cpuid_open, cpuid_discover, NULL,
    cpuid_close};
