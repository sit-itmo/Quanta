/**
 * @file tests/unit/test_metric_def.c
 *
 * Regression tests for common .def helpers and equivalence of the generated descriptors.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
/* SPDX-License-Identifier: MIT -- the helpers generate ordinary C descriptors. */
#include "quanta/osal.h"
#include "quanta/metric_def.h"
#include "quanta/quanta.h"

static unsigned checks, failures;
#define CHECK(test) do { ++checks; if (!(test)) { ++failures; \
    quanta_fprintf(quanta_stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); } } while (0)

static quanta_status fixture_read(quanta_metric_context *ctx, quanta_instance *instance,
    quanta_value *value, uint64_t *window, uint32_t *quality)
{
    (void)ctx; (void)instance;
    value->type = QUANTA_U64;
    value->as.u64 = 42;
    *window = 0;
    *quality = 0;
    return QUANTA_OK;
}
static quanta_status fixture_init(quanta_metric_context *ctx) { (void)ctx; return QUANTA_OK; }
static quanta_status fixture_enumerate(quanta_metric_context *ctx, quanta_discovery_sink *sink)
{ return quanta_discover_single(ctx, sink); }
static quanta_status fixture_collect(quanta_metric_context *ctx) { (void)ctx; return QUANTA_OK; }
static void fixture_destroy(quanta_metric_context *ctx) { (void)ctx; }

static const quanta_metric_desc bytes = {
    QUANTA_META("fixture.bytes", "Bytes"), QUANTA_GAUGE_BYTES(250),
    QUANTA_SNAPSHOT(fixture, "bytes"), QUANTA_SOURCE("", "fixture")};
static const quanta_metric_desc counter = {
    QUANTA_META("fixture.total", "Counter"), QUANTA_COUNTER_BYTES(100),
    QUANTA_SNAPSHOT(fixture, "counter"), QUANTA_SOURCE("device", "fixture"),
    .flags = QUANTA_SENSITIVE};
static const quanta_metric_desc ratio = {
    QUANTA_META_DESC("fixture.ratio", "Ratio", "Detailed description"), QUANTA_GAUGE_RATIO(1000),
    QUANTA_SNAPSHOT(fixture, "ratio"), QUANTA_SOURCE("", "")};
static const quanta_metric_desc property = {
    QUANTA_META_VERSION("fixture.property", "Property", "Versioned description", 2),
    QUANTA_PROPERTY_U64("count", 0), QUANTA_INIT_SNAPSHOT(fixture, "property"),
    QUANTA_SOURCE("", "")};
static const quanta_metric_desc direct = {
    QUANTA_META("fixture.direct", "Direct"), QUANTA_PROPERTY_U64("", 5000),
    QUANTA_SINGLE_READ(fixture_read, "direct"), QUANTA_SOURCE("", "")};
static const quanta_metric_desc event = {
    QUANTA_META("fixture.event", "Event with polling fallback"),
    QUANTA_VALUE(QUANTA_U64, QUANTA_GAUGE, QUANTA_REFRESH_ON_EVENT, "B"),
    QUANTA_TIMING_MS(100, 250, 1000), QUANTA_SNAPSHOT(fixture, "event"), QUANTA_SOURCE("", "")};

int main(void)
{
    const quanta_metric_desc *const entries[] = {&bytes, &counter, &ratio, &property, &direct, &event};
    const quanta_catalog catalog = {entries, sizeof(entries) / sizeof(entries[0])};
    char error[256];
    quanta_sample sample;
    CHECK(quanta_catalog_validate(&catalog, error, sizeof(error)) == QUANTA_OK);
    CHECK(!quanta_strcmp(bytes.doc_path, "docs/metrics/fixture.bytes.md"));
    CHECK(!quanta_strcmp(bytes.description, bytes.title));
    CHECK(bytes.type == QUANTA_U64 && bytes.kind == QUANTA_GAUGE && !quanta_strcmp(bytes.unit, "B"));
    CHECK(bytes.min_poll_ns == 250 * QUANTA_NS_MS);
    CHECK(bytes.recommended_poll_ns == QUANTA_NS_S && bytes.stale_after_ns == 5 * QUANTA_NS_S);
    CHECK(bytes.init == fixture_init && bytes.collect == fixture_collect && bytes.read == fixture_read);
    CHECK(counter.kind == QUANTA_COUNTER && counter.counter_bits == 64);
    CHECK(counter.flags == QUANTA_SENSITIVE && !quanta_strcmp(counter.label_schema, "device"));
    CHECK(ratio.type == QUANTA_F64 && !quanta_strcmp(ratio.unit, "1"));
    CHECK(!quanta_strcmp(ratio.description, "Detailed description"));
    CHECK(property.semantic_version == 2 && property.collect == NULL && property.init == fixture_init);
    CHECK(property.refresh == QUANTA_REFRESH_ONCE && property.stale_after_ns == 0);
    CHECK(direct.init == NULL && direct.destroy == NULL && direct.enumerate == quanta_discover_single);
    CHECK(event.refresh == QUANTA_REFRESH_ON_EVENT && event.recommended_poll_ns == 250 * QUANTA_NS_MS);
    CHECK(quanta_metric_read_once(&direct, NULL, NULL, &sample) == QUANTA_OK);
    CHECK(sample.status == QUANTA_OK && sample.value.type == QUANTA_U64 && sample.value.as.u64 == 42);
    quanta_printf("Metric definition helpers: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
