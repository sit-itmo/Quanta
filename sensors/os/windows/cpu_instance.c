/**
 * @file sensors/os/windows/cpu_instance.c
 *
 * Parse PDH CPU instance names into group/number pairs; do not treat the aggregate _Total as an
 * individual core.
 *
 * Series: a particular metric instance with labels and an identity. Replacing a device may start
 * a new series even when its name is unchanged.
 *
 * Labels: key/value pairs selecting an instance, such as cpu="0". They describe the source, not
 * a widget caption.
 */
/* SPDX-License-Identifier: MIT */
#include "quanta/osal.h"
#include "windows_cpu.h"
#include <limits.h>
static int number(const char **s, unsigned *v)
{
    unsigned n = 0;
    if (**s < '0' || **s > '9')
    {
        return 0;
    }
    do
    {
        unsigned digit = (unsigned)(**s - '0');
        if (n > (UINT_MAX - digit) / 10)
        {
            return 0;
        }
        n = n * 10 + digit;
        (*s)++;
    } while (**s >= '0' && **s <= '9');
    *v = n;
    return 1;
}

int quanta_win_cpu_instance(const char *s, unsigned *group, unsigned *cpu)
{
    unsigned first, second;
    if (!quanta_strcmp(s, "_Total"))
    {
        return 0;
    }
    if (!number(&s, &first))
    {
        return -1;
    }
    if (!*s)
    {
        *group = 0;
        *cpu = first;
        return 1;
    }
    if (*s++ != ',' || !number(&s, &second) || *s || first > 65535 || second > 63)
    {
        return -1;
    }
    *group = first;
    *cpu = second;
    return 1;
}
