/**
 * @file sensors/private/hwmon.h
 *
 * Discovered temperature/fan channels with units and identity. This header is private, not
 * public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
#ifndef QUANTA_SENSOR_HWMON_H
#define QUANTA_SENSOR_HWMON_H

#include "snapshot_context.h"

typedef enum quanta_hwmon_kind
{
    HWMON_TEMP,
    HWMON_FAN
} quanta_hwmon_kind;

typedef enum quanta_hwmon_unit
{
    HWMON_MILLI_C,
    HWMON_KELVIN,
    HWMON_RPM
} quanta_hwmon_unit;

typedef struct quanta_hwmon_channel
{
    quanta_hwmon_kind kind;
    quanta_hwmon_unit unit;
    char chip[QUANTA_TEXT_CAP];
    char channel[48];
    char label[QUANTA_TEXT_CAP];
    char device[QUANTA_PATH_CAP];
    char entity_key[QUANTA_ID_CAP];
    char resource[QUANTA_PATH_CAP];
    uint64_t min_poll_ns;
} quanta_hwmon_channel;

typedef struct quanta_hwmon_snapshot
{
    quanta_hwmon_channel *ch;
    size_t count, cap;
    quanta_status status;
    void *private_data;
} quanta_hwmon_snapshot;

quanta_status quanta_native_hwmon_open(quanta_hwmon_snapshot *out, quanta_snapshot_context context);
quanta_status quanta_native_hwmon_discover(quanta_hwmon_snapshot *out, quanta_snapshot_context context);
quanta_status quanta_native_hwmon_refresh(quanta_hwmon_snapshot *out, quanta_snapshot_context context);
quanta_status quanta_native_hwmon_read(const quanta_hwmon_snapshot *d, const quanta_hwmon_channel *ch,
                                    double *raw, quanta_status *fault);
void quanta_native_hwmon_cleanup(quanta_hwmon_snapshot *d);

#endif
