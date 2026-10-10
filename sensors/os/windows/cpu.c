/**
 * @file sensors/os/windows/cpu.c
 *
 * Windows CPU/CPU-instance collector: use PDH and system CPU counters to populate shared-
 * snapshot data and statuses.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "cpu.h"
#include "pdh_set.h"
#include "windows_internal.h"
#include "normalize.h"
#include "windows_cpu.h"
#include <math.h>

typedef struct windows_cpu_state
{
    quanta_pdh_set pdh;
    int active;
} windows_cpu_state;

static quanta_status add_row(quanta_cpu_snapshot *data, const quanta_cpu_row *row)
{
    if (data->count == data->cap)
    {
        size_t capacity = data->cap ? data->cap * 2 : 32;
        quanta_cpu_row *rows;
        if (data->count >= QUANTA_MAX_CHANNELS)
        {
            return QUANTA_LIMIT;
        }
        if (capacity > QUANTA_MAX_CHANNELS)
        {
            capacity = QUANTA_MAX_CHANNELS;
        }
        rows = (quanta_cpu_row *)quanta_realloc(data->rows, capacity * sizeof(*rows));
        if (!rows)
        {
            return QUANTA_NO_MEMORY;
        }
        data->rows = rows;
        data->cap = capacity;
    }
    data->rows[data->count++] = *row;
    return QUANTA_OK;
}

static void set_fraction(quanta_cpu_row *row, const quanta_pdh_item *item)
{
    row->direct_utilization = 1;
    row->utilization_status = item->status;
    row->window_ns = item->window_ns;
    if (item->status == QUANTA_OK)
    {
        if (!quanta_isfinite(item->value) || item->value < 0.0 || item->value > 100.0)
        {
            row->utilization_status = QUANTA_INVALID_DATA;
        }
        else
        {
            row->fraction = item->value / 100.0;
        }
    }
}

quanta_status quanta_native_cpu_refresh(quanta_cpu_snapshot *out, quanta_snapshot_context context)
{
    windows_cpu_state *state = (windows_cpu_state *)out->private_data;
    quanta_cpu_row aggregate;
    quanta_status status;
    WORD groups = GetActiveProcessorGroupCount();
    FILETIME idle, kernel, user;
    (void)context;
    if (!state)
    {
        state = (windows_cpu_state *)quanta_calloc(1, sizeof(*state));
        if (!state)
        {
            return QUANTA_NO_MEMORY;
        }
        out->private_data = state;
    }
    out->count = 0;
    out->tick_frequency_hz = UINT64_C(10000000);
    quanta_memset(&aggregate, 0, sizeof(aggregate));
    quanta_copy(aggregate.name, sizeof(aggregate.name), "cpu");
    aggregate.accounting_status = QUANTA_UNSUPPORTED;
    aggregate.utilization_status = QUANTA_NO_DATA;
    aggregate.direct_utilization = groups != 1;
    /* GetSystemTimes is group-scoped on large machines; never advertise it as an
     * all-group sum. */
    if (groups == 1)
    {
        if (GetSystemTimes(&idle, &kernel, &user))
        {
            uint64_t idle_ticks = quanta_win_filetime(idle);
            uint64_t kernel_ticks = quanta_win_filetime(kernel);
            if (kernel_ticks < idle_ticks)
            {
                aggregate.accounting_status = QUANTA_INVALID_DATA;
            }
            else
            {
                aggregate.t[CPU_USER] = quanta_win_filetime(user);
                aggregate.t[CPU_SYSTEM] = kernel_ticks - idle_ticks;
                aggregate.t[CPU_IDLE] = idle_ticks;
                aggregate.accounting_status = QUANTA_OK;
            }
        }
        else
        {
            aggregate.accounting_status = quanta_win_status(GetLastError());
        }
    }
    status = add_row(out, &aggregate);
    if (status != QUANTA_OK)
    {
        return status;
    }
    if (!state->active)
    {
        status = quanta_pdh_open(&state->pdh, L"\\Processor Information(*)\\% Processor Time", 1);
        if (status == QUANTA_NO_MEMORY || status == QUANTA_LIMIT)
        {
            return status;
        }
        state->active = status == QUANTA_OK;
        out->rows[0].utilization_status = status;
    }
    if (state->active)
    {
        status = quanta_pdh_discover(&state->pdh);
        if (status == QUANTA_NO_MEMORY || status == QUANTA_LIMIT)
        {
            return status;
        }
        if (status == QUANTA_OK)
        {
            status = quanta_pdh_collect(&state->pdh);
        }
        if (status == QUANTA_NO_MEMORY || status == QUANTA_LIMIT)
        {
            return status;
        }
        if (status != QUANTA_OK)
        {
            out->rows[0].utilization_status = status;
        }
        else
        {
            for (size_t i = 0; i < state->pdh.count; i++)
            {
                const quanta_pdh_item *item = &state->pdh.items[i];
                quanta_cpu_row row;
                unsigned group = 0, number = 0;
                int kind = quanta_win_cpu_instance(item->name, &group, &number);
                if (!quanta_strcmp(item->name, "_Total"))
                {
                    set_fraction(&out->rows[0], item);
                    continue;
                }
                if (kind != 1)
                {
                    continue;
                }
                quanta_memset(&row, 0, sizeof(row));
                quanta_snprintf(row.name, sizeof(row.name), "cpu%u:%u", group, number);
                row.group = group;
                row.number = number;
                row.accounting_status = QUANTA_UNSUPPORTED;
                set_fraction(&row, item);
                status = add_row(out, &row);
                if (status != QUANTA_OK)
                {
                    return status;
                }
            }
        }
    }
    out->time_ns = quanta_now_ns();
    return QUANTA_OK;
}

void quanta_native_cpu_cleanup(quanta_cpu_snapshot *data)
{
    windows_cpu_state *state;
    if (!data)
    {
        return;
    }
    state = (windows_cpu_state *)data->private_data;
    if (state)
    {
        quanta_pdh_close(&state->pdh);
        quanta_free(state);
    }
    quanta_free(data->rows);
    quanta_memset(data, 0, sizeof(*data));
}
