/**
 * @file tests/platform/esp8266/stubs/driver/gpio.h
 *
 * Minimal gpio.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_ESP_GPIO_H
#define TEST_ESP_GPIO_H
#include "esp_system.h"
typedef int gpio_num_t;
int gpio_get_level(gpio_num_t);
esp_err_t gpio_set_level(gpio_num_t, uint32_t);
#endif
