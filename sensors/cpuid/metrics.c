#include "collector_internal.h"
#include "cpuid_snapshot.h"

quanta_status quanta_cpuid_init(quanta_metric_context *c)
{
    quanta_cpuid_snapshot *p = (quanta_cpuid_snapshot *)calloc(1, sizeof(*p));
    if (!p)
    {
        return QUANTA_NO_MEMORY;
    }
    c->private_data = p;
    return quanta_native_cpuid_probe(
        p, quanta_sensor_snapshot_context(c)); /* свойство ONCE: пробуем один раз при open */
}

quanta_status quanta_cpuid_enumerate(quanta_metric_context *c, quanta_discovery_sink *s)
{
    return quanta_discover_single(c, s);
}

void quanta_cpuid_destroy(quanta_metric_context *c)
{
    if (!c->private_data)
    {
        return;
    }
    quanta_native_cpuid_cleanup((quanta_cpuid_snapshot *)c->private_data);
    free(c->private_data);
    c->private_data = NULL;
}

quanta_status quanta_cpuid_read(quanta_metric_context *c, quanta_instance *in, quanta_value *v,
                                uint64_t *w, uint32_t *q)
{
    quanta_cpuid_snapshot *p = (quanta_cpuid_snapshot *)c->private_data;
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

