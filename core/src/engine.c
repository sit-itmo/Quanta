/**
 * @file core/src/engine.c
 *
 * Create and destroy the engine, manage resource ownership, group compatible operations and
 * handle stop requests.
 *
 * Plan: a parsed query containing selected metrics, polling intervals, history, HAL and
 * presentation settings. Parsing does not access hardware.
 *
 * Catalog: an immutable set of metric descriptors with unique names. It neither stores
 * measurement results nor selects providers.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "engine_internal.h"

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


/* Sharing is a scheduler optimization over a metric's lifecycle functions.
 * There is no public source identity and no alternative implementation selection. */
static int same_acquisition(const quanta_metric_desc *a, const quanta_metric_desc *b)
{
    return (a->init || a->collect) && a->init == b->init && a->collect == b->collect &&
           a->enumerate == b->enumerate && a->destroy == b->destroy;
}

static quanta_status add_metric(quanta_engine *e, const quanta_metric_desc *metric)
{
    size_t index, count;
    quanta_acquisition *a;
    const quanta_metric_desc **selected;
    for (index = 0; index < e->acquisition_count; ++index)
        if (same_acquisition(e->acquisitions[index].representative, metric)) break;
    if (index == e->acquisition_count)
    {
        quanta_acquisition *grown;
        if (index >= QUANTA_MAX_ACQUISITIONS) return QUANTA_LIMIT;
        grown = (quanta_acquisition *)quanta_realloc(e->acquisitions, (index + 1) * sizeof(*grown));
        if (!grown) return QUANTA_NO_MEMORY;
        e->acquisitions = grown;
        quanta_memset(&grown[index], 0, sizeof(grown[index]));
        grown[index].representative = metric;
        ++e->acquisition_count;
    }
    a = &e->acquisitions[index];
    count = a->context.metric_count;
    selected = (const quanta_metric_desc **)quanta_realloc(a->selected, (count + 1) * sizeof(*selected));
    if (!selected) return QUANTA_NO_MEMORY;
    selected[count] = metric;
    a->selected = selected;
    a->context.metrics = selected;
    a->context.metric_count = count + 1;
    return QUANTA_OK;
}

quanta_status quanta_engine_create_with_catalog(const quanta_plan *plan, const quanta_paths *paths,
                                               const quanta_catalog *catalog, quanta_engine **out,
                                               char *error, size_t capacity)
{
    quanta_engine_options options = {0};
    options.paths = paths;
    return quanta_engine_create_with_options(plan, catalog, &options, out, error, capacity);
}

quanta_status quanta_engine_create_with_options(const quanta_plan *plan, const quanta_catalog *catalog,
                                               const quanta_engine_options *options, quanta_engine **out,
                                               char *err, size_t cap)
{
    quanta_engine *e;
    size_t i, j;
    quanta_status status;
    const quanta_paths *paths = options ? options->paths : NULL;
    if (!out || (cap && !err)) return QUANTA_INVALID_DATA;
    *out = NULL;
    if (err && cap) err[0] = '\0';
    if (!plan || !catalog) return QUANTA_INVALID_DATA;
    status = quanta_catalog_validate(catalog, err, cap);
    if (status != QUANTA_OK) return status;
    e = (quanta_engine *)quanta_calloc(1, sizeof(*e));
    if (!e) return QUANTA_NO_MEMORY;
    if (quanta_mutex_init(&e->control_mutex))
    {
        quanta_free(e);
        return QUANTA_IO_ERROR;
    }
    e->plan = *plan;
    e->catalog = *catalog;
    status = quanta_plan_validate_with_catalog(&e->plan, &e->catalog, err, cap);
    if (status != QUANTA_OK) goto error_keep;
    plan = &e->plan;
    if (!quanta_copy(e->roots[0], sizeof(e->roots[0]), paths && paths->proc_root ? paths->proc_root : "/proc") ||
        !quanta_copy(e->roots[1], sizeof(e->roots[1]), paths && paths->sys_root ? paths->sys_root : "/sys") ||
        !quanta_copy(e->roots[2], sizeof(e->roots[2]), paths && paths->etc_root ? paths->etc_root : "/etc"))
    {
        status = QUANTA_LIMIT;
        goto error;
    }
    e->paths.proc_root = e->roots[0];
    e->paths.sys_root = e->roots[1];
    e->paths.etc_root = e->roots[2];
    if (options && options->hal)
    {
        quanta_hal_info info;
        e->hal = options->hal;
        status = quanta_hal_get_info(e->hal, &info);
        if (status != QUANTA_OK) goto error;
        if (plan->hal[0] && quanta_strcmp(plan->hal, info.name))
        {
            quanta_snprintf(err, cap, "query HAL '%s' conflicts with injected HAL '%s'", plan->hal, info.name);
            status = QUANTA_BAD_QUERY;
            goto error_keep;
        }
    }
    else
    {
        quanta_hal_options hal_options = {0};
        hal_options.name = plan->hal;
        status = quanta_hal_open(&hal_options, &e->hal);
        if (status != QUANTA_OK)
        {
            quanta_snprintf(err, cap, "cannot open HAL '%s': %s", plan->hal[0] ? plan->hal : "native", quanta_status_name(status));
            goto error_keep;
        }
        e->owns_hal = 1;
    }
    for (i = 0; i < catalog->count; ++i)
    {
        for (j = 0; j < plan->select_count; ++j)
        {
            if (quanta_catalog_selects(catalog, &plan->selects[j], catalog->metrics[i]))
            {
                status = add_metric(e, catalog->metrics[i]);
                if (status != QUANTA_OK) goto error;
                break;
            }
        }
    }
    for (i = 0; i < e->acquisition_count; ++i)
    {
        quanta_acquisition *a = &e->acquisitions[i];
        a->context.paths = e->paths;
        a->context.hal = e->hal;
        a->initialized = 1; /* destroy balances partial initialization too. */
        a->init_status = a->representative->init ? a->representative->init(&a->context) : QUANTA_OK;
        if (a->init_status == QUANTA_NO_MEMORY)
        {
            status = QUANTA_NO_MEMORY;
            goto error;
        }
    }
    status = quanta_engine_discover(e);
    if (status != QUANTA_OK) goto error;
    for (i = 0; i < plan->select_count; ++i)
    {
        int found = 0;
        for (j = 0; j < e->binding_count; ++j)
            if (e->bindings[j].active && e->bindings[j].meta.selection_index == i) { found = 1; break; }
        if (!found && !plan->selects[i].optional)
        {
            quanta_snprintf(err, cap, "no discoverable instances for '%s'; use optional for absent hardware", plan->selects[i].pattern);
            status = QUANTA_NOT_PRESENT;
            goto error_keep;
        }
    }
    *out = e;
    return QUANTA_OK;
error:
    quanta_snprintf(err, cap, "engine initialization: %s%s", quanta_status_name(status),
             status == QUANTA_BAD_QUERY ? " (strict polling is below a discovered device minimum)" : "");
error_keep:
    quanta_engine_destroy(e);
    return status;
}

