/* SPDX-License-Identifier: MIT -- no invented hardware capabilities */
#include "hal_internal.h"
#include "quanta/osal.h"
quanta_status quanta_hal_platform_open(const quanta_hal_options *options, quanta_hal **out)
{
    const char *name = options && options->name ? options->name : "native";
    quanta_hal_info info = {0};
    quanta_hal_operations operations = {0};
    if (*name && quanta_strcmp(name, "native") && quanta_strcmp(name, "portable"))
        return QUANTA_UNSUPPORTED;
    quanta_memcpy(info.name, "portable", sizeof("portable"));
    return quanta_hal_attach(&info, &operations, NULL, options, out);
}
