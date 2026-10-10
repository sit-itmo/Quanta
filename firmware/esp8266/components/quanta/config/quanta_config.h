/**
 * @file firmware/esp8266/components/quanta/config/quanta_config.h
 *
 * Quanta feature and capacity settings for the ESP8266 SDK component, not the desktop GUI
 * configuration.
 *
 * Catalog: an immutable set of metric descriptors with unique names. It neither stores
 * measurement results nor selects providers.
 *
 * OSAL: the minimal runtime contract for memory, files, clocks, threads and synchronization. OS
 * metrics belong to sensors, not OSAL.
 */
/* SPDX-License-Identifier: MIT -- platform selection; ABI capacities are PUBLIC CMake definitions. */
#ifndef QUANTA_CONFIG_H
#define QUANTA_CONFIG_H
#define QUANTA_ARCH_X86 0
#define QUANTA_OS_LINUX 0
#define QUANTA_OS_WINDOWS 0
#define QUANTA_PLATFORM_FREERTOS 1
#define QUANTA_WITH_MCU 1
#define QUANTA_WITH_GUI 0
#define QUANTA_WITH_TUI 0
#define QUANTA_WITH_MEMORY 0
#define QUANTA_WITH_CPU 0
#define QUANTA_WITH_DMI 0
#define QUANTA_WITH_NETWORK 0
#define QUANTA_WITH_HWMON 0
#define QUANTA_WITH_CPUID 0
#define QUANTA_WITH_FILESYSTEM 0
#define QUANTA_WITH_PROCESS 0
#define QUANTA_EMBED_DOCS 0
#define QUANTA_STRICT_STATIC 0
#endif
