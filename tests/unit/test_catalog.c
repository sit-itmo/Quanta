/* SPDX-License-Identifier: MIT
 * A core-only client: no builtin sensor or renderer is linked.
 * Core links only the minimal HAL and OSAL infrastructure.
 * Mock metric implementations exercise public extension contracts without real hardware. */
#include "quanta/osal.h"
#include "quanta/hal.h"
#include "quanta/quanta.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            quanta_fprintf(quanta_stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct mock_state
{
    unsigned method, refreshes;
} mock_state;

typedef struct mock_control
{
    unsigned opens[2], closes[2], reads[2];
    quanta_status open_result;
    quanta_hal *observed_hal;
    int invalid_discovery, invalid_value;
} mock_control;

static mock_control control;
static const quanta_metric_desc alternate_count;

static quanta_status open_mock(quanta_metric_context *context, unsigned method)
{
    mock_state *state = (mock_state *)quanta_calloc(1, sizeof(*state));
    control.opens[method]++;
    control.observed_hal = context->hal;
    if (!state)
    {
        return QUANTA_NO_MEMORY;
    }
    state->method = method;
    context->private_data = state;
    return method ? QUANTA_OK : control.open_result;
}

static quanta_status open_primary(quanta_metric_context *context)
{
    return open_mock(context, 0);
}

static quanta_status open_secondary(quanta_metric_context *context)
{
    return open_mock(context, 1);
}

static void close_mock(quanta_metric_context *context)
{
    mock_state *state = (mock_state *)context->private_data;
    if (state)
    {
        control.closes[state->method]++;
        quanta_free(state);
    }
    context->private_data = NULL;
}

static quanta_status refresh_mock(quanta_metric_context *context)
{
    mock_state *state = (mock_state *)context->private_data;
    state->refreshes++;
    return QUANTA_OK;
}

static quanta_status discover_primary(quanta_metric_context *context, quanta_discovery_sink *sink)
{
    if (control.invalid_discovery)
    {
        quanta_instance instance;
        quanta_memset(&instance, 0, sizeof(instance));
        instance.metric = context->metrics[0];
        quanta_copy(instance.entity_key, sizeof(instance.entity_key), "fixture");
        if (control.invalid_discovery == 1)
        {
            instance.label_count = QUANTA_LABEL_CAP + 1;
        }
        if (control.invalid_discovery == 2)
        {
            instance.metric = &alternate_count;
        }
        if (control.invalid_discovery == 3)
        {
            quanta_label_set(instance.labels, &instance.label_count, "not_in_schema", "x");
        }
        return sink->emit(sink->context, &instance);
    }
    return quanta_discover_single(context, sink);
}

static quanta_status discover_secondary(quanta_metric_context *context, quanta_discovery_sink *sink)
{
    return quanta_discover_single(context, sink);
}

static quanta_status read_mock(quanta_metric_context *context, quanta_instance *instance,
                               quanta_value *value, uint64_t *window, uint32_t *quality)
{
    mock_state *state = (mock_state *)context->private_data;
    (void)window;
    (void)quality;
    control.reads[state->method]++;
    if (control.invalid_value)
    {
        value->type = QUANTA_STRING;
        quanta_memset(value->as.string, 'X', sizeof(value->as.string));
        return QUANTA_OK;
    }
    if (!quanta_strcmp(instance->metric->arg, "property"))
    {
        value->as.u64 = 42;
    }
    else
    {
        value->as.u64 = (state->method + 1) * 100u + state->refreshes;
    }
    return QUANTA_OK;
}

#define COMMON_FIELDS                                                                              \
    .title = "Fixture", .description = "Not a real system metric", .doc_path = "test fixture",     \
    .semantic_version = 1, .type = QUANTA_U64, .unit = "B", .label_schema = "",                    \
    .dependencies = "mock", .read = read_mock, .recommended_poll_ns = 10 * QUANTA_NS_MS
static const quanta_metric_desc first_count = {
    COMMON_FIELDS,          .key = "mock.count",
    .kind = QUANTA_COUNTER, .refresh = QUANTA_REFRESH_POLL,
    .init = open_primary, .enumerate = discover_primary, .collect = refresh_mock, .destroy = close_mock,   .min_poll_ns = 10 * QUANTA_NS_MS,
    .arg = "count",         .counter_bits = 64};
static const quanta_metric_desc property = {COMMON_FIELDS,           .key = "mock.property",
                                            .kind = QUANTA_PROPERTY, .refresh = QUANTA_REFRESH_ONCE,
                                            .init = open_primary, .enumerate = discover_primary, .collect = refresh_mock, .destroy = close_mock,    .arg = "property"};
