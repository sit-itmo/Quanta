/**
 * @file sensors/private/windows_native.h
 *
 * Native Windows collector helpers, including system-drive discovery. This header is private,
 * not public API.
 *
 * Snapshot: the result of a single collection operation shared by several fields. Batched
 * collection is an internal optimization, not a selectable provider.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT -- native helpers for Windows metrics only. */
#ifndef QUANTA_WINDOWS_NATIVE_H
#define QUANTA_WINDOWS_NATIVE_H
#include "windows_internal.h"
int quanta_win_system_drive(wchar_t *buffer, size_t capacity);
#endif
