/* SPDX-License-Identifier: MIT */
#include "osal_internal.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

uint64_t quanta_now_ns(void)
{
    int64_t microseconds;
    /* SDK 64-bit clock: no private, call-frequency-dependent tick-wrap counter.
     * Critical section prevents a torn read on the single-core 32-bit target. */
    taskENTER_CRITICAL();
    microseconds = esp_timer_get_time();
    taskEXIT_CRITICAL();
    if (microseconds < 0) return 0;
    if ((uint64_t)microseconds > UINT64_MAX / 1000u) return UINT64_MAX;
    return (uint64_t)microseconds * 1000u;
}
uint64_t quanta_realtime_ns(void) { return 0; /* No configured RTC/SNTP. */ }
void quanta_sleep_ns(uint64_t ns)
{
    while (ns)
    {
        uint64_t chunk = ns > 20 * QUANTA_NS_MS ? 20 * QUANTA_NS_MS : ns;
        TickType_t ticks = (TickType_t)((chunk * configTICK_RATE_HZ + QUANTA_NS_S - 1) / QUANTA_NS_S);
        uint64_t start = quanta_now_ns(), elapsed;
        if (!ticks) ticks = 1;
        vTaskDelay(ticks);
        elapsed = quanta_now_ns() - start;
        if (!elapsed || elapsed >= ns) break;
        ns -= elapsed;
    }
}
int quanta_install_signal_handlers(void) { return 1; }
int quanta_interrupted(void) { return 0; }
unsigned long quanta_process_id(void) { return 1; }
FILE *quanta_fopen_utf8(const char *path, const char *mode)
{ (void)path; (void)mode; errno = ENOSYS; return NULL; }
FILE *quanta_create_output(const char *path)
{ (void)path; errno = ENOSYS; return NULL; }
int quanta_path(char *out, size_t capacity, const char *root, const char *relative)
{
    int n;
    if (!out || !capacity || !root || !relative) return 0;
    n = snprintf(out, capacity, "%s/%s", root, relative);
    return n >= 0 && (size_t)n < capacity;
}
quanta_status quanta_read_text(const char *path, char *out, size_t capacity)
{
    if (!path || !out || !capacity) return QUANTA_INVALID_DATA;
    out[0] = 0;
    return QUANTA_NOT_PRESENT;
}
quanta_status quanta_read_u64(const char *path, uint64_t *out)
{
    if (!path || !out) return QUANTA_INVALID_DATA;
    *out = 0;
    return QUANTA_NOT_PRESENT;
}
