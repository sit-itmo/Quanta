/* SPDX-License-Identifier: MIT */
#ifndef QUANTA_WINDOWS_INTERNAL_H
#define QUANTA_WINDOWS_INTERNAL_H
/* winsock2.h must precede windows.h (Win32 lean is also set by CMake). */
#include "support.h"
#include <wchar.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2ipdef.h> /* Required before iphlpapi.h exposes MIB_IF_ROW2/TABLE2. */
quanta_status quanta_win_status(DWORD);
void quanta_win_error(char *, size_t, DWORD, const char *);
int quanta_win_utf8(char *, size_t, const wchar_t *);
wchar_t *quanta_win_widen(const char *);
uint64_t quanta_win_filetime(FILETIME);
#endif
