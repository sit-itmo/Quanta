/**
 * @file sensors/private/network.h
 *
 * Interface rows: device identity and separate RX/TX counters. This header is private, not
 * public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SENSOR_NETWORK_H
#define QUANTA_SENSOR_NETWORK_H
#include "snapshot_context.h"

typedef struct quanta_network_row
{
    char name[QUANTA_TEXT_CAP], entity_key[QUANTA_ID_CAP];
    char address[128], state[32], guid[48];
    uint64_t ifindex, rx_bytes, rx_packets, rx_errors, rx_dropped;
    uint64_t tx_bytes, tx_packets, tx_errors, tx_dropped, rx_link_bps, tx_link_bps;
    quanta_status counters_status, address_status, state_status, link_status;
} quanta_network_row;

typedef struct quanta_network_snapshot
{
    quanta_network_row *rows;
    size_t count, cap;
    quanta_status status;
} quanta_network_snapshot;

quanta_status quanta_native_network_refresh(quanta_network_snapshot *out, quanta_snapshot_context context);
void quanta_native_network_cleanup(quanta_network_snapshot *data);
#endif
