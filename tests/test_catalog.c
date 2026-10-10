/* SPDX-License-Identifier: MIT
 * A core-only executable: no builtin sensor, HAL or renderer is linked.
 * Mock providers exercise public extension contracts without real hardware. */
#include "quanta/osal.h"
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
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct mock_state
{
    unsigned provider, refreshes;
} mock_state;

typedef struct mock_control
{
    unsigned opens[2], closes[2], reads[2];
    quanta_status open_result;
    int invalid_discovery, invalid_value;
} mock_control;

static mock_control control;
static const quanta_provider_desc primary, secondary;

static quanta_status open_mock(quanta_read_ctx *context, unsigned provider)
{
    mock_state *state = (mock_state *)calloc(1, sizeof(*state));
    control.opens[provider]++;
    if (!state)
    {
        return QUANTA_NO_MEMORY;
    }
    state->provider = provider;
    context->data = state;
    return provider ? QUANTA_OK : control.open_result;
}

static quanta_status open_primary(quanta_read_ctx *context)
{
    return open_mock(context, 0);
}

static quanta_status open_secondary(quanta_read_ctx *context)
{
    return open_mock(context, 1);
}

static void close_mock(quanta_read_ctx *context)
{
    mock_state *state = (mock_state *)context->data;
    if (state)
    {
        control.closes[state->provider]++;
        free(state);
    }
    context->data = NULL;
}

static quanta_status refresh_mock(quanta_read_ctx *context)
{
    mock_state *state = (mock_state *)context->data;
    state->refreshes++;
    return QUANTA_OK;
}

static quanta_status discover_primary(quanta_read_ctx *context, quanta_discovery_sink *sink)
{
    if (control.invalid_discovery)
    {
        quanta_instance instance;
        memset(&instance, 0, sizeof(instance));
        instance.metric = context->catalog->metrics[0];
        quanta_copy(instance.entity_key, sizeof(instance.entity_key), "fixture");
        if (control.invalid_discovery == 1)
        {
            instance.label_count = QUANTA_LABEL_CAP + 1;
        }
        if (control.invalid_discovery == 2)
        {
            instance.metric = context->catalog->metrics[2];
        }
        if (control.invalid_discovery == 3)
        {
            quanta_label_set(instance.labels, &instance.label_count, "not_in_schema", "x");
        }
        return sink->emit(sink->context, &instance);
    }
    return quanta_discover_single(context, &primary, sink);
}

static quanta_status discover_secondary(quanta_read_ctx *context, quanta_discovery_sink *sink)
{
    return quanta_discover_single(context, &secondary, sink);
}

static quanta_status read_mock(quanta_read_ctx *context, quanta_instance *instance,
                               quanta_value *value, uint64_t *window, uint32_t *quality)
{
    mock_state *state = (mock_state *)context->data;
    (void)window;
    (void)quality;
    control.reads[state->provider]++;
    if (control.invalid_value)
    {
        value->type = QUANTA_STRING;
        memset(value->as.string, 'X', sizeof(value->as.string));
        return QUANTA_OK;
    }
    if (!strcmp(instance->metric->arg, "property"))
    {
        value->as.u64 = 42;
    }
    else
    {
        value->as.u64 = (state->provider + 1) * 100u + state->refreshes;
    }
    return QUANTA_OK;
}

static const quanta_provider_desc primary = {
    0,         "mock.primary", "Unit-test provider", open_primary, discover_primary, refresh_mock,
    close_mock};
static const quanta_provider_desc secondary = {1,
                                               "mock.secondary",
                                               "Unit-test alternate",
                                               open_secondary,
                                               discover_secondary,
                                               refresh_mock,
                                               close_mock};
#define COMMON_FIELDS                                                                              \
    .title = "Fixture", .description = "Not a real system metric", .doc_path = "test fixture",     \
    .semantic_version = 1, .type = QUANTA_U64, .unit = "B", .label_schema = "",                    \
    .dependencies = "mock", .read = read_mock, .recommended_poll_ns = 10 * QUANTA_NS_MS
