/**
 * @file sensors/platform/esp8266/os.c
 *
 * Embedded OS snapshot: name/architecture, uptime and available ESP8266 SDK properties.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#include "os.h"
#include "quanta/osal.h"
#include "quanta/text.h"
#include <esp_system.h>
#include <string.h>

quanta_status quanta_native_os_refresh(quanta_os_snapshot *out, quanta_snapshot_context context)
{
    uint8_t mac[6];
    const char *version;
    (void)context;
    if (!out) return QUANTA_INVALID_DATA;
    quanta_memset(out, 0, sizeof(*out));
    quanta_copy(out->hostname, sizeof(out->hostname), "esp8266");
    if (esp_efuse_mac_get_default(mac) == ESP_OK)
        quanta_snprintf(out->hostname, sizeof(out->hostname), "esp8266-%02x%02x%02x",
                 (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
    out->hostname_status = QUANTA_OK;
    version = esp_get_idf_version();
    out->kernel_status = version && version[0] ? QUANTA_OK : QUANTA_NO_DATA;
    if (version)
    {
        if (!quanta_copy(out->kernel, sizeof(out->kernel), version)) out->kernel_status = QUANTA_LIMIT;
        quanta_copy(out->version, sizeof(out->version), version);
    }
    quanta_copy(out->name, sizeof(out->name), "ESP8266 RTOS SDK");
    quanta_copy(out->arch, sizeof(out->arch), "xtensa");
    out->online_cpus = 1;
    out->online_status = QUANTA_OK;
    out->uptime = (double)quanta_now_ns() / (double)QUANTA_NS_S;
    out->uptime_status = QUANTA_OK;
    out->load_status = QUANTA_UNSUPPORTED;
    return QUANTA_OK;
}
void quanta_native_os_cleanup(quanta_os_snapshot *snapshot) { (void)snapshot; }
