# Changelog

## 0.2.2 — Native Windows validation follow-up

- Исправлен native Windows live-session test: `count` ограничивает output frames, а не число samples; heartbeat frame может не содержать нового sample, если метрика имеет больший `min_poll_ns`.
- Windows network discovery теперь исключает `MIB_IF_ROW2.InterfaceAndOperStatusFlags.FilterInterface`, чтобы WFP/Npcap/VirtualBox filter modules не выдавались как отдельные пользовательские NIC.
- Для `INV_REFRESH_ON_EVENT` больше не печатается вводящая в заблуждение диагностика `polling clamped`; 5-секундная rediscovery cadence остаётся внутренней политикой.
- Исправлен Windows Markdown example selector `filesystem.root.*` → `fs.root.*`; соответствующая integration prerequisite также исправлена.
- Compile-time GUI/TUI validation использует `#if`, устраняя MSVC C4127 в GUI build и позволяя проверять проект с warnings-as-errors.
- Подтверждено пользователем: 0.2.1 собирается нативно MSVC 19.51 / VS 18 без GUI и MSVC 19.44 / VS 2022 с Dear ImGui; `windows_gui` проходит.

## 0.2.1 — Windows SDK networking header fix

- Fixed native MSVC compilation of the Windows network provider.
- `ws2ipdef.h` is now included before IP Helper headers, as required for
  `MIB_IF_ROW2`/`MIB_IF_TABLE2` declarations in current Windows SDKs.
- Removed direct inclusion of `netioapi.h`; `iphlpapi.h` owns that include path.
- No metric/query ABI or runtime behavior changes are intended.

## 0.2.0 — Windows source port

- Shared core platform layer: threads, clocks, cancellation, UTF-8 paths.
- 70 Windows metric families; physical memory and system commit kept distinct.
- SMBIOS types 0/1/2/17 and pure-C malformed-input tests.
- Processor-group-aware CPU utilization, localized PDH wildcard counters.
- Interface GUID/LUID identities; 64-bit counters and link speeds.
- Native Windows console TUI; shared rendering/navigation with POSIX.
- Win32/GDI software backend for existing ImGui view; UTF-8 font paths.
- MSVC /MT profiles, RC manifest, Unicode wmain, x64/Win32 build helpers.
- PE import audit, Windows native tests/workflow, 13 Windows query examples.
- Linux regressions tested; Windows SDK/runtime/full ImGui NOT available here.

# 0.1.0 — 2026-09-14

First Linux implementation. 52 metric families on x86; source-level build
selection; C99 INVQ parser/registry/scheduler/status model; deduplicated group
acquisition; live series discovery; rate/delta; bounded history; FILE and TUI;
optional ImGui 1.91 software/XCB frontend; CMake static builds; unit, fixture,
CLI, terminal and independent XCB tests. Full ImGui integration was not executed
in the preparation environment; see TEST_REPORT.md for tested configurations.

No claim of Windows/ARM/native Wayland support, general expression DAG,
universal sensor calibration, hard cancellation or stable external C ABI.
