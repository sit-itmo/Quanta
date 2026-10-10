/* SPDX-License-Identifier: MIT -- bounded SMBIOS parser; no casts to packed structs */
#ifndef INV_SMBIOS_H
#define INV_SMBIOS_H
#include "inv/inv.h"
#define INV_SMBIOS_MAX_DIMMS 256
#define INV_SMBIOS_FIELD_CAP 256
/* Ordering matches SMBIOS type 0/1/2 index fields, not user-facing IDs. */
enum { INV_BIOS_VENDOR,INV_BIOS_VERSION,INV_BIOS_DATE,
       INV_BOARD_VENDOR,INV_BOARD_MODEL,INV_BOARD_SERIAL,
       INV_SYSTEM_VENDOR,INV_SYSTEM_MODEL,INV_SYSTEM_SERIAL,INV_DMI_FIELDS };
typedef struct inv_dmi_string {
    inv_status status;
    char text[INV_SMBIOS_FIELD_CAP];
} inv_dmi_string;
typedef struct inv_dmi_dimm {
    uint16_t handle;
    uint64_t size_bytes;
    uint32_t speed_mt_s;
    inv_status size_status,speed_status;
    inv_dmi_string locator,bank,vendor,serial,part;
} inv_dmi_dimm;
typedef struct inv_smbios {
    inv_dmi_string fields[INV_DMI_FIELDS];
    inv_dmi_string uuid;
    inv_dmi_dimm dimms[INV_SMBIOS_MAX_DIMMS];
    size_t dimm_count;
} inv_smbios;
/* Input is SMBIOS table bytes, NOT the Windows RawSMBIOSData 8-byte prefix.
   Output is cleared on any structural error. Unknown types are skipped.
   Missing and too-long strings have per-field statuses, never truncated values. */
inv_status inv_smbios_parse(const uint8_t *,size_t,unsigned major,unsigned minor,inv_smbios *);
/* Validates the Windows 8-byte header and declared table length. */
inv_status inv_smbios_parse_windows(const uint8_t *,size_t,inv_smbios *);
#endif