static const quanta_metric_desc alternate_count = {
    COMMON_FIELDS,          .key = "mock.alternate_count",
    .kind = QUANTA_COUNTER, .refresh = QUANTA_REFRESH_POLL,
    .init = open_secondary, .enumerate = discover_secondary, .collect = refresh_mock, .destroy = close_mock, .min_poll_ns = 10 * QUANTA_NS_MS,
    .arg = "count",         .counter_bits = 64};
static const quanta_metric_desc *const entries[] = {&first_count, &property, &alternate_count};
static const quanta_catalog catalog = {entries, 3};

typedef struct capture
{
    unsigned series_count, sample_count, ended, frames;
    quanta_status end_status, sample_failure;
    quanta_series series[8];
    quanta_sample samples[16];
} capture;

static quanta_status on_series(void *context, const quanta_series *series)
{
    capture *out = (capture *)context;
    if (out->series_count == 8)
    {
        return QUANTA_LIMIT;
    }
    out->series[out->series_count++] = *series;
    return QUANTA_OK;
}

static quanta_status on_sample(void *context, const quanta_sample *sample)
{
    capture *out = (capture *)context;
    if (out->sample_failure != QUANTA_OK)
    {
        return out->sample_failure;
    }
    if (out->sample_count == 16)
    {
        return QUANTA_LIMIT;
    }
    out->samples[out->sample_count++] = *sample;
    return QUANTA_OK;
}

static quanta_status on_frame(void *context)
{
    capture *out = (capture *)context;
    out->frames++;
    return QUANTA_OK;
}

static quanta_status on_end(void *context, quanta_status status)
{
    capture *out = (capture *)context;
    out->ended++;
    out->end_status = status;
    return QUANTA_OK;
}

static quanta_sink make_sink(capture *out)
{
    quanta_sink sink;
    quanta_memset(&sink, 0, sizeof(sink));
    sink.api_version = 1;
    sink.struct_size = sizeof(sink);
    sink.context = out;
    sink.upsert_series = on_series;
    sink.emit_sample = on_sample;
    sink.end_frame = on_frame;
    sink.end_session = on_end;
    return sink;
}

static quanta_engine *create(const char *query)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    quanta_engine *engine = NULL;
    char error[256];
    CHECK(plan != NULL);
    if (!plan)
    {
        return NULL;
    }
    CHECK(quanta_query_parse(query, plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error, sizeof(error)) ==
          QUANTA_OK);
    quanta_free(plan); /* The engine must own its plan copy. */
    return engine;
}

static void test_registry(void)
{
    char error[256];
    quanta_metric_desc changed = alternate_count;
    const quanta_metric_desc *local[] = {&first_count, &changed};
    quanta_catalog custom = {local, 2};
    CHECK(quanta_catalog_validate(&catalog, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_catalog_find(&catalog, "mock.count") == &first_count);
    CHECK(quanta_catalog_find(&catalog, "mock.alternate_count") == &alternate_count);
    CHECK(quanta_catalog_find(&catalog, "missing") == NULL);
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_OK);
    changed.type = QUANTA_F64;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_OK);
    changed.unit = "Hz";
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_OK);
    changed.key = first_count.key;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    changed = alternate_count;
    changed.type = (quanta_value_type)999;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    changed = alternate_count;
    changed.read = NULL;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    CHECK(quanta_catalog_validate(NULL, NULL, 0) == QUANTA_INVALID_DATA);
    custom.metrics = NULL;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
}

