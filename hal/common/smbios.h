/* SPDX-License-Identifier: MIT -- bounded SMBIOS parser; no casts to packed
 * structs */
#ifndef QUANTA_SMBIOS_H
#define QUANTA_SMBIOS_H
#include "quanta/types.h"
#define QUANTA_SMBIOS_MAX_DIMMS 256
#define QUANTA_SMBIOS_FIELD_CAP 256

/* Ordering matches SMBIOS type 0/1/2 index fields, not user-facing IDs. */
enum
{
    QUANTA_BIOS_VENDOR,
    QUANTA_BIOS_VERSION,
    QUANTA_BIOS_DATE,
    QUANTA_BOARD_VENDOR,
    QUANTA_BOARD_MODEL,
    QUANTA_BOARD_SERIAL,
    QUANTA_SYSTEM_VENDOR,
    QUANTA_SYSTEM_MODEL,
    QUANTA_SYSTEM_SERIAL,
    QUANTA_DMI_FIELDS
};

typedef struct quanta_dmi_string
{
    quanta_status status;
    char text[QUANTA_SMBIOS_FIELD_CAP];
} quanta_dmi_string;

typedef struct quanta_dmi_dimm
{
    uint16_t handle;
    uint64_t size_bytes;
    uint32_t speed_mt_s;
    quanta_status size_status, speed_status;
    quanta_dmi_string locator, bank, vendor, serial, part;
} quanta_dmi_dimm;

typedef struct quanta_smbios
{
    quanta_dmi_string fields[QUANTA_DMI_FIELDS];
    quanta_dmi_string uuid;
    quanta_dmi_dimm dimms[QUANTA_SMBIOS_MAX_DIMMS];
    size_t dimm_count;
} quanta_smbios;

/* Input is SMBIOS table bytes, NOT the Windows RawSMBIOSData 8-byte prefix.
   Output is cleared on any structural error. Unknown types are skipped.
   Missing and too-long strings have per-field statuses, never truncated values.
 */
quanta_status quanta_smbios_parse(const uint8_t *, size_t, unsigned major, unsigned minor,
                                  quanta_smbios *);
/* Validates the Windows 8-byte header and declared table length. */
quanta_status quanta_smbios_parse_windows(const uint8_t *, size_t, quanta_smbios *);
#endif
