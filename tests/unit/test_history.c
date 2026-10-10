/* SPDX-License-Identifier: MIT -- bounded history and value tests; no
 * sensor/HAL link. */
#include "quanta/osal.h"
#include "quanta/quanta.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, failures;
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        ++checks;                                                                                  \
        if (!(x))                                                                                  \
        {                                                                                          \
            ++failures;                                                                            \
            quanta_fprintf(quanta_stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
        }                                                                                          \
    } while (0)
static const quanta_metric_desc metric = {.key = "fixture.gauge", .type = QUANTA_F64};

static void values(void)
{
    quanta_value value = {0};
    double number;
    uint64_t window;
    char *text = value.as.string;
    CHECK(quanta_value_validate(NULL) == QUANTA_INVALID_DATA);
    value.type = QUANTA_STRING;
    quanta_memset(text, 'x', sizeof(value.as.string));
    CHECK(quanta_value_validate(&value) == QUANTA_INVALID_DATA);
    text[sizeof(value.as.string) - 1] = 0;
    CHECK(quanta_value_validate(&value) == QUANTA_OK);
    quanta_memset(&value, 0, sizeof(value));
    value.type = QUANTA_BYTES;
    value.as.bytes.size = 257;
    CHECK(quanta_value_validate(&value) == QUANTA_INVALID_DATA);
    value.as.bytes.size = 256;
    CHECK(quanta_value_validate(&value) == QUANTA_OK);
    value.type = QUANTA_BOOL;
    value.as.boolean = 2;
    CHECK(quanta_value_validate(&value) == QUANTA_INVALID_DATA);
    CHECK(!quanta_value_number(&value, &number));
    value.type = QUANTA_F64;
    value.as.f64 = NAN;
    CHECK(quanta_value_validate(&value) == QUANTA_INVALID_DATA);
    value.as.f64 = INFINITY;
    CHECK(!quanta_value_number(&value, &number));
    CHECK(!quanta_value_number(NULL, &number));
    CHECK(!quanta_unit_scale(NULL, "Hz", &number));
    CHECK(quanta_unit_scale("Hz", "MHz", &number) && quanta_fabs(number - 1e-6) < 1e-12);
    CHECK(quanta_counter_derive(QUANTA_RATE, NULL, NULL, &value, &window) == QUANTA_INVALID_DATA);
}

static void history(void)
{
    quanta_plan *plan = (quanta_plan *)quanta_malloc(sizeof(*plan));
    quanta_model model, clone = {0};
    quanta_sink sink;
    quanta_series series = {0};
    quanta_sample sample = {0};
    CHECK(plan != NULL);
    if (!plan)
    {
        return;
    }
    quanta_plan_init(plan);
    plan->history_ns = 3 * QUANTA_NS_S;
    quanta_model_init(&model, plan);
    quanta_free(plan);
    sink = quanta_model_sink(&model);
    series.id = 7;
    series.metric = &metric;
    series.type = QUANTA_F64;
    series.effective_period_ns = QUANTA_NS_S;
    series.view.widget = QUANTA_WIDGET_PLOT;
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_OK);
    sample.series_id = series.id;
    sample.status = QUANTA_OK;
    sample.value.type = QUANTA_F64;
    for (unsigned i = 1; i <= 12; i++)
    {
        sample.sequence = i;
        sample.measured_at_ns = (uint64_t)i * QUANTA_NS_S;
        sample.value.as.f64 = (double)i;
        CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    }
    quanta_item *item = quanta_model_find(&model, 7);
    CHECK(item != NULL);
    if (!item)
    {
        quanta_model_destroy(&model);
        return;
    }
    CHECK(item->history_cap == 5 && item->history_count == 4);
    CHECK(quanta_history_at(item, 0)->value == 9 && quanta_history_at(item, 3)->value == 12);
    CHECK(quanta_history_at(item, 4) == NULL && quanta_history_at(NULL, 0) == NULL);
    sample.sequence++;
    sample.measured_at_ns = QUANTA_NS_S;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_INVALID_DATA);
    CHECK(item->last.sequence == 12);
    sample.measured_at_ns = 13 * QUANTA_NS_S;
    sample.value.as.f64 = NAN;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_INVALID_DATA);
    CHECK(sink.emit_sample(sink.context, NULL) == QUANTA_INVALID_DATA);
    sample.status = QUANTA_IO_ERROR;
    CHECK(sink.emit_sample(sink.context, &sample) == QUANTA_OK);
    CHECK(item->good.sequence == 12 && item->last.sequence == 13);
    CHECK(quanta_model_clone(&clone, &model) == QUANTA_OK);
    CHECK(clone.items[0].history != item->history &&
          clone.items[0].history_count == item->history_count);
    series.effective_period_ns = 2 * QUANTA_NS_S;
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_OK);
    CHECK(item->history_cap == 3 && item->history_count == 3);
    CHECK(quanta_history_at(item, 0)->value == 11);
    series.view.widget = QUANTA_WIDGET_VALUE;
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_OK);
    CHECK(item->history == NULL && model.history_bytes == 0);
    series.view.widget = QUANTA_WIDGET_PLOT;
    model.history_bytes = QUANTA_HISTORY_BUDGET;
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_LIMIT);
    CHECK(item->history == NULL && item->meta.view.widget == QUANTA_WIDGET_VALUE);
    model.history_bytes = 0;
    CHECK(sink.upsert_series(sink.context, &series) == QUANTA_OK);
    CHECK(sink.remove_series(sink.context, 7) == QUANTA_OK && model.history_bytes == 0);
    CHECK(quanta_model_find(&model, 7) == NULL);
    CHECK(quanta_model_clone(&clone, &clone) == QUANTA_OK);
    quanta_model_destroy(&clone);
    quanta_model_destroy(&model);
}

int main(void)
{
    values();
    history();
    quanta_printf("History/value boundaries: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
