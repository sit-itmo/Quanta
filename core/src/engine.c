/* SPDX-License-Identifier: MIT */
#include "engine_internal.h"
#include <math.h>

quanta_status quanta_engine_diagnostic(quanta_engine *e, quanta_status s, const char *m)
{
    return e->sink && e->sink->emit_diagnostic ? e->sink->emit_diagnostic(e->sink->context, s, m)
                                               : QUANTA_OK;
}

int quanta_engine_stopping(quanta_engine *e)
{
    int stop;
    quanta_mutex_lock(&e->control_mutex);
    stop = e->stopped;
    quanta_mutex_unlock(&e->control_mutex);
    return stop || quanta_interrupted();
}

void quanta_engine_stop(quanta_engine *e)
{
    if (!e)
    {
        return;
    }
    quanta_mutex_lock(&e->control_mutex);
    e->stopped = 1;
    quanta_mutex_unlock(&e->control_mutex);
}

void quanta_engine_refresh(quanta_engine *e)
{
    if (!e)
    {
        return;
    }
    quanta_mutex_lock(&e->control_mutex);
    e->force = 1;
    quanta_mutex_unlock(&e->control_mutex);
}

int quanta_engine_consume_refresh(quanta_engine *e)
{
    int force;
    quanta_mutex_lock(&e->control_mutex);
    force = e->force;
    e->force = 0;
    quanta_mutex_unlock(&e->control_mutex);
    return force;
}

quanta_status quanta_engine_create_with_catalog(const quanta_plan *plan, const quanta_paths *paths,
                                                const quanta_catalog *catalog, quanta_engine **out,
                                                char *err, size_t cap)
{
    quanta_engine *e;
    size_t n, i, j;
    const quanta_metric_desc *const *r;
    quanta_status s;
    if (!out || (cap && !err))
    {
        return QUANTA_INVALID_DATA;
    }
    *out = NULL;
    if (!plan || !catalog)
    {
        return QUANTA_INVALID_DATA;
    }
    s = quanta_catalog_validate(catalog, err, cap);
    if (s != QUANTA_OK)
    {
        return s;
    }
    r = catalog->metrics;
    n = catalog->count;
    e = (quanta_engine *)calloc(1, sizeof(*e));
    if (!e)
    {
        return QUANTA_NO_MEMORY;
    }
    if (quanta_mutex_init(&e->control_mutex))
    {
        free(e);
        return QUANTA_IO_ERROR;
    }
    e->plan = *plan;
    e->catalog = *catalog;
    s = quanta_plan_validate_with_catalog(&e->plan, &e->catalog, err, cap);
    if (s != QUANTA_OK)
    {
        goto error_keep;
    }
    plan = &e->plan;
    if (!quanta_copy(e->roots[0], sizeof(e->roots[0]),
                     paths && paths->proc_root ? paths->proc_root : "/proc") ||
        !quanta_copy(e->roots[1], sizeof(e->roots[1]),
                     paths && paths->sys_root ? paths->sys_root : "/sys") ||
        !quanta_copy(e->roots[2], sizeof(e->roots[2]),
                     paths && paths->etc_root ? paths->etc_root : "/etc"))
    {
        s = QUANTA_LIMIT;
        goto error;
    }
    e->paths.proc_root = e->roots[0];
    e->paths.sys_root = e->roots[1];
    e->paths.etc_root = e->roots[2];
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < plan->select_count; j++)
        {
            if (quanta_catalog_selects(&e->catalog, &plan->selects[j], r[i]))
            {
                e->providers[r[i]->provider->index] = r[i]->provider;
            }
        }
    }
    for (i = 0; i < QUANTA_PROVIDER_COUNT; i++)
    {
        if (e->providers[i])
        {
            e->ctx[i].plan = &e->plan;
            e->ctx[i].catalog = &e->catalog;
            e->ctx[i].paths = e->paths;
            e->provider_opened[i] = 1;
            e->open_status[i] =
                e->providers[i]->open ? e->providers[i]->open(&e->ctx[i]) : QUANTA_OK;
            if (e->open_status[i] == QUANTA_NO_MEMORY)
            {
                s = QUANTA_NO_MEMORY;
                goto error;
            }
        }
    }
    s = quanta_engine_discover(e);
    if (s != QUANTA_OK)
    {
        goto error;
    }
    for (i = 0; i < plan->select_count; i++)
    {
        int found = 0;
        for (j = 0; j < e->binding_count; j++)
        {
            if (e->bindings[j].active && e->bindings[j].meta.selection_index == i)
            {
                found = 1;
                break;
            }
        }
        if (!found && !plan->selects[i].optional)
        {
            snprintf(err, cap,
                     "no discoverable instances for '%s'; use optional for absent "
                     "hardware",
                     plan->selects[i].pattern);
            s = QUANTA_NOT_PRESENT;
            goto error_keep;
        }
    }
    *out = e;
    return QUANTA_OK;
error:
    snprintf(err, cap, "engine initialization: %s%s", quanta_status_name(s),
             s == QUANTA_BAD_QUERY ? " (strict polling is below a discovered device minimum)" : "");
error_keep:
    quanta_engine_destroy(e);
    return s;
}

void quanta_engine_destroy(quanta_engine *e)
{
    unsigned i;
    if (!e)
    {
        return;
    }
    for (i = 0; i < QUANTA_PROVIDER_COUNT; i++)
    {
        if (e->provider_opened[i] && e->providers[i] && e->providers[i]->close)
        {
            e->providers[i]->close(&e->ctx[i]);
        }
    }
    free(e->channels);
    free(e->bindings);
    quanta_mutex_destroy(&e->control_mutex);
    free(e);
}

int quanta_engine_has_sample_errors(const quanta_engine *e)
{
    return e ? e->has_errors : 0;
}

void quanta_engine_explain(const quanta_engine *e, FILE *out)
{
    size_t i;
    if (!e || !out)
    {
        return;
    }
    fprintf(out, "Quanta Query plan | mode=%s | %s | selections=%zu\n",
            e->plan.mode == QUANTA_MODE_GUI   ? "gui"
            : e->plan.mode == QUANTA_MODE_TUI ? "tui"
                                              : "file",
            e->plan.once ? "once" : "watch", e->plan.select_count);
    fprintf(out, "Acquisition channels: %zu; output series/views: %zu\n", e->channel_count,
            e->binding_count);
    for (i = 0; i < e->binding_count; i++)
    {
        if (e->bindings[i].active)
        {
            const quanta_binding *b = &e->bindings[i];
            char name[1024];
            quanta_series_name(name, sizeof(name), &b->meta);
            fprintf(out,
                    "  %s\n    output=%" PRIu64 " source=%" PRIu64
                    " provider=%s type=%s unit=%s period=%.3fs policy=%s\n",
                    name, b->meta.id, b->meta.source_id, b->meta.metric->provider->id,
                    quanta_type_name(b->meta.type), b->meta.unit,
                    (double)b->meta.effective_period_ns / 1e9,
                    b->meta.metric->refresh == QUANTA_REFRESH_ONCE       ? "once"
                    : b->meta.metric->refresh == QUANTA_REFRESH_ON_EVENT ? "rediscovery"
                                                                         : "poll");
        }
    }
}

uint64_t quanta_engine_provider_refreshes(const quanta_engine *e, const char *name)
{
    unsigned i;
    if (!e || !name)
    {
        return 0;
    }
    for (i = 0; i < QUANTA_PROVIDER_COUNT; i++)
    {
        if (e->providers[i] && !strcmp(e->providers[i]->id, name))
        {
            return e->refresh_count[i];
        }
    }
    return 0;
}
