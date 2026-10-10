/* SPDX-License-Identifier: MIT -- test-only SDK seam, never firmware headers. */
#ifndef TEST_ESP_SYSTEM_H
#define TEST_ESP_SYSTEM_H
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define CHIP_FEATURE_EMB_FLASH (1u << 0)
typedef struct { int model; uint32_t features; uint8_t cores, revision; } esp_chip_info_t;
uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);
void esp_chip_info(esp_chip_info_t *);
esp_err_t esp_efuse_mac_get_default(uint8_t *);
const char *esp_get_idf_version(void);
#endif
