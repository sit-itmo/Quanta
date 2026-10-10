/* SPDX-License-Identifier: MIT */
#ifndef INV_WINDOWS_INTERNAL_H
#define INV_WINDOWS_INTERNAL_H
/* winsock2.h must precede windows.h (Win32 lean is also set by CMake). */
#include <winsock2.h>
#include <ws2ipdef.h> /* Required before iphlpapi.h exposes MIB_IF_ROW2/TABLE2. */
#include "internal.h"
#include <wchar.h>
inv_status inv_win_status(DWORD);
void inv_win_error(inv_read_ctx *,DWORD,const char *);
int inv_win_utf8(char *,size_t,const wchar_t *);
wchar_t *inv_win_widen(const char *);
uint64_t inv_win_filetime(FILETIME);
int inv_win_system_drive(wchar_t *,size_t);
#endif
