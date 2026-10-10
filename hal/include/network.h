/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_HAL_NETWORK_H
#define QUANTA_HAL_NETWORK_H
#include "hal.h"

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

quanta_status quanta_hal_network_refresh(quanta_network_snapshot *out, quanta_hal_context context);
void quanta_hal_network_cleanup(quanta_network_snapshot *data);
#endif
