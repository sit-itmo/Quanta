/* SPDX-License-Identifier: MIT
 * ONLY for the schema-validation test executable on non-Windows hosts.
 * These functions are NEVER linked into quanta. They do not simulate WinAPI or
 * return invented hardware values; only descriptor addresses are needed.
 */
#include "collector_internal.h"
#include "frontend_internal.h"
#define STUB(name, index_)                                                                         \
    quanta_status quanta_##name##_read(quanta_read_ctx *c, quanta_instance *i, quanta_value *v,    \
                                       uint64_t *w, uint32_t *q)                                   \
    {                                                                                              \
        (void)c;                                                                                   \
        (void)i;                                                                                   \
        (void)v;                                                                                   \
        (void)w;                                                                                   \
        (void)q;                                                                                   \
        return QUANTA_UNSUPPORTED;                                                                 \
    }                                                                                              \
    const quanta_provider_desc quanta_##name##_provider = {                                        \
        index_, "schema_only." #name, "Descriptor-only validation", NULL, NULL, NULL, NULL};
STUB(os, 0)
STUB(memory, 1)
STUB(cpu, 2)
STUB(dmi, 3) STUB(network, 4) STUB(hwmon, 5) STUB(cpuid, 6) STUB(filesystem, 7) STUB(process, 8)
