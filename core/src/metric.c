/**
 * @file core/src/metric.c
 *
 * Match selects and labels, discover a singleton instance and perform a lightweight single-
 * metric read.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 *
 * Labels: key/value pairs selecting an instance, such as cpu="0". They describe the source, not
 * a widget caption.
 *
 * Sample: the value of a particular series together with its timestamp, status and quality
 * flags. An error must not be presented as a valid zero.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "core_internal.h"

int quanta_select_matches_metric(const quanta_select *select, const quanta_metric_desc *metric)
{
    return select && metric && quanta_glob_match(select->pattern, metric->key);
}

int quanta_select_matches(const quanta_select *select, const quanta_instance *instance)
{
    size_t i;
    if (!select || !instance || !quanta_select_matches_metric(select, instance->metric)) return 0;
    for (i = 0; i < select->label_count; ++i)
    {
        const char *value = quanta_label_get(instance->labels, instance->label_count, select->labels[i].key);
        if (!value || !quanta_glob_match(select->labels[i].value, value)) return 0;
    }
    return 1;
}

int quanta_metric_wanted(const quanta_metric_context *context, const quanta_metric_desc *metric)
{
    size_t i;
    if (!context || !metric) return 0;
    for (i = 0; i < context->metric_count; ++i)
        if (context->metrics[i] == metric) return 1;
    return 0;
}

quanta_status quanta_discover_single(quanta_metric_context *context, quanta_discovery_sink *sink)
{
    size_t i;
    if (!context || !sink || !sink->emit) return QUANTA_INVALID_DATA;
    for (i = 0; i < context->metric_count; ++i)
    {
        quanta_instance instance;
        quanta_status status;
        quanta_memset(&instance, 0, sizeof(instance));
        instance.metric = context->metrics[i];
        quanta_copy(instance.entity_key, sizeof(instance.entity_key), "system");
        status = sink->emit(sink->context, &instance);
        if (status != QUANTA_OK) return status;
    }
    return QUANTA_OK;
}

quanta_status quanta_metric_read_once(const quanta_metric_desc *metric, quanta_hal *hal,
                                    const quanta_paths *paths, quanta_sample *sample)
{
    quanta_metric_context context;
    quanta_instance instance;
    quanta_status status;
    uint64_t start;
    const quanta_metric_desc *selected[1];
    if (!sample) return QUANTA_INVALID_DATA;
    quanta_memset(sample, 0, sizeof(*sample));
    sample->status = QUANTA_INVALID_DATA;
    if (!metric || !metric->read || !metric->label_schema || metric->label_schema[0]) return sample->status;
    quanta_memset(&context, 0, sizeof(context));
    quanta_memset(&instance, 0, sizeof(instance));
    selected[0] = metric;
    {
        const quanta_catalog single = {selected, 1};
        if (quanta_catalog_validate(&single, NULL, 0) != QUANTA_OK) return sample->status;
    }
    context.metrics = selected;
    context.metric_count = 1;
    context.hal = hal;
    context.paths.proc_root = paths && paths->proc_root ? paths->proc_root : "/proc";
    context.paths.sys_root = paths && paths->sys_root ? paths->sys_root : "/sys";
    context.paths.etc_root = paths && paths->etc_root ? paths->etc_root : "/etc";
    instance.metric = metric;
    quanta_copy(instance.entity_key, sizeof(instance.entity_key), "system");
    sample->value.type = metric->type;
    start = quanta_now_ns();
    context.now_ns = start;
    status = metric->init ? metric->init(&context) : QUANTA_OK;
    if (status == QUANTA_OK && metric->collect) status = metric->collect(&context);
    context.now_ns = quanta_now_ns();
    context.acquisition_ns = context.now_ns - start;
    if (status == QUANTA_OK)
        status = metric->read(&context, &instance, &sample->value, &sample->window_ns, &sample->quality_flags);
    if ((unsigned)status > QUANTA_BAD_QUERY) status = QUANTA_INVALID_DATA;
    if (status == QUANTA_OK && (sample->value.type != metric->type || quanta_value_validate(&sample->value) != QUANTA_OK))
        status = QUANTA_INVALID_DATA;
    sample->status = status;
    sample->sequence = 1;
    sample->measured_at_ns = context.now_ns;
    sample->read_duration_ns = quanta_now_ns() - start;
    if (status != QUANTA_OK)
        quanta_copy(sample->message, sizeof(sample->message), context.error[0] ? context.error : quanta_status_name(status));
    if (metric->destroy) metric->destroy(&context);
    return status;
}
