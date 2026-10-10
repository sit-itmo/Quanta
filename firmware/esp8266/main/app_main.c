/* SPDX-License-Identifier: MIT */
#include "quanta/sensors.h"
#include "quanta/osal.h"
#include "quanta/hal.h"
#include "serial_format.h"
#include <stdio.h>

/* Low-memory synchronous path: no desktop renderer, live queue or large plan.
 * All nine original MCU fields are kept, not only a subset of smoke metrics. */
static const char *const keys[] = {
    "mcu.heap.free_bytes", "mcu.heap.min_free_bytes", "mcu.cpu.freq_mhz", "mcu.power.vdd_mv",
    "mcu.cores", "mcu.revision", "mcu.chip_id", "mcu.flash_embedded", "mcu.sdk_version",
    "os.uptime", "os.hostname", "os.name"
};
void app_main(void)
{
    quanta_hal *hal = NULL;
    quanta_hal_info info;
    char error[160];
    size_t count = 0;
    quanta_registry(&count);
    if (quanta_registry_validate(error, sizeof(error)) != QUANTA_OK)
    { printf("Quanta catalog error: %s\n", error); return; }
    if (quanta_hal_open(NULL, &hal) != QUANTA_OK)
    { puts("Quanta HAL initialization failed"); return; }
    quanta_hal_get_info(hal, &info);
    printf("Quanta %s | ESP8266 | %u metrics | HAL %s | writes disabled\n",
           QUANTA_VERSION, (unsigned)count, info.name);
    for (;;)
    {
        size_t i;
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        {
            quanta_sample sample;
            char text[QUANTA_TEXT_CAP];
            const quanta_metric_desc *metric = quanta_metric_find(keys[i]);
            quanta_status status = quanta_metric_read_once(metric, hal, NULL, &sample);
            if (status == QUANTA_OK && quanta_serial_value(text, sizeof(text), &sample.value))
                printf("%s = %s %s\n", keys[i], text, metric->unit);
            else printf("%s = [%s]\n", keys[i], quanta_status_name(status == QUANTA_OK ? QUANTA_INVALID_DATA : status));
        }
        quanta_sleep_ns(2 * QUANTA_NS_S);
    }
}