static void test_query(void)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    char error[256];
    const char *bad[] = {"select provider();", "select provider(\"\",mock.count);",
                         "select provider(mock.primary,mock.count);",
                         "select provider(\"mock.primary\",mock.count);",
                         "select rate(provider(\"mock.primary\",mock.count));",
                         "hal \"\"; select mock.count;",
                         "hal \"none\"; hal \"native\"; select mock.count;"};
    CHECK(plan != NULL);
    if (!plan) return;
    CHECK(quanta_query_parse("version 1; hal \"none\"; select rate(mock.alternate_count);",
                             plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(!quanta_strcmp(plan->hal, "none"));
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        CHECK(quanta_query_parse(bad[i], plan, error, sizeof(error)) != QUANTA_OK);
    CHECK(quanta_query_parse("select absent;", plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select optional absent;", plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    plan->select_count = QUANTA_MAX_SELECT + 1;
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select mock.count;", plan, error, sizeof(error)) == QUANTA_OK);
    quanta_memset(plan->selects[0].pattern, 'a', sizeof(plan->selects[0].pattern));
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select mock.count;", plan, error, sizeof(error)) == QUANTA_OK);
    quanta_memset(plan->hal, 'a', sizeof(plan->hal));
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse(NULL, plan, error, sizeof(error)) == QUANTA_INVALID_DATA);
    quanta_free(plan);
}

static void test_shared_collection(void)
{
    quanta_engine *engine;
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    quanta_memset(&control, 0, sizeof(control));
    engine = create("once; select mock.count; select mock.count as plot; select "
                    "mock.property;");
    if (!engine)
    {
        return;
    }
    sink.api_version = 999;
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_UNSUPPORTED);
    sink.api_version = 1;
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
    CHECK(out.series_count == 3 && out.sample_count == 3 && out.ended == 1);
    CHECK(out.series[0].source_id == out.series[1].source_id);
    CHECK(out.series[0].id != out.series[1].id);
    CHECK(control.opens[0] == 1 && control.opens[1] == 0 && control.reads[0] == 2);
    CHECK(quanta_engine_metric_acquisitions(engine, "mock.count") == 1);
    CHECK(out.samples[0].value.as.u64 == 101 && out.samples[2].value.as.u64 == 42);
    CHECK(out.samples[0].measured_at_ns == out.samples[2].measured_at_ns);
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_INVALID_DATA);
    quanta_engine_destroy(engine);
    CHECK(control.closes[0] == 1 && control.closes[1] == 0);
}

static void test_distinct_methods(void)
{
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    quanta_memset(&control, 0, sizeof(control));
    quanta_engine *engine = create("once; select mock.count; select "
                                   "mock.alternate_count;");
    if (!engine)
    {
        return;
    }
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
    CHECK(out.series_count == 2 && out.samples[0].value.as.u64 == 101 &&
          out.samples[1].value.as.u64 == 201);
    CHECK(out.series[0].source_id != out.series[1].source_id);
    CHECK(control.opens[0] == 1 && control.opens[1] == 1);
    quanta_engine_destroy(engine);
    CHECK(control.closes[0] == 1 && control.closes[1] == 1);
}

static void test_rates_and_once(void)
{
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    quanta_memset(&control, 0, sizeof(control));
    quanta_engine *engine =
        create("every 10ms; count 3; select rate(mock.count); select mock.property;");
    if (!engine)
    {
        return;
    }
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
    CHECK(out.frames == 3 && out.sample_count == 4);
    CHECK(out.samples[0].status == QUANTA_WARMUP);
    CHECK(out.samples[2].status == QUANTA_OK && out.samples[2].window_ns > 0 &&
          out.samples[2].value.as.f64 > 0);
    CHECK(out.samples[3].status == QUANTA_OK);
    CHECK(control.reads[0] == 4); /* 3 polls, 1 ONCE field. */
    CHECK(quanta_engine_metric_acquisitions(engine, "mock.count") == 3);
    quanta_engine_destroy(engine);
}

static void test_failures(void)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    quanta_engine *engine = NULL;
    char error[256];
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    CHECK(plan != NULL);
    if (!plan)
    {
        return;
    }
    CHECK(quanta_query_parse("once; select mock.count; select "
                             "mock.alternate_count;",
                             plan, error, sizeof(error)) == QUANTA_OK);
    quanta_memset(&control, 0, sizeof(control));
    control.open_result = QUANTA_NO_MEMORY;
    CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error, sizeof(error)) ==
              QUANTA_NO_MEMORY &&
          !engine);
    CHECK(control.opens[0] == 1 && control.closes[0] == 1 && control.opens[1] == 0 &&
          control.closes[1] == 0);
    for (int mode = 1; mode <= 3; mode++)
    {
        quanta_memset(&control, 0, sizeof(control));
        control.invalid_discovery = mode;
        CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error,
                                                sizeof(error)) == QUANTA_INVALID_DATA &&
              !engine);
        CHECK(control.closes[0] == 1 && control.closes[1] == 1);
    }
    quanta_free(plan);
    quanta_memset(&control, 0, sizeof(control));
    control.invalid_value = 1;
    engine = create("once; select mock.count;");
    if (engine)
    {
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
        CHECK(out.samples[0].status == QUANTA_INVALID_DATA);
        CHECK(quanta_engine_has_sample_errors(engine));
        quanta_engine_destroy(engine);
    }
    quanta_memset(&control, 0, sizeof(control));
    quanta_memset(&out, 0, sizeof(out));
    out.sample_failure = QUANTA_IO_ERROR;
    engine = create("once; select mock.count;");
    if (engine)
    {
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_IO_ERROR);
        CHECK(out.ended == 1 && out.end_status == QUANTA_IO_ERROR);
        quanta_engine_destroy(engine);
    }
    quanta_memset(&out, 0, sizeof(out));
    engine = create("once; select mock.count;");
    if (engine)
    {
        quanta_engine_stop(engine);
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_CANCELLED);
        CHECK(out.ended == 1 && out.sample_count == 0);
        quanta_engine_destroy(engine);
    }
    CHECK(quanta_engine_run(NULL, &sink) == QUANTA_INVALID_DATA);
    CHECK(quanta_engine_metric_acquisitions(NULL, "x") == 0);
    quanta_engine_destroy(NULL);
}

