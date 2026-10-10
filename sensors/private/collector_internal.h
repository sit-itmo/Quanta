/**
 * @file sensors/private/collector_internal.h
 *
 * Connect metric_context to snapshots, requested fields, paths and diagnostics. This header is
 * private, not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_COLLECTOR_INTERNAL_H
#define QUANTA_COLLECTOR_INTERNAL_H
#include "snapshot_context.h"
#include "memory.h"
#include "quanta/sensors.h"
#include "support.h"

static inline quanta_snapshot_context quanta_sensor_snapshot_context(quanta_metric_context *context)
{
    quanta_snapshot_context hal;
    hal.paths = context->paths;
    hal.error = context->error;
    hal.error_capacity = sizeof(context->error);
    return hal;
}


quanta_status quanta_os_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                             uint32_t *);
quanta_status quanta_memory_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                                 uint32_t *);
quanta_status quanta_cpu_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                              uint32_t *);
quanta_status quanta_dmi_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                              uint32_t *);
quanta_status quanta_network_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                                  uint32_t *);
quanta_status quanta_hwmon_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                                uint32_t *);
quanta_status quanta_cpuid_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                                uint32_t *);
quanta_status quanta_filesystem_read(quanta_metric_context *, quanta_instance *, quanta_value *,
                                     uint64_t *, uint32_t *);
quanta_status quanta_process_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *,
                                  uint32_t *);
quanta_status quanta_cpu_fraction(const uint64_t prev[8], const uint64_t now[8], double *);
quanta_status quanta_counter_derive(quanta_transform, const quanta_sample *, const quanta_sample *,
                                    quanta_value *, uint64_t *);

quanta_status quanta_os_init(quanta_metric_context *);
quanta_status quanta_os_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_os_collect(quanta_metric_context *);
void quanta_os_destroy(quanta_metric_context *);
quanta_status quanta_memory_init(quanta_metric_context *);
quanta_status quanta_memory_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_memory_collect(quanta_metric_context *);
void quanta_memory_destroy(quanta_metric_context *);
quanta_status quanta_cpu_init(quanta_metric_context *);
quanta_status quanta_cpu_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_cpu_collect(quanta_metric_context *);
void quanta_cpu_destroy(quanta_metric_context *);
quanta_status quanta_dmi_init(quanta_metric_context *);
quanta_status quanta_dmi_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_dmi_collect(quanta_metric_context *);
void quanta_dmi_destroy(quanta_metric_context *);
quanta_status quanta_network_init(quanta_metric_context *);
quanta_status quanta_network_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_network_collect(quanta_metric_context *);
void quanta_network_destroy(quanta_metric_context *);
quanta_status quanta_hwmon_init(quanta_metric_context *);
quanta_status quanta_hwmon_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_hwmon_collect(quanta_metric_context *);
void quanta_hwmon_destroy(quanta_metric_context *);
quanta_status quanta_cpuid_init(quanta_metric_context *);
quanta_status quanta_cpuid_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_cpuid_collect(quanta_metric_context *);
void quanta_cpuid_destroy(quanta_metric_context *);
quanta_status quanta_filesystem_init(quanta_metric_context *);
quanta_status quanta_filesystem_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_filesystem_collect(quanta_metric_context *);
void quanta_filesystem_destroy(quanta_metric_context *);
quanta_status quanta_process_init(quanta_metric_context *);
quanta_status quanta_process_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_process_collect(quanta_metric_context *);
void quanta_process_destroy(quanta_metric_context *);
quanta_status quanta_mcu_init(quanta_metric_context *);
quanta_status quanta_mcu_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_mcu_collect(quanta_metric_context *);
void quanta_mcu_destroy(quanta_metric_context *);
quanta_status quanta_mcu_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *, uint32_t *);
quanta_status quanta_smbios_mem_init(quanta_metric_context *);
quanta_status quanta_smbios_mem_enumerate(quanta_metric_context *, quanta_discovery_sink *);
quanta_status quanta_smbios_mem_collect(quanta_metric_context *);
void quanta_smbios_mem_destroy(quanta_metric_context *);
quanta_status quanta_smbios_mem_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *, uint32_t *);
quanta_status quanta_runtime_read(quanta_metric_context *, quanta_instance *, quanta_value *, uint64_t *, uint32_t *);
#endif
