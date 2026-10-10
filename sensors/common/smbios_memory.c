/**
 * @file sensors/common/smbios_memory.c
 *
 * Separate legacy SMBIOS metrics: locate the entry point and read physical tables through HAL
 * read_mem.
 *
 * HAL: hardware primitives and information about their availability. Metrics and SMBIOS/ACPI
 * parsing are not HAL operations.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT
 * A distinct metric method: legacy firmware-window scan + HAL physical reads.
 * Not a HAL service. Never falls back to sysfs/WinAPI metric values.
 * UEFI-only tables outside this window require another discovery method. */
#include "quanta/osal.h"
#include "quanta/metric.h"
#include "quanta/text.h"
#include "dmi.h"
#include "quanta/hal.h"

#define ROM_BASE UINT64_C(0xF0000)
#define ROM_LENGTH 65536u
#define TABLE_LIMIT (1024u * 1024u)

static uint64_t little_endian(const uint8_t *bytes, unsigned count)
{
    uint64_t value = 0;
    unsigned i;
    for (i = 0; i < count; ++i) value |= (uint64_t)bytes[i] << (8u * i);
    return value;
}
static int checksum(const uint8_t *bytes, size_t count)
{
    uint8_t sum = 0;
    size_t i;
    for (i = 0; i < count; ++i) sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}
static quanta_status read_blocks(quanta_hal *hal, uint64_t address, uint8_t *out, size_t size)
{
    quanta_hal_info info;
    quanta_status status = quanta_hal_get_info(hal, &info);
    size_t done = 0;
    if (status != QUANTA_OK) return status;
    if (size && size - 1 > UINT64_MAX - address) return QUANTA_INVALID_DATA;
    while (done < size)
    {
        size_t chunk = size - done;
        if (chunk > info.maximum_transfer) chunk = info.maximum_transfer;
        status = quanta_hal_read_mem(hal, address + done, out + done, chunk);
        if (status != QUANTA_OK) return status;
        done += chunk;
    }
    return QUANTA_OK;
}
static quanta_status scan_table(quanta_hal *hal, quanta_smbios *out)
{
    uint8_t *rom = NULL, *table = NULL;
    uint64_t address = 0;
    size_t size = 0, offset;
    unsigned major = 0, minor = 0, pass;
    int malformed = 0;
    quanta_status status;
    quanta_hal_info info;
    if (!hal) return QUANTA_UNSUPPORTED;
    status = quanta_hal_get_info(hal, &info);
    if (status != QUANTA_OK) return status;
    if (!(info.capabilities & QUANTA_HAL_READ_MEM)) return QUANTA_UNSUPPORTED;
    rom = quanta_malloc(ROM_LENGTH);
    if (!rom) return QUANTA_NO_MEMORY;
    status = read_blocks(hal, ROM_BASE, rom, ROM_LENGTH);
    if (status != QUANTA_OK) goto finish;
    /* Prefer the 64-bit entry when both versions are present in the same ROM.
     * Strict lengths/checksums are validated before trusting physical pointers. */
    for (pass = 0; pass < 2 && !size; ++pass)
    {
        for (offset = 0; offset + 32 <= ROM_LENGTH; offset += 16)
        {
            const uint8_t *entry = rom + offset;
            if (pass == 0 && !quanta_memcmp(entry, "_SM3_", 5))
            {
                if (entry[6] < 24 || entry[6] > 32 || !checksum(entry, entry[6]))
                { malformed = 1; continue; }
                address = little_endian(entry + 16, 8);
                size = (size_t)little_endian(entry + 12, 4);
                major = entry[7]; minor = entry[8];
            }
            else if (pass == 1 && !quanta_memcmp(entry, "_SM_", 4))
            {
                if (entry[5] < 31 || entry[5] > 32 || !checksum(entry, entry[5]) ||
                    quanta_memcmp(entry + 16, "_DMI_", 5) || !checksum(entry + 16, 15))
                { malformed = 1; continue; }
                address = little_endian(entry + 24, 4);
                size = (size_t)little_endian(entry + 22, 2);
                major = entry[6]; minor = entry[7];
            }
            else continue;
            if (!size || !address) { malformed = 1; size = 0; continue; }
            break;
        }
    }
    if (!size) { status = malformed ? QUANTA_INVALID_DATA : QUANTA_NOT_PRESENT; goto finish; }
    if (size > TABLE_LIMIT) { status = QUANTA_LIMIT; goto finish; }
    if (size - 1 > UINT64_MAX - address) { status = QUANTA_INVALID_DATA; goto finish; }
    table = quanta_malloc(size);
    if (!table) { status = QUANTA_NO_MEMORY; goto finish; }
    status = read_blocks(hal, address, table, size);
    if (status == QUANTA_OK) status = quanta_smbios_parse(table, size, major, minor, out);
finish:
    quanta_free(table);
    quanta_free(rom);
    return status;
}
quanta_status quanta_smbios_mem_init(quanta_metric_context *context)
{
    context->private_data = quanta_calloc(1, sizeof(quanta_board_snapshot));
    return context->private_data ? QUANTA_OK : QUANTA_NO_MEMORY;
}
quanta_status quanta_smbios_mem_enumerate(quanta_metric_context *context, quanta_discovery_sink *sink)
{ return quanta_discover_single(context, sink); }
quanta_status quanta_smbios_mem_collect(quanta_metric_context *context)
{
    quanta_board_snapshot *snapshot = context->private_data;
    snapshot->status = scan_table(context->hal, &snapshot->table);
    return snapshot->status;
}
void quanta_smbios_mem_destroy(quanta_metric_context *context)
{ quanta_free(context->private_data); context->private_data = NULL; }
quanta_status quanta_smbios_mem_read(quanta_metric_context *context, quanta_instance *instance,
                                    quanta_value *value, uint64_t *window, uint32_t *quality)
{
    quanta_board_snapshot *snapshot = context->private_data;
    const quanta_dmi_string *field;
    (void)window; (void)quality;
    if (snapshot->status != QUANTA_OK) return snapshot->status;
    if (!quanta_strcmp(instance->metric->arg, "board_model")) field = &snapshot->table.fields[QUANTA_BOARD_MODEL];
    else if (!quanta_strcmp(instance->metric->arg, "bios_vendor")) field = &snapshot->table.fields[QUANTA_BIOS_VENDOR];
    else return QUANTA_UNSUPPORTED;
    if (field->status != QUANTA_OK) return field->status;
    return quanta_copy(value->as.string, sizeof(value->as.string), field->text) ? QUANTA_OK : QUANTA_LIMIT;
}
