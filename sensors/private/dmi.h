#ifndef QUANTA_SENSOR_DMI_H
#define QUANTA_SENSOR_DMI_H

#include "snapshot_context.h"
#include "smbios.h"

typedef struct quanta_board_snapshot
{
    quanta_smbios table;
    quanta_status status;
} quanta_board_snapshot;

quanta_status quanta_native_dmi_refresh(quanta_board_snapshot *out, quanta_snapshot_context context);
void quanta_native_dmi_cleanup(quanta_board_snapshot *d);

#endif
