/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_MCU_SNAPSHOT_H
#define QUANTA_MCU_SNAPSHOT_H
#include "quanta/types.h"
typedef struct quanta_mcu_snapshot
{
    uint64_t heap_free, heap_min_free, freq_mhz, vdd_mv;
    uint32_t cores, revision, chip_id;
    int embedded_flash;
    char sdk_version[QUANTA_TEXT_CAP];
    quanta_status chip_id_status, vdd_status, sdk_status;
} quanta_mcu_snapshot;
quanta_status quanta_native_mcu_refresh(quanta_mcu_snapshot *);
#endif
