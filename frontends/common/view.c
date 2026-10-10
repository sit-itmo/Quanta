/**
 * @file frontends/common/view.c
 *
 * Shared presentation calculations: units, scales, bounds and display values.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 *
 * History: bounded storage for a series of samples. The latest sample and the latest successful
 * sample may differ.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "frontend_internal.h"

void quanta_item_value(char *out, size_t cap, const quanta_item *p, int redact)
{
    if (redact && (p->meta.metric->flags & QUANTA_SENSITIVE))
    {
        quanta_copy(out, cap, "[redacted]");
        return;
    }
    if (p->has_last && p->last.status == QUANTA_OK)
    {
        quanta_format_value(out, cap, &p->last.value, p->meta.unit, &p->meta.view, 0);
    }
    else if (p->has_good)
    {
        char b[QUANTA_TEXT_CAP + 64];
        quanta_format_value(b, sizeof(b), &p->good.value, p->meta.unit, &p->meta.view, 0);
        quanta_snprintf(out, cap, "%s (last good)", b);
    }
    else
    {
        quanta_copy(out, cap, "--");
    }
}

void quanta_item_status(char *out, size_t cap, const quanta_item *p, uint64_t now)
{
    quanta_status s = p->has_last ? p->last.status : QUANTA_NO_DATA;
    uint64_t age = p->has_good && now > p->good.measured_at_ns ? now - p->good.measured_at_ns : 0;
    int stale = p->has_good && p->meta.metric->refresh != QUANTA_REFRESH_ONCE &&
                age > QUANTA_MAX(p->meta.metric->stale_after_ns, 3 * p->meta.effective_period_ns);
    if (s != QUANTA_OK && p->has_good)
    {
        quanta_snprintf(out, cap, "%s / last good %.1fs ago%s", quanta_status_name(s), (double)age / 1e9,
                 stale ? " / STALE" : "");
    }
    else
    {
        quanta_snprintf(out, cap, "%s%s%s", quanta_status_name(s), stale ? " / STALE" : "",
                 p->has_last && (p->last.quality_flags & QUANTA_QUALITY_ESTIMATED) ? " / estimate"
                                                                                   : "");
    }
}