static void test_live_queue(void)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    quanta_engine *engine = NULL;
    quanta_live *live = NULL;
    char error[256];
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    CHECK(plan != NULL);
    if (!plan)
    {
        return;
    }
    quanta_memset(&control, 0, sizeof(control));
    CHECK(quanta_query_parse("every 10ms; count 3; select mock.count;", plan, error,
                             sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error, sizeof(error)) ==
          QUANTA_OK);
    if (!engine)
    {
        quanta_free(plan);
        return;
    }
    CHECK(quanta_live_start(engine, plan, &live) == QUANTA_OK);
    if (live)
    {
        uint64_t deadline = quanta_now_ns() + 5 * QUANTA_NS_S;
        while (!quanta_live_done(live) && quanta_now_ns() < deadline)
        {
            CHECK(quanta_live_pump(live, &sink) == QUANTA_OK);
            quanta_sleep_ns(QUANTA_NS_MS);
        }
        int done = quanta_live_done(live);
        CHECK(done);
        CHECK(quanta_live_finish(live, !done) == QUANTA_OK);
        CHECK(out.sample_count == 3 && out.frames == 3 && out.ended == 1);
    }
    quanta_engine_destroy(engine);
    CHECK(quanta_live_start(NULL, plan, &live) == QUANTA_INVALID_DATA && !live);
    quanta_free(plan);
}

/* Injected handles are borrowed: success, validation failure and initialization
 * failure must all leave ownership with the caller. */
static void close_injected_hal(void *data)
{
    ++*(unsigned *)data;
}

static void test_hal_ownership(void)
{
    quanta_hal_info info = {0};
    quanta_hal_operations operations = {0};
    quanta_engine_options options = {0};
    quanta_hal *hal = NULL;
    quanta_engine *engine = NULL;
    quanta_plan *plan = quanta_calloc(1, sizeof(*plan));
    unsigned closed = 0;
    char error[256];
    CHECK(plan != NULL);
    if (!plan) return;
    quanta_strcpy(info.name, "owned-fixture");
    operations.close = close_injected_hal;
    CHECK(quanta_hal_attach(&info, &operations, &closed, NULL, &hal) == QUANTA_OK);
    if (!hal) { quanta_free(plan); return; }
    options.hal = hal;
    quanta_memset(&control, 0, sizeof(control));
    CHECK(quanta_query_parse("hal \"owned-fixture\"; once; select mock.property;", plan,
                             error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create_with_options(plan, &catalog, &options, &engine,
                                            error, sizeof(error)) == QUANTA_OK);
    CHECK(control.observed_hal == hal);
    quanta_engine_destroy(engine);
    CHECK(closed == 0 && control.closes[0] == 1);
    quanta_strcpy(plan->hal, "none");
    engine = NULL;
    CHECK(quanta_engine_create_with_options(plan, &catalog, &options, &engine,
                                            error, sizeof(error)) == QUANTA_BAD_QUERY);
    CHECK(engine == NULL && closed == 0);
    plan->hal[0] = 0; /* No constraint: explicitly injected HAL wins. */
    control.open_result = QUANTA_NO_MEMORY;
    CHECK(quanta_engine_create_with_options(plan, &catalog, &options, &engine,
                                            error, sizeof(error)) == QUANTA_NO_MEMORY);
    CHECK(engine == NULL && closed == 0 && control.closes[0] == 2);
    quanta_hal_close(hal);
    CHECK(closed == 1);
    quanta_free(plan);
}

int main(void)
{
    test_registry();
    test_query();
    test_shared_collection();
    test_distinct_methods();
    test_rates_and_once();
    test_failures();
    test_live_queue();
    test_hal_ownership();
    quanta_printf("Core/catalog contracts: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
