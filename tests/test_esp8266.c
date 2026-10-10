/* SPDX-License-Identifier: MIT
 * Executes the REAL Quanta ESP/FreeRTOS source files against SDK seam fixtures.
 * No emulated sensor result can be mistaken for a measurement on real ESP8266. */
#include "quanta/sensors.h"
#include "quanta/osal.h"
#include "quanta/hal.h"
#include "serial_format.h"
#include "thread.h"
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <sdkconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

static unsigned checks, failures, voltage_reads, gpio_writes, delayed_ticks;
static int mac_failed, gpio_level, gpio_failure;
static uint16_t raw_voltage = 3379;
static int64_t clock_us = 12345000;
static const char *sdk_version = "SDK-contract-fixture";
static pthread_mutex_t sdk_lock = PTHREAD_MUTEX_INITIALIZER;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, #x); } } while (0)
uint32_t esp_get_free_heap_size(void) { return 40000; }
uint32_t esp_get_minimum_free_heap_size(void) { return 35000; }
void esp_chip_info(esp_chip_info_t *info)
{ memset(info, 0, sizeof(*info)); info->cores=1; info->revision=3; info->features=CHIP_FEATURE_EMB_FLASH; }
esp_err_t esp_efuse_mac_get_default(uint8_t *mac)
{ const uint8_t bytes[] = {0x18, 0xfe, 0x34, 0xab, 0xcd, 0xef}; memcpy(mac, bytes, 6); return mac_failed ? ESP_FAIL : ESP_OK; }
const char *esp_get_idf_version(void) { return sdk_version; }
uint16_t esp_wifi_get_vdd33(void) { ++voltage_reads; return raw_voltage; }
int64_t esp_timer_get_time(void) { return clock_us; }
void sdk_enter_critical(void) { pthread_mutex_lock(&sdk_lock); }
void sdk_exit_critical(void) { pthread_mutex_unlock(&sdk_lock); }
void vTaskDelay(TickType_t ticks)
{ CHECK(ticks > 0); delayed_ticks += ticks; pthread_mutex_lock(&sdk_lock); clock_us += (int64_t)ticks * 1000000 / configTICK_RATE_HZ; pthread_mutex_unlock(&sdk_lock); }
int gpio_get_level(int pin) { (void)pin; return gpio_level; }
esp_err_t gpio_set_level(int pin, uint32_t level)
{ (void)pin; ++gpio_writes; if (gpio_failure) return ESP_FAIL; gpio_level=(int)level; return ESP_OK; }

