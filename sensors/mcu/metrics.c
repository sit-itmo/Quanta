/* SPDX-License-Identifier: MIT */
#include "collector_internal.h"
#include "mcu.h"

quanta_status quanta_mcu_init(quanta_metric_context *context)
{
    context->private_data = calloc(1, sizeof(quanta_mcu_snapshot));
    return context->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}
quanta_status quanta_mcu_enumerate(quanta_metric_context *context, quanta_discovery_sink *sink)
{
    return quanta_discover_single(context, sink);
}
quanta_status quanta_mcu_collect(quanta_metric_context *context)
{
    return quanta_native_mcu_refresh(context->private_data);
}
void quanta_mcu_destroy(quanta_metric_context *context)
{
    free(context->private_data);
    context->private_data = NULL;
}
quanta_status quanta_mcu_read(quanta_metric_context *context, quanta_instance *instance,
                              quanta_value *value, uint64_t *window, uint32_t *quality)
{
    const quanta_mcu_snapshot *s = context->private_data;
    const char *field = instance->metric->arg;
    (void)window;
    if (!s) return QUANTA_NO_DATA;
    if (!strcmp(field, "heap_free")) value->as.u64 = s->heap_free;
    else if (!strcmp(field, "heap_min_free")) value->as.u64 = s->heap_min_free;
    else if (!strcmp(field, "freq_mhz"))
    {
        value->as.u64 = s->freq_mhz;
        *quality |= QUANTA_QUALITY_ESTIMATED; /* Configured, not a measured clock. */
    }
    else if (!strcmp(field, "cores")) value->as.u64 = s->cores;
    else if (!strcmp(field, "revision")) value->as.u64 = s->revision;
    else if (!strcmp(field, "chip_id"))
    {
        value->as.u64 = s->chip_id;
        return s->chip_id_status;
    }
    else if (!strcmp(field, "flash_embedded")) value->as.boolean = s->embedded_flash;
    else if (!strcmp(field, "vdd_mv"))
    {
        value->as.u64 = s->vdd_mv;
        return s->vdd_status;
    }
    else if (!strcmp(field, "sdk_version"))
    {
        if (s->sdk_status != QUANTA_OK) return s->sdk_status;
        return quanta_copy(value->as.string, sizeof(value->as.string), s->sdk_version) ? QUANTA_OK : QUANTA_LIMIT;
    }
    else return QUANTA_UNSUPPORTED;
    return QUANTA_OK;
}
