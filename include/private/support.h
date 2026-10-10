/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_SUPPORT_H
#define QUANTA_SUPPORT_H
#include "quanta/osal.h"
#include "quanta/text.h"
#include "quanta/types.h"
#include "quanta_config.h"
#include <inttypes.h>
#include <limits.h>
#define QUANTA_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define QUANTA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define QUANTA_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
