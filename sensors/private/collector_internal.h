/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_COLLECTOR_INTERNAL_H
#define QUANTA_COLLECTOR_INTERNAL_H
#include "hal.h"
#include "memory.h"
#include "quanta/sensors.h"
#include "support.h"

static inline quanta_hal_context quanta_sensor_hal_context(quanta_read_ctx *context)
{
    quanta_hal_context hal;
    hal.paths = context->paths;
    hal.error = context->error;
    hal.error_capacity = sizeof(context->error);
    return hal;
}

extern const quanta_provider_desc quanta_os_provider, quanta_memory_provider, quanta_cpu_provider,
    quanta_dmi_provider, quanta_network_provider, quanta_hwmon_provider, quanta_cpuid_provider,
    quanta_filesystem_provider, quanta_process_provider;
quanta_status quanta_os_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                             uint32_t *);
quanta_status quanta_memory_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                                 uint32_t *);
quanta_status quanta_cpu_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                              uint32_t *);
quanta_status quanta_dmi_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                              uint32_t *);
quanta_status quanta_network_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                                  uint32_t *);
quanta_status quanta_hwmon_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                                uint32_t *);
quanta_status quanta_cpuid_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                                uint32_t *);
quanta_status quanta_filesystem_read(quanta_read_ctx *, quanta_instance *, quanta_value *,
                                     uint64_t *, uint32_t *);
quanta_status quanta_process_read(quanta_read_ctx *, quanta_instance *, quanta_value *, uint64_t *,
                                  uint32_t *);
quanta_status quanta_cpu_fraction(const uint64_t prev[8], const uint64_t now[8], double *);
quanta_status quanta_counter_derive(quanta_transform, const quanta_sample *, const quanta_sample *,
                                    quanta_value *, uint64_t *);

#endif
