/* SPDX-License-Identifier: MIT */
#include "network.h"
#include "windows_internal.h"
#include <iphlpapi.h>

quanta_status quanta_hal_network_refresh(quanta_network_snapshot *out, quanta_hal_context context)
{
    MIB_IF_TABLE2 *table = NULL;
    DWORD error;
    ULONG i;
    (void)context;
    error = GetIfTable2(&table);
    if (error != NO_ERROR)
    {
        return out->status = quanta_win_status(error);
    }
    if (table->NumEntries > QUANTA_MAX_CHANNELS)
    {
        FreeMibTable(table);
        return QUANTA_LIMIT;
    }
    if (table->NumEntries > out->cap)
    {
        quanta_network_row *rows = realloc(out->rows, (size_t)table->NumEntries * sizeof(*rows));
        if (!rows)
        {
            FreeMibTable(table);
            return QUANTA_NO_MEMORY;
        }
        out->rows = rows;
        out->cap = table->NumEntries;
    }
    out->count = 0;
    for (i = 0; i < table->NumEntries; ++i)
    {
        const MIB_IF_ROW2 *source = &table->Table[i];
        quanta_network_row *row = &out->rows[out->count++];
        const GUID *guid = &source->InterfaceGuid;
        ULONG j;
        size_t offset = 0;
        memset(row, 0, sizeof(*row));
        row->ifindex = source->InterfaceIndex;
        if (!quanta_win_utf8(row->name, sizeof(row->name), source->Alias) || !row->name[0])
        {
            snprintf(row->name, sizeof(row->name), "if-%lu", (unsigned long)source->InterfaceIndex);
        }
        snprintf(row->entity_key, sizeof(row->entity_key), "luid:%016" PRIx64,
                 (uint64_t)source->InterfaceLuid.Value);
        snprintf(row->guid, sizeof(row->guid), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 (unsigned long)guid->Data1, (unsigned)guid->Data2, (unsigned)guid->Data3,
                 (unsigned)guid->Data4[0], (unsigned)guid->Data4[1], (unsigned)guid->Data4[2],
                 (unsigned)guid->Data4[3], (unsigned)guid->Data4[4], (unsigned)guid->Data4[5],
                 (unsigned)guid->Data4[6], (unsigned)guid->Data4[7]);
        row->rx_bytes = source->InOctets;
        row->tx_bytes = source->OutOctets;
        if (UINT64_MAX - source->InUcastPkts < source->InNUcastPkts ||
            UINT64_MAX - source->OutUcastPkts < source->OutNUcastPkts)
        {
            row->counters_status = QUANTA_INVALID_DATA;
        }
        else
        {
            row->rx_packets = source->InUcastPkts + source->InNUcastPkts;
            row->tx_packets = source->OutUcastPkts + source->OutNUcastPkts;
        }
        row->rx_errors = source->InErrors;
        row->tx_errors = source->OutErrors;
        row->rx_dropped = source->InDiscards;
        row->tx_dropped = source->OutDiscards;
        row->rx_link_bps = source->ReceiveLinkSpeed;
        row->tx_link_bps = source->TransmitLinkSpeed;
        if (source->PhysicalAddressLength > sizeof(source->PhysicalAddress))
        {
            row->address_status = QUANTA_INVALID_DATA;
        }
        else if (!source->PhysicalAddressLength)
        {
            row->address_status = QUANTA_NO_DATA;
        }
        else
        {
            for (j = 0; j < source->PhysicalAddressLength; ++j)
            {
                offset +=
                    (size_t)snprintf(row->address + offset, sizeof(row->address) - offset, "%s%02x",
                                     j ? ":" : "", (unsigned)source->PhysicalAddress[j]);
            }
        }
        switch (source->OperStatus)
        {
            case IfOperStatusUp:
                quanta_copy(row->state, sizeof(row->state), "up");
                break;
            case IfOperStatusDown:
                quanta_copy(row->state, sizeof(row->state), "down");
                break;
            case IfOperStatusTesting:
                quanta_copy(row->state, sizeof(row->state), "testing");
                break;
            case IfOperStatusDormant:
                quanta_copy(row->state, sizeof(row->state), "dormant");
                break;
            case IfOperStatusNotPresent:
                quanta_copy(row->state, sizeof(row->state), "notpresent");
                break;
            case IfOperStatusLowerLayerDown:
                quanta_copy(row->state, sizeof(row->state), "lowerlayerdown");
                break;
            default:
                quanta_copy(row->state, sizeof(row->state), "unknown");
                break;
        }
    }
    FreeMibTable(table);
    return out->status = QUANTA_OK;
}

void quanta_hal_network_cleanup(quanta_network_snapshot *data)
{
    if (!data)
    {
        return;
    }
    free(data->rows);
    data->rows = NULL;
    data->count = data->cap = 0;
}
