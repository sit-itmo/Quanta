/**
 * @file tests/platform/esp8266/stubs/freertos/task.h
 *
 * Minimal task.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_FREERTOS_TASK_H
#define TEST_FREERTOS_TASK_H
#include "FreeRTOS.h"
void vTaskDelay(TickType_t);
#endif
