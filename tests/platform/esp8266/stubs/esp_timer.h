/**
 * @file tests/platform/esp8266/stubs/esp_timer.h
 *
 * Minimal esp_timer.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_ESP_TIMER_H
#define TEST_ESP_TIMER_H
#include <stdint.h>
int64_t esp_timer_get_time(void);
#endif
