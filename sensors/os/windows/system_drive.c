/**
 * @file sensors/os/windows/system_drive.c
 *
 * Find the system drive for Windows filesystem metrics with path-capacity validation.
 *
 * Metric: a measurement descriptor with a stable name, type, unit and callbacks. The descriptor
 * itself is not a measured value.
 */
/* SPDX-License-Identifier: MIT */
#include "windows_native.h"
int quanta_win_system_drive(wchar_t *out, size_t cap)
{
    wchar_t directory[MAX_PATH];
    UINT n = GetWindowsDirectoryW(directory, MAX_PATH);
    if (!n || n >= MAX_PATH || cap < 4 || directory[1] != L':')
    {
        return 0;
    }
    out[0] = directory[0];
    out[1] = L':';
    out[2] = L'\\';
    out[3] = 0;
    return 1;
}
