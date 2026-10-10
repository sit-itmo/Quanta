/* SPDX-License-Identifier: MIT
 * SDK metrics deliberately bypass HAL. No high-level MCU API belongs in HAL. */
#include "quanta/osal.h"
#include "mcu.h"
#include "quanta/text.h"
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <sdkconfig.h>
#include <string.h>

#if CONFIG_QUANTA_MEASURE_VDD33
#if !defined(CONFIG_ESP_PHY_INIT_DATA_VDD33_CONST) || CONFIG_ESP_PHY_INIT_DATA_VDD33_CONST != 255
#error "Quanta VDD33 requires PHY vdd33_const=255 and a board with unconnected TOUT"
#endif
#endif

quanta_status quanta_native_mcu_refresh(quanta_mcu_snapshot *out)
{
    esp_chip_info_t chip;
    uint8_t mac[6];
    const char *version;
    if (!out) return QUANTA_INVALID_DATA;
    quanta_memset(out, 0, sizeof(*out));
    quanta_memset(&chip, 0, sizeof(chip));
    out->heap_free = esp_get_free_heap_size();
    out->heap_min_free = esp_get_minimum_free_heap_size();
    out->freq_mhz = (uint64_t)configCPU_CLOCK_HZ / 1000000u;
    esp_chip_info(&chip);
    out->cores = chip.cores;
    out->revision = chip.revision;
    out->embedded_flash = !!(chip.features & CHIP_FEATURE_EMB_FLASH);
    /* Stable factory identity, available without initializing the Wi-Fi stack. */
    out->chip_id_status = QUANTA_NO_DATA;
    if (esp_efuse_mac_get_default(mac) == ESP_OK)
    {
        out->chip_id = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
        out->chip_id_status = QUANTA_OK;
    }
    version = esp_get_idf_version();
    out->sdk_status = version && version[0] ? QUANTA_OK : QUANTA_NO_DATA;
    if (version && !quanta_copy(out->sdk_version, sizeof(out->sdk_version), version))
        out->sdk_status = QUANTA_LIMIT;
    out->vdd_status = QUANTA_UNSUPPORTED;
#if CONFIG_QUANTA_MEASURE_VDD33
    /* Only valid with TOUT unconnected and PHY vdd33_const=255. The SDK returns
     * 1/1024 V, not millivolts. Invalid sentinels must never become measurements. */
    {
        uint32_t raw = esp_wifi_get_vdd33();
        out->vdd_status = raw && raw != 65535u ? QUANTA_OK : QUANTA_NO_DATA;
        if (out->vdd_status == QUANTA_OK) out->vdd_mv = ((uint64_t)raw * 1000u + 512u) / 1024u;
    }
#endif
    return QUANTA_OK;
}
