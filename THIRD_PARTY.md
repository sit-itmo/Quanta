# Third-party components and source provenance

The application/core/collectors/TUI/software rasterizer/XCB and Win32 adapters are provided
under the root MIT license. No external monitor executable is called at runtime.

## Dear ImGui (optional; not bundled in this source archive)

- Upstream: https://github.com/ocornut/imgui
- Version: v1.91.9b
- Commit: f5befd2d29e66809cd1110a152e375a7f1981f06
- License: upstream MIT, retain its LICENSE.txt when fetching/distributing.
- Four .cpp translation units and required headers are linked as a static library.
- No system font files are included. --font reads a user-provided local file.

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
- Dear ImGui backend contract (pinned): https://github.com/ocornut/imgui/blob/v1.91.9b/docs/BACKENDS.md
- XCB core API: https://xcb.freedesktop.org/manual/group__XCB____API.html

## Windows toolchain and system API

No Windows SDK, runtime binary or font is distributed in this archive. The user
provides a licensed SDK/toolchain. Application libraries and optional ImGui are
STATIC; MSVC uses /MT for release and /MTd for debug in the strict profile.
System import libraries refer to Windows DLLs; these DLLs are not bundled or
incorporated into the EXE. MinGW may depend on the system MSVCRT/UCRT even with
-static. Consult toolchain redistribution terms before distributing binaries.

Primary API/semantics references are listed in docs/AUDIT.md and per-metric
Markdown. SMBIOS parser is original bounded C code, not code copied from dmidecode.
Windows PDH localization follows Microsoft's documented English-counter sequence.
