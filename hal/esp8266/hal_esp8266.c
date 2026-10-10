/**
 * @file hal/esp8266/hal_esp8266.c
 *
 * ESP8266 HAL: platform information and limited GPIO operations; not general access to arbitrary
 * registers.
 *
 * HAL: hardware primitives and information about their availability. Metrics and SMBIOS/ACPI
 * parsing are not HAL operations.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "hal_internal.h"
#include <driver/gpio.h>

static int valid_pin(uint32_t controller, uint32_t pin)
{
    /* GPIO6..11 normally carry SPI flash. Never reconfigure pins implicitly. */
    return controller == 0 && pin <= 16 && !(pin >= 6 && pin <= 11);
}
static quanta_status read_gpio(void *state, uint32_t controller, uint32_t pin, int *level)
{
    (void)state;
    if (!valid_pin(controller, pin)) return QUANTA_INVALID_DATA;
    *level = gpio_get_level((gpio_num_t)pin);
    return QUANTA_OK;
}
static quanta_status write_gpio(void *state, uint32_t controller, uint32_t pin, int level)
{
    (void)state;
    if (!valid_pin(controller, pin)) return QUANTA_INVALID_DATA;
    return gpio_set_level((gpio_num_t)pin, (uint32_t)level) == ESP_OK ? QUANTA_OK : QUANTA_IO_ERROR;
}
quanta_status quanta_hal_platform_open(const quanta_hal_options *options, quanta_hal **out)
{
    quanta_hal_info info = {0};
    quanta_hal_operations ops = {0};
    const char *name = options && options->name ? options->name : "native";
    if (*name && quanta_strcmp(name, "native") && quanta_strcmp(name, "esp8266-native")) return QUANTA_UNSUPPORTED;
    quanta_strcpy(info.name, "esp8266-native");
    info.logical_cpus = 1;
    info.known = QUANTA_HAL_CPU_COUNT_KNOWN;
    info.maximum_physical_address = UINT32_MAX;
    ops.read_gpio = read_gpio;
    ops.write_gpio = write_gpio;
    return quanta_hal_attach(&info, &ops, NULL, options, out);
}