static quanta_sample read_metric(const char *key, quanta_status expected)
{
    quanta_sample sample;
    const quanta_metric_desc *metric = quanta_metric_find(key);
    CHECK(metric != NULL);
    CHECK(quanta_metric_read_once(metric, NULL, NULL, &sample) == expected);
    CHECK(sample.status == expected);
    return sample;
}
static void test_metrics(void)
{
    size_t count;
    char error[256];
    quanta_sample sample;
    quanta_registry(&count);
    CHECK(count == 18); /* 9 OS + all 9 MCU descriptors. */
    CHECK(quanta_registry_validate(error, sizeof(error)) == QUANTA_OK);
    CHECK(read_metric("mcu.heap.free_bytes", QUANTA_OK).value.as.u64 == 40000);
    CHECK(read_metric("mcu.heap.min_free_bytes", QUANTA_OK).value.as.u64 == 35000);
    sample = read_metric("mcu.cpu.freq_mhz", QUANTA_OK);
    CHECK(sample.value.as.u64 == 160 && (sample.quality_flags & QUANTA_QUALITY_ESTIMATED));
    CHECK(read_metric("mcu.cores", QUANTA_OK).value.as.u64 == 1);
    CHECK(read_metric("mcu.revision", QUANTA_OK).value.as.u64 == 3);
    CHECK(read_metric("mcu.flash_embedded", QUANTA_OK).value.as.boolean == 1);
    CHECK(read_metric("mcu.chip_id", QUANTA_OK).value.as.u64 == 0xabcdef);
    sample = read_metric("mcu.sdk_version", QUANTA_OK);
    CHECK(!strcmp(sample.value.as.string, sdk_version));
#if CONFIG_QUANTA_MEASURE_VDD33
    sample = read_metric("mcu.power.vdd_mv", QUANTA_OK);
    CHECK(sample.value.as.u64 == 3300);
    raw_voltage = 65535;
    read_metric("mcu.power.vdd_mv", QUANTA_NO_DATA);
    raw_voltage = 0;
    read_metric("mcu.power.vdd_mv", QUANTA_NO_DATA);
    raw_voltage = 3379;
    CHECK(voltage_reads > 0);
#else
    read_metric("mcu.power.vdd_mv", QUANTA_UNSUPPORTED);
    CHECK(voltage_reads == 0);
#endif
    mac_failed = 1;
    read_metric("mcu.chip_id", QUANTA_NO_DATA);
    CHECK(read_metric("mcu.heap.free_bytes", QUANTA_OK).value.as.u64 == 40000);
    sample = read_metric("os.hostname", QUANTA_OK);
    CHECK(!strcmp(sample.value.as.string, "esp8266"));
    mac_failed = 0;
    sample = read_metric("os.hostname", QUANTA_OK);
    CHECK(!strcmp(sample.value.as.string, "esp8266-abcdef"));
    sample = read_metric("os.arch", QUANTA_OK);
    CHECK(!strcmp(sample.value.as.string, "xtensa"));
    CHECK(read_metric("os.cpu.online", QUANTA_OK).value.as.u64 == 1);
    CHECK(fabs(read_metric("os.uptime", QUANTA_OK).value.as.f64 - 12.345) < 0.0001);
    read_metric("os.load.1m", QUANTA_UNSUPPORTED);
    sdk_version = NULL;
    read_metric("mcu.sdk_version", QUANTA_NO_DATA);
    sdk_version = "SDK-contract-fixture";
}
static quanta_status sink_sample(void *state, const quanta_sample *sample)
{
    unsigned *count = state;
    ++*count;
    CHECK(sample->status == QUANTA_OK);
    return QUANTA_OK;
}
static void test_engine(void)
{
    quanta_plan *plan = malloc(sizeof(*plan));
    quanta_engine *engine = NULL;
    quanta_sink sink = {0};
    unsigned count = 0;
    char error[256];
    CHECK(plan != NULL);
    if (!plan) return;
    CHECK(quanta_query_parse("once; hal \"none\"; select mcu.heap.free_bytes; select mcu.cpu.freq_mhz; select os.name;",
                            plan, error, sizeof(error)) == QUANTA_OK);
    CHECK(quanta_engine_create(plan, NULL, &engine, error, sizeof(error)) == QUANTA_OK);
    free(plan);
    sink.struct_size=sizeof(sink); sink.api_version=1; sink.context=&count; sink.emit_sample=sink_sample;
    if (engine)
    {
        CHECK(quanta_engine_run(engine, &sink) == QUANTA_OK);
        CHECK(count == 3);
        CHECK(quanta_engine_metric_acquisitions(engine, "mcu.heap.free_bytes") == 1);
        CHECK(quanta_engine_metric_acquisitions(engine, "mcu.cpu.freq_mhz") == 1);
        quanta_engine_destroy(engine);
    }
}
static void test_hal(void)
{
    quanta_hal *hal = NULL;
    quanta_hal_info info;
    quanta_hal_options options = {0};
    uint64_t reg;
    int level;
    char buffer[4];
    CHECK(quanta_hal_open(NULL, &hal) == QUANTA_OK);
    CHECK(quanta_hal_get_info(hal, &info) == QUANTA_OK);
    CHECK(!strcmp(info.name, "esp8266-native"));
    CHECK(info.capabilities == (QUANTA_HAL_READ_GPIO | QUANTA_HAL_WRITE_GPIO));
    CHECK(info.logical_cpus == 1 && !(info.known & QUANTA_HAL_MEMORY_SIZE_KNOWN));
    CHECK(quanta_hal_read_mem(hal, 0, buffer, sizeof(buffer)) == QUANTA_UNSUPPORTED);
    CHECK(quanta_hal_read_reg(hal, QUANTA_REGISTER_CONTROL, 0, 0, &reg) == QUANTA_UNSUPPORTED);
    CHECK(quanta_hal_write_gpio(hal, 0, 5, 1) == QUANTA_PERMISSION_DENIED && gpio_writes == 0);
    CHECK(quanta_hal_read_gpio(hal, 0, 5, &level) == QUANTA_OK);
    CHECK(quanta_hal_read_gpio(hal, 0, 6, &level) == QUANTA_INVALID_DATA);
    CHECK(quanta_hal_read_gpio(hal, 1, 5, &level) == QUANTA_INVALID_DATA);
    quanta_hal_close(hal);
    options.flags = QUANTA_HAL_ALLOW_WRITE;
    CHECK(quanta_hal_open(&options, &hal) == QUANTA_OK);
    CHECK(quanta_hal_write_gpio(hal, 0, 5, 1) == QUANTA_OK);
    CHECK(quanta_hal_read_gpio(hal, 0, 5, &level) == QUANTA_OK && level == 1);
    gpio_failure = 1;
    CHECK(quanta_hal_write_gpio(hal, 0, 5, 0) == QUANTA_IO_ERROR);
    quanta_hal_close(hal);
}
static void test_format(void)
{
    char text[128];
    quanta_value value = {0};
    value.type=QUANTA_U64; value.as.u64=UINT64_MAX;
    CHECK(quanta_serial_value(text, sizeof(text), &value) && !strcmp(text,"18446744073709551615"));
    CHECK(!quanta_serial_value(text, 5, &value));
    value.type=QUANTA_S64; value.as.s64=INT64_MIN;
    CHECK(quanta_serial_value(text, sizeof(text), &value) && !strcmp(text,"-9223372036854775808"));
    value.type=QUANTA_F64; value.as.f64=12.345;
    CHECK(quanta_serial_value(text, sizeof(text), &value) && !strcmp(text,"12.345000"));
    value.as.f64=-0.012;
    CHECK(quanta_serial_value(text, sizeof(text), &value) && !strcmp(text,"-0.012000"));
    value.as.f64=NAN; CHECK(!quanta_serial_value(text,sizeof(text),&value));
    value.type=QUANTA_STRING; memset(value.as.string, 'x', sizeof(value.as.string));
    CHECK(!quanta_serial_value(text,sizeof(text),&value));
}
static void test_osal(void)
{
    char text[32];
    uint64_t value = 999, previous;
    clock_us = INT64_C(4294967296) * 10000 + 123;
    CHECK(quanta_now_ns() == (uint64_t)clock_us * 1000);
    previous = quanta_now_ns();
    quanta_sleep_ns(1);
    CHECK(quanta_now_ns() > previous && delayed_ticks > 0);
    quanta_sleep_ns(100 * QUANTA_NS_MS);
    CHECK(quanta_realtime_ns() == 0);
    CHECK(quanta_read_text("x", text, sizeof(text)) == QUANTA_NOT_PRESENT && text[0] == 0);
    CHECK(quanta_read_u64("x", &value) == QUANTA_NOT_PRESENT && value == 0);
    CHECK(quanta_fopen_utf8("x", "w") == NULL && quanta_create_output("x") == NULL);
}
int main(void)
{
    test_metrics(); test_engine(); test_hal(); test_format(); test_osal();
    printf("ESP8266 SDK seam (NOT hardware): %u checks, %u failures; VDD enabled=%d\n",
           checks, failures, CONFIG_QUANTA_MEASURE_VDD33);
    return failures ? 1 : 0;
}
