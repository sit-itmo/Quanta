// hal/windows/hwmon.c
#include "hwmon.h"
#include "pdh_set.h"
#include "windows_internal.h"

typedef struct win_hwmon_ctx
{
    quanta_pdh_set pdh;
    quanta_status status;
} win_hwmon_ctx;

static int add_channel(quanta_hwmon_snapshot *out, const quanta_hwmon_channel *ch)
{
    if (out->count == out->cap)
    {
        size_t cap = out->cap ? out->cap * 2 : 16;
        quanta_hwmon_channel *p;
        if (cap > QUANTA_MAX_CHANNELS)
        {
            return 0;
        }
        p = (quanta_hwmon_channel *)realloc(out->ch, cap * sizeof(*p));
        if (!p)
        {
            return 0;
        }
        out->ch = p;
        out->cap = cap;
    }
    out->ch[out->count++] = *ch;
    return 1;
}

quanta_status quanta_hal_hwmon_open(quanta_hwmon_snapshot *out, quanta_hal_context context)
{
    win_hwmon_ctx *win;
    (void)context;
    win = (win_hwmon_ctx *)calloc(1, sizeof(*win));
    if (!win)
    {
        return QUANTA_NO_MEMORY;
    }
    out->quanta_hal_private = win;
    win->status = quanta_pdh_open(&win->pdh, L"\\Thermal Zone Information(*)\\Temperature", 0);
    out->status = QUANTA_OK;
    return QUANTA_OK;
}

quanta_status quanta_hal_hwmon_discover(quanta_hwmon_snapshot *out, quanta_hal_context context)
{
    win_hwmon_ctx *win = (win_hwmon_ctx *)out->quanta_hal_private;
    size_t i;
    (void)context;
    out->count = 0;
    if (win->status != QUANTA_OK)
    {
        return win->status;
    }
    if (win->pdh.query)
    {
        win->status = quanta_pdh_discover(&win->pdh);
        if (win->status != QUANTA_OK)
        {
            return win->status;
        }
    }
    for (i = 0; i < win->pdh.count; i++)
    {
        quanta_hwmon_channel chan;
        const char *name = win->pdh.items[i].name;
        if (!strcmp(name, "_Total"))
        {
            continue;
        }
        memset(&chan, 0, sizeof(chan));
        chan.kind = HWMON_TEMP;
        chan.unit = HWMON_KELVIN;
        quanta_copy(chan.chip, sizeof(chan.chip), "acpi_thermal_zone");
        quanta_copy(chan.channel, sizeof(chan.channel), "temperature");
        quanta_copy(chan.label, sizeof(chan.label), name);
        quanta_copy(chan.device, sizeof(chan.device), name);
        quanta_copy(chan.resource, sizeof(chan.resource), name);
        if (snprintf(chan.entity_key, sizeof(chan.entity_key), "thermal:%s", name) >=
            (int)sizeof(chan.entity_key))
        {
            return QUANTA_LIMIT;
        }
        chan.min_poll_ns = QUANTA_NS_S;
        if (!add_channel(out, &chan))
        {
            return QUANTA_NO_MEMORY;
        }
    }
    out->status = QUANTA_OK;
    return QUANTA_OK;
}

quanta_status quanta_hal_hwmon_refresh(quanta_hwmon_snapshot *out, quanta_hal_context context)
{
    win_hwmon_ctx *win = (win_hwmon_ctx *)out->quanta_hal_private;
    (void)context;
    if (win->pdh.query)
    {
        return quanta_pdh_collect(&win->pdh);
    }
    return win->status;
}

quanta_status quanta_hal_hwmon_read(const quanta_hwmon_snapshot *d, const quanta_hwmon_channel *ch,
                                    double *raw, quanta_status *fault)
{
    const win_hwmon_ctx *win = (const win_hwmon_ctx *)d->quanta_hal_private;
    const quanta_pdh_item *item;
    *fault = QUANTA_OK;
    item = quanta_pdh_find(&win->pdh, ch->resource);
    if (!item)
    {
        return QUANTA_GONE;
    }
    if (item->status != QUANTA_OK)
    {
        return item->status;
    }
    *raw = item->value;
    return QUANTA_OK;
}

void quanta_hal_hwmon_cleanup(quanta_hwmon_snapshot *d)
{
    win_hwmon_ctx *win = (win_hwmon_ctx *)d->quanta_hal_private;
    if (win)
    {
        quanta_pdh_close(&win->pdh);
        free(win);
        d->quanta_hal_private = NULL;
    }
    free(d->ch);
    d->ch = NULL;
    d->count = d->cap = 0;
}
