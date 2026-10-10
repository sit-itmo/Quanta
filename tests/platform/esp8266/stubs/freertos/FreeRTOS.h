/**
 * @file tests/platform/esp8266/stubs/freertos/FreeRTOS.h
 *
 * Minimal FreeRTOS.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
#define configCPU_CLOCK_HZ 160000000u
#define configTICK_RATE_HZ 100u
void sdk_enter_critical(void);
void sdk_exit_critical(void);
#define taskENTER_CRITICAL() sdk_enter_critical()
#define taskEXIT_CRITICAL() sdk_exit_critical()
#endif