static const quanta_metric_desc first_count = {
    COMMON_FIELDS,          .key = "mock.count",
    .kind = QUANTA_COUNTER, .refresh = QUANTA_REFRESH_POLL,
    .provider = &primary,   .min_poll_ns = 10 * QUANTA_NS_MS,
    .arg = "count",         .counter_bits = 64};
static const quanta_metric_desc property = {COMMON_FIELDS,           .key = "mock.property",
                                            .kind = QUANTA_PROPERTY, .refresh = QUANTA_REFRESH_ONCE,
                                            .provider = &primary,    .arg = "property"};
static const quanta_metric_desc alternate_count = {
    COMMON_FIELDS,          .key = "mock.count",
    .kind = QUANTA_COUNTER, .refresh = QUANTA_REFRESH_POLL,
    .provider = &secondary, .min_poll_ns = 10 * QUANTA_NS_MS,
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
    memset(&sink, 0, sizeof(sink));
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
    quanta_plan *plan = (quanta_plan *)malloc(sizeof(*plan));
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
    free(plan); /* The engine must own its plan copy. */
    return engine;
}

static void test_registry(void)
{
    char error[256];
    quanta_metric_desc changed = alternate_count;
    const quanta_metric_desc *local[] = {&first_count, &changed};
    quanta_catalog custom = {local, 2};
    quanta_provider_desc collision = secondary;
    CHECK(quanta_catalog_validate(&catalog, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_catalog_find(&catalog, "mock.count", NULL) == &first_count);
    CHECK(quanta_catalog_find(&catalog, "mock.count", "mock.secondary") == &alternate_count);
    CHECK(quanta_catalog_find(&catalog, "missing", NULL) == NULL);
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_OK);
    changed.type = QUANTA_F64;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    changed = alternate_count;
    changed.unit = "Hz";
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    changed = alternate_count;
    changed.provider = &primary;
    CHECK(quanta_catalog_validate(&custom, error, sizeof(error)) == QUANTA_INVALID_DATA);
    changed = alternate_count;
    collision.index = 0;
    changed.provider = &collision;
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
    quanta_plan *plan = (quanta_plan *)malloc(sizeof(*plan));
    char error[256];
    const char *bad[] = {"select provider();", "select provider(\"\",mock.count);",
                         "select provider(mock.primary,mock.count);",
                         "select provider(\"mock.primary\",rate(mock.count));",
                         "select rate(provider(\"mock.primary\"));"};
    CHECK(plan != NULL);
    if (!plan)
    {
        return;
    }
    CHECK(quanta_query_parse("version 1; select rate(provider(\"mock.secondary\",mock.count));",
                             plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(!strcmp(plan->selects[0].provider, "mock.secondary"));
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        CHECK(quanta_query_parse(bad[i], plan, error, sizeof(error)) != QUANTA_OK);
    }
    CHECK(quanta_query_parse("select provider(\"absent\",mock.count);", plan, error,
                             sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) ==
          QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select optional provider(\"absent\",mock.count);", plan, error,
                             sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    plan->select_count = QUANTA_MAX_SELECT + 1;
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) ==
          QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse("select mock.count;", plan, error, sizeof(error)) == QUANTA_OK);
    memset(plan->selects[0].pattern, 'a', sizeof(plan->selects[0].pattern));
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) ==
          QUANTA_BAD_QUERY);
    CHECK(quanta_query_parse(NULL, plan, error, sizeof(error)) == QUANTA_INVALID_DATA);
    free(plan);
}

