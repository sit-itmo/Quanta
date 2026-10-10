// hal/include/dmi.h
#ifndef QUANTA_HAL_DMI_H
#define QUANTA_HAL_DMI_H

#include "hal.h"
#include "smbios.h"

typedef struct quanta_board_snapshot
{
    quanta_smbios table;
    quanta_status status;
} quanta_board_snapshot;

quanta_status quanta_hal_dmi_refresh(quanta_board_snapshot *out, quanta_hal_context context);
void quanta_hal_dmi_cleanup(quanta_board_snapshot *d);

#endif
