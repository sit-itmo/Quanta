/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "frontend_internal.h"

void quanta_format_value(char *out, size_t cap, const quanta_value *v, const char *unit,
                         const quanta_view_spec *view, int machine)
{
    double n, scale = 1;
    if (!out || !cap)
    {
        return;
    }
    out[0] = 0;
    if (quanta_value_validate(v) != QUANTA_OK)
    {
        quanta_copy(out, cap, "invalid");
        return;
    }
    const char *du = view ? view->unit : "";
    int prec = view ? view->precision : 2;
    if (v->type == QUANTA_STRING)
    {
        quanta_safe_text(out, cap, v->as.string);
        return;
    }
    if (v->type == QUANTA_BOOL)
    {
        quanta_snprintf(out, cap, "%s", v->as.boolean ? "true" : "false");
        return;
    }
    if (v->type == QUANTA_BYTES)
    {
        size_t i, off = 0;
        out[0] = 0;
        for (i = 0; i < v->as.bytes.size && off + 2 < cap; i++)
        {
            off += (size_t)quanta_snprintf(out + off, cap - off, "%02x", v->as.bytes.data[i]);
        }
        return;
    }
    if (machine || !view)
    {
        switch (v->type)
        {
            case QUANTA_U64:
                quanta_snprintf(out, cap, "%" PRIu64, v->as.u64);
                return;
            case QUANTA_S64:
                quanta_snprintf(out, cap, "%" PRId64, v->as.s64);
                return;
            default:
                quanta_snprintf(out, cap, "%.17g", v->as.f64);
                return;
        }
    }
    if (!quanta_value_number(v, &n))
    {
        quanta_snprintf(out, cap, "invalid");
        return;
    }
    if (*du)
    {
        quanta_unit_scale(unit, du, &scale);
    }
    else if (!quanta_strcmp(unit, "1"))
    {
        du = "%";
        scale = 100;
    }
    else if (!quanta_strcmp(unit, "B") || !quanta_strcmp(unit, "B/s"))
    {
        static const char *const u[] = {"B", "KiB", "MiB", "GiB", "TiB"};
        static const char *const r[] = {"B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"};
        unsigned k = 0;
        while (quanta_fabs(n * scale) >= 1024 && k < 4)
        {
            k++;
            scale /= 1024;
        }
        du = !quanta_strcmp(unit, "B") ? u[k] : r[k];
    }
    else
    {
        du = unit;
    }
    if (prec < 0)
    {
        prec = 2;
    }
    if (prec > 9)
    {
        prec = 9;
    }
    if (scale == 1 && v->type == QUANTA_U64)
    {
        quanta_snprintf(out, cap, "%" PRIu64 "%s%s", v->as.u64, *du ? " " : "", du);
    }
    else if (scale == 1 && v->type == QUANTA_S64)
    {
        quanta_snprintf(out, cap, "%" PRId64 "%s%s", v->as.s64, *du ? " " : "", du);
    }
    else
    {
        quanta_snprintf(out, cap, "%.*f%s%s", prec, n * scale, *du ? " " : "", du);
    }
}
