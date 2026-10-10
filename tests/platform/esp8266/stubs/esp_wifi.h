/**
 * @file tests/platform/esp8266/stubs/esp_wifi.h
 *
 * Minimal esp_wifi.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_ESP_WIFI_H
#define TEST_ESP_WIFI_H
#include "esp_system.h"
uint16_t esp_wifi_get_vdd33(void);
#endif
