/* SPDX-License-Identifier: MIT
 * Descriptor-only Windows catalogue check on Linux; not a WinAPI emulation.
 * This file is never linked into a production executable. */
#include "collector_internal.h"
#define STUB(name) \
quanta_status quanta_##name##_read(quanta_metric_context *c, quanta_instance *i, quanta_value *v, uint64_t *w, uint32_t *q) \
{ (void)c; (void)i; (void)v; (void)w; (void)q; return QUANTA_UNSUPPORTED; } \
quanta_status quanta_##name##_init(quanta_metric_context *c) { (void)c; return QUANTA_UNSUPPORTED; } \
quanta_status quanta_##name##_enumerate(quanta_metric_context *c, quanta_discovery_sink *s) { (void)c; (void)s; return QUANTA_UNSUPPORTED; } \
quanta_status quanta_##name##_collect(quanta_metric_context *c) { (void)c; return QUANTA_UNSUPPORTED; } \
void quanta_##name##_destroy(quanta_metric_context *c) { (void)c; }
STUB(os)
STUB(memory)
STUB(cpu)
STUB(dmi)
STUB(network)
STUB(hwmon)
STUB(cpuid)
STUB(filesystem)
STUB(process)

STUB(smbios_mem)