static void test_shared_collection(void)
{
    quanta_engine *engine;
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    memset(&control, 0, sizeof(control));
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
    CHECK(quanta_engine_provider_refreshes(engine, "mock.primary") == 1);
    CHECK(out.samples[0].value.as.u64 == 101 && out.samples[2].value.as.u64 == 42);
    CHECK(out.samples[0].measured_at_ns == out.samples[2].measured_at_ns);
    CHECK(quanta_engine_run(engine, &sink) == QUANTA_INVALID_DATA);
    quanta_engine_destroy(engine);
    CHECK(control.closes[0] == 1 && control.closes[1] == 0);
}

static void test_explicit_provider(void)
{
    capture out = {0};
    quanta_sink sink = make_sink(&out);
    memset(&control, 0, sizeof(control));
    quanta_engine *engine = create("once; select mock.count; select "
                                   "provider(\"mock.secondary\", mock.count);");
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
    memset(&control, 0, sizeof(control));
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
    CHECK(quanta_engine_provider_refreshes(engine, "mock.primary") == 3);
    quanta_engine_destroy(engine);
}

static void test_failures(void)
{
    quanta_plan *plan = (quanta_plan *)malloc(sizeof(*plan));
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
                             "provider(\"mock.secondary\",mock.count);",
                             plan, error, sizeof(error)) == QUANTA_OK);
    memset(&control, 0, sizeof(control));
    control.open_result = QUANTA_NO_MEMORY;
    CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error, sizeof(error)) ==
              QUANTA_NO_MEMORY &&
          !engine);
    CHECK(control.opens[0] == 1 && control.closes[0] == 1 && control.opens[1] == 0 &&
          control.closes[1] == 0);
    for (int mode = 1; mode <= 3; mode++)
    {
        memset(&control, 0, sizeof(control));
        control.invalid_discovery = mode;
        CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error,
                                                sizeof(error)) == QUANTA_INVALID_DATA &&
              !engine);
        CHECK(control.closes[0] == 1 && control.closes[1] == 1);
    }
    free(plan);
    memset(&control, 0, sizeof(control));
    control.invalid_value = 1;
    engine = create("once; select mock.count;");
    if (engine)
    {
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
        CHECK(out.samples[0].status == QUANTA_INVALID_DATA);
        CHECK(quanta_engine_has_sample_errors(engine));
        quanta_engine_destroy(engine);
    }
    memset(&control, 0, sizeof(control));
    memset(&out, 0, sizeof(out));
    out.sample_failure = QUANTA_IO_ERROR;
    engine = create("once; select mock.count;");
    if (engine)
    {
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_IO_ERROR);
        CHECK(out.ended == 1 && out.end_status == QUANTA_IO_ERROR);
        quanta_engine_destroy(engine);
    }
    memset(&out, 0, sizeof(out));
    engine = create("once; select mock.count;");
    if (engine)
    {
        quanta_engine_stop(engine);
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_CANCELLED);
        CHECK(out.ended == 1 && out.sample_count == 0);
        quanta_engine_destroy(engine);
    }
    CHECK(quanta_engine_run(NULL, &sink) == QUANTA_INVALID_DATA);
    CHECK(quanta_engine_provider_refreshes(NULL, "x") == 0);
    quanta_engine_destroy(NULL);
}

static void test_live_queue(void)
{
    quanta_plan *plan = (quanta_plan *)malloc(sizeof(*plan));
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
    memset(&control, 0, sizeof(control));
    CHECK(quanta_query_parse("every 10ms; count 3; select mock.count;", plan, error,
                             sizeof(error)) == QUANTA_OK);
    CHECK(quanta_plan_validate_with_catalog(plan, &catalog, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create_with_catalog(plan, NULL, &catalog, &engine, error, sizeof(error)) ==
          QUANTA_OK);
    if (!engine)
    {
        free(plan);
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
    free(plan);
}

int main(void)
{
    test_registry();
    test_query();
    test_shared_collection();
    test_explicit_provider();
    test_rates_and_once();
    test_failures();
    test_live_queue();
    printf("Core/catalog contracts: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