void quanta_engine_destroy(quanta_engine *e)
{
    size_t i;
    if (!e) return;
    for (i = 0; i < e->acquisition_count; ++i)
    {
        quanta_acquisition *a = &e->acquisitions[i];
        if (a->initialized && a->representative->destroy) a->representative->destroy(&a->context);
        quanta_free(a->selected);
    }
    quanta_free(e->acquisitions);
    quanta_free(e->channels);
    quanta_free(e->bindings);
    if (e->owns_hal) quanta_hal_close(e->hal);
    quanta_mutex_destroy(&e->control_mutex);
    quanta_free(e);
}
int quanta_engine_has_sample_errors(const quanta_engine *e)
{
    return e ? e->has_errors : 0;
}

void quanta_engine_explain(const quanta_engine *e, quanta_file *out)
{
    size_t i;
    if (!e || !out)
    {
        return;
    }
    {
        quanta_hal_info info;
        if (quanta_hal_get_info(e->hal, &info) == QUANTA_OK)
            quanta_fprintf(out, "HAL: %s; implemented=0x%08x; hardware-writes=%s\n",
                    info.name, info.capabilities, info.writes_enabled ? "enabled" : "disabled");
    }
    quanta_fprintf(out, "Quanta Query plan | mode=%s | %s | selections=%zu\n",
            e->plan.mode == QUANTA_MODE_GUI   ? "gui"
            : e->plan.mode == QUANTA_MODE_TUI ? "tui"
                                              : "file",
            e->plan.once ? "once" : "watch", e->plan.select_count);
    quanta_fprintf(out, "Acquisition channels: %zu; output series/views: %zu\n", e->channel_count,
            e->binding_count);
    for (i = 0; i < e->binding_count; i++)
    {
        if (e->bindings[i].active)
        {
            const quanta_binding *b = &e->bindings[i];
            char name[1024];
            quanta_series_name(name, sizeof(name), &b->meta);
            quanta_fprintf(out,
                    "  %s\n    output=%" PRIu64 " source=%" PRIu64
                    " method=%s type=%s unit=%s period=%.3fs policy=%s\n",
                    name, b->meta.id, b->meta.source_id, quanta_metric_source(b->meta.metric),
                    quanta_type_name(b->meta.type), b->meta.unit,
                    (double)b->meta.effective_period_ns / 1e9,
                    b->meta.metric->refresh == QUANTA_REFRESH_ONCE       ? "once"
                    : b->meta.metric->refresh == QUANTA_REFRESH_ON_EVENT ? "rediscovery"
                                                                         : "poll");
        }
    }
}


uint64_t quanta_engine_metric_acquisitions(const quanta_engine *e, const char *key)
{
    size_t i, j;
    if (!e || !key) return 0;
    for (i = 0; i < e->acquisition_count; ++i)
    {
        const quanta_acquisition *a = &e->acquisitions[i];
        for (j = 0; j < a->context.metric_count; ++j)
            if (!quanta_strcmp(a->selected[j]->key, key)) return a->refresh_count;
    }
    return 0;
}
