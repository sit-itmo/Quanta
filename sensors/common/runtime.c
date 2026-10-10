/* SPDX-License-Identifier: MIT -- common metadata, no OS-specific acquisition */
#include "quanta/hal.h"
#include "quanta/osal.h"
#include "quanta/text.h"
#include "quanta/metric.h"

quanta_status quanta_runtime_read(quanta_metric_context *context, quanta_instance *instance,
                                  quanta_value *value, uint64_t *window, uint32_t *quality)
{
    quanta_hal_info info;
    quanta_status status;
    const char *field = instance->metric->arg;
    (void)window;
    (void)quality;
    if (!quanta_strcmp(field, "version"))
        return quanta_copy(value->as.string, sizeof(value->as.string), QUANTA_VERSION) ?
               QUANTA_OK : QUANTA_LIMIT;
    if (!context->hal) return QUANTA_UNSUPPORTED;
    status = quanta_hal_get_info(context->hal, &info);
    if (status != QUANTA_OK) return status;
    if (!quanta_strcmp(field, "hal_name"))
        return quanta_copy(value->as.string, sizeof(value->as.string), info.name) ? QUANTA_OK : QUANTA_LIMIT;
    if (!quanta_strcmp(field, "capabilities"))
    {
        value->as.u64 = info.capabilities;
        return QUANTA_OK;
    }
    if (!quanta_strcmp(field, "logical_cpus"))
    {
        if (!(info.known & QUANTA_HAL_CPU_COUNT_KNOWN)) return QUANTA_UNSUPPORTED;
        value->as.u64 = info.logical_cpus;
        return QUANTA_OK;
    }
    return QUANTA_UNSUPPORTED;
}
