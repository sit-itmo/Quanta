/**
 * @file tests/platform/esp8266/stubs/sdkconfig.h
 *
 * Minimal sdkconfig.h SDK test stub; declares only functions/constants used by host tests.
 *
 * A mock/stub test validates the program contract but does not replace execution on real
 * hardware or the target OS.
 */
#ifndef TEST_SDKCONFIG_H
#define TEST_SDKCONFIG_H
#ifndef CONFIG_ESP_PHY_INIT_DATA_VDD33_CONST
#define CONFIG_ESP_PHY_INIT_DATA_VDD33_CONST 255
#endif
#ifndef CONFIG_QUANTA_MEASURE_VDD33
#define CONFIG_QUANTA_MEASURE_VDD33 1
#endif
#endif
