#ifndef TEST_ESP_GPIO_H
#define TEST_ESP_GPIO_H
#include "esp_system.h"
typedef int gpio_num_t;
int gpio_get_level(gpio_num_t);
esp_err_t gpio_set_level(gpio_num_t, uint32_t);
#endif
