# Third-party components and source provenance

The application/core/collectors/TUI/software rasterizer/XCB and Win32 adapters are provided
under the root MIT license. No external monitor executable is called at runtime.

## Dear ImGui (bundled library sources)

- Upstream: https://github.com/ocornut/imgui
- Version: **1.92.9b**, from the user-supplied archive, not a network fetch.
- Library sources/headers are unmodified; license: `vendor/imgui/LICENSE.txt` (MIT).
- Four core .cpp translation units are built as STATIC `imgui_vendor` by default.
- `imgui_demo.cpp` is retained for reference, not compiled into Quanta.
- Unused upstream backends/examples/build scripts and external font files are excluded.
- Exact included files and source SHA256: [vendor/IMGUI.md](vendor/IMGUI.md).
- `--font` reads a local font supplied by the user. No external font files are distributed.

## XCB (Linux-only optional GUI)

- Upstream: https://xcb.freedesktop.org/
- Ubuntu development package: libxcb1-dev. Default GUI links the selected system XCB library; strict static additionally requires static archives.
- XCB sources/license files are not bundled; retain applicable upstream notices
  when distributing the linked library or binary. The platform uses the local
  socket connection path, avoiding DNS/NSS and vendor GPU libraries.

## Toolchain C/C++ libraries

A static build also incorporates the toolchain C library, libm, pthread support
and, for GUI, C++ runtime support. Their terms are not replaced by this project's
MIT license. This archive intentionally contains source, not a prebuilt glibc
executable. Arrange all required licenses, source/relink materials, and applicable
exceptions before distributing your own statically linked binaries.

## Primary semantic references

- Linux procfs: https://docs.kernel.org/filesystems/proc.html
- Linux hwmon ABI: https://docs.kernel.org/hwmon/sysfs-interface.html
- Linux network statistics: https://docs.kernel.org/networking/statistics.html
- DMI sysfs implementation: https://github.com/torvalds/linux/blob/master/drivers/firmware/dmi-id.c
- Dear ImGui backend contract (pinned): vendor/imgui/docs/BACKENDS.md (bundled version 1.92.9b)
- XCB core API: https://xcb.freedesktop.org/manual/group__XCB____API.html

## Windows toolchain and system API

No Windows SDK, runtime binary or font is distributed in this archive. The user
provides a licensed SDK/toolchain. Application libraries and bundled ImGui are
STATIC; MSVC uses /MT for release and /MTd for debug in the strict profile.
System import libraries refer to Windows DLLs; these DLLs are not bundled or
incorporated into the EXE. MinGW may depend on the system MSVCRT/UCRT even with
-static. Consult toolchain redistribution terms before distributing binaries.

Primary API/semantics references are listed in docs/HAL.md, docs/ESP8266.md and per-metric
Markdown. SMBIOS parser is original bounded C code, not code copied from dmidecode.
Windows PDH localization follows Microsoft's documented English-counter sequence.

## ESP8266 SDK (not bundled)

The active firmware component targets ESP8266_RTOS_SDK v3.4 APIs and its Xtensa LX106 toolchain.
SDK source, prebuilt radio libraries and toolchain are NOT in this archive. Follow Espressif's
individual source/binary licenses; the root MIT license does not relicense them.
`tests/platform/esp8266/stubs` is original minimal test fixture code, not an SDK redistribution.

- https://github.com/espressif/ESP8266_RTOS_SDK/tree/v3.4
- https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.4/README.md

The original SDK configuration supplied by the user is retained only under firmware/esp8266/reference;
it is not asserted to fit a different board.

## Build-fix references

- CMake language scope: https://cmake.org/cmake/help/latest/command/enable_language.html
- RegOpenKeyExW: https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexw
- Alternate registry views: https://learn.microsoft.com/en-us/windows/win32/winprog64/accessing-an-alternate-registry-view

Source references explain contracts; they do not constitute execution tests of this archive.
