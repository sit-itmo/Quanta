# INV 0.2.2 — фактически выполненные проверки

Дата: 14 сентября 2026 года.

Поставка содержит исходники Windows/Linux. В среде подготовки нет Windows SDK/MSVC/Wine,
поэтому **сам INV 0.2.2 нативно на Windows здесь не пересобирался**. Однако пользователь
предоставил полный лог реальной нативной проверки INV 0.2.1, и именно по нему подготовлен
0.2.2.

## Что подтверждено реальной Windows-сборкой 0.2.1

На пользовательской Windows-машине:

- CLI/TUI профиль сконфигурирован Visual Studio 18 2026, MSVC 19.51.36257.0 и Windows SDK
  10.0.26100.0; `inv_core.lib`, тесты и `Release/inv.exe` успешно собраны.
- CTest: `unit`, `windows_data`, `documentation`, `integration` прошли. Единственный fail —
  `windows_platform`, assertion `model.samples>=2`.
- `inv.exe --version`, `list --compiled`, `describe`, inventory, memory, SMBIOS DIMM inventory,
  CPU utilization, CSV/Markdown output и `explain` реально запускались.
- GUI профиль отдельно сконфигурирован Visual Studio 2022 / MSVC 19.44 с `INV_WITH_GUI=ON`;
  настоящие `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` были
  скомпилированы, `inv_gui.lib` и `Release/inv.exe` успешно собраны.
- В GUI CTest `windows_gui` прошёл. Единственный fail снова был тот же
  `windows_platform:model.samples>=2`.

Таким образом, прежнее ограничение отчёта 0.2.1 «Windows build не подтверждён» больше не
актуально **для 0.2.1**: пользовательский лог подтверждает native MSVC build и существенную
часть runtime-тестов. Это не автоматически подтверждает 0.2.2 — его следует повторно
собрать тем же способом после этих точечных изменений.

## Что обнаружено по native runtime 0.2.1 и исправлено в 0.2.2

### 1. `windows_platform` был неверным тестом, а не ошибкой scheduler/live

Тест запускал:

```text
mode file; every 250ms; count 3; select os.uptime;
```

`count` по контракту INVQ ограничивает **output frames**, включая heartbeat frames.
`os.uptime` имеет минимальный poll interval 1 s. На быстрой Windows-машине engine успевал
сформировать три frame за ~0.75 s, но новое значение `os.uptime` к этому моменту ещё не было
положено собирать. Поэтому `model.frames == 3` при `model.samples == 1` — допустимый и ожидаемый
результат.

В 0.2.2 native test проверяет фактический контракт:

- session завершилась `INV_OK`;
- model получил end-session;
- `model.frames == 3`;
- `1 <= model.samples <= model.frames`.

Engine для обхода теста не менялся.

### 2. Windows network discovery выдавал NDIS filter modules как отдельные NIC

Native inventory показал WFP, Npcap, VirtualBox и Hyper-V filter-layer interfaces как
отдельные `device.net.*` series. Это объясняется семантикой `GetIfTable2`: таблица содержит
в том числе NDIS filter interfaces.

В 0.2.2 discovery пропускает строки, у которых
`MIB_IF_ROW2.InterfaceAndOperStatusFlags.FilterInterface != 0`. Физические и обычные
логические/виртуальные/VPN/loopback interfaces не отбрасываются этим правилом. Источник
counters по-прежнему один `GetIfTable2` snapshot, идентичность серии остаётся GUID + NET_LUID.

### 3. Ложный шум `polling clamped` для event-driven MAC property

`device.net.mac` имеет `INV_REFRESH_ON_EVENT`; внутренняя 5-секундная rediscovery cadence не
является пользовательским polling period. 0.2.1 печатал десятки диагностик вида
`polling clamped from 1.000s to 5.000s` — по одной на найденный interface.

В 0.2.2 clamp diagnostic печатается только для `INV_REFRESH_POLL`. Семантика сбора не
ослаблена; убрана вводящая в заблуждение диагностика для event-driven property.

### 4. Windows Markdown example содержал неверный key

`queries/windows/09-report-markdown.invq` выбирал `filesystem.root.*`, тогда как реальный
catalog key — `fs.root.*`. Поэтому native run корректно сообщал, что optional selector не
имеет instances. Исправлено на `fs.root.*`.

Дополнительно `tests/windows_integration.py` проверял prerequisite
`filesystem.root.total_bytes`; из-за этого блок полного прогона всех 13 Windows examples
никогда не включался. Исправлено на `fs.root.total_bytes`.

### 5. Убран MSVC C4127 в feature checks

Нативный GUI build 0.2.1 показал C4127 на константных выражениях `!INV_WITH_GUI` /
`!INV_WITH_TUI`. В 0.2.2 эти compile-time ветви оформлены через `#if`, поэтому сам
query validator не должен генерировать эти предупреждения при MSVC warnings-as-errors.

## Проверки INV 0.2.2 в среде подготовки

Среда: Debian GNU/Linux 13 x86-64; GCC 14.2.0, Clang 17.0.0, CMake 3.31.6, Python 3.
Это не Windows VM и не замена повторной native Windows-сборке.

| Проверка | Фактический результат |
|---|---|
| GCC Release, C99, strict static, GUI OFF, TUI ON, warnings-as-errors | Build OK; CTest **7/7** |
| Clang Release, GUI OFF, TUI ON, warnings-as-errors | Build OK; CTest **7/7** |
| Windows data tests (host-portable parser/math tests) | Passed |
| Windows schema target | **70 metric descriptors; 13 Windows INVQ files validated** |
| Embedded documentation consistency | Passed |
| Linux CLI integration | Passed |
| Linux PTY TUI integration | Passed |
| Strict-static audit | `inv` has no `PT_INTERP` and no `DT_NEEDED` |
| 11 Linux examples through FILE sink | Passed |
| Native Windows 0.2.2 build/runtime | **Not yet rerun** |
| Native Windows 0.2.2 GUI | **Not yet rerun** |

`windows_schema` compiles the real Windows descriptors and validates all Windows query files,
but uses test-only provider stubs and does **not** execute WinAPI. It therefore validates the
registry/query contract, not `MIB_IF_ROW2.FilterInterface` runtime behaviour.

## Recommended native 0.2.2 verification

CLI/TUI:

```powershell
cmake -S . -B build -A x64 -DINV_WITH_GUI=OFF -DINV_STRICT_STATIC=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python tools/verify_windows_imports.py build/Release/inv.exe --profile msvc
```

Expected change: `windows_platform` should now pass. The inventory should no longer be flooded
with names ending in WFP/Npcap/VirtualBox filter layers, and `01-inventory.invq` should no longer
print one `device.net.mac: polling clamped ...` message per interface.

Then:

```powershell
$Inv = ".\build\Release\inv.exe"
& $Inv run --query-file queries/windows/01-inventory.invq --redact
& $Inv run --query-file queries/windows/02-memory.invq
& $Inv run --query-file queries/windows/09-report-markdown.invq --redact --output report-022.md
& $Inv explain --query-file queries/windows/03-dashboard-tui.invq
```

GUI:

```powershell
cmake -S . -B build-gui -G "Visual Studio 17 2022" -A x64 -DINV_WITH_GUI=ON -DINV_STRICT_STATIC=ON
cmake --build build-gui --config Release --parallel
ctest --test-dir build-gui -C Release --output-on-failure
.\build-gui\Release\inv.exe run --query-file queries/windows/04-dashboard-gui.invq
```

## Remaining scope limits

- `sensor.temperature` is Windows ACPI/PDH thermal-zone data; it is not presented as a guaranteed
  CPU package/core sensor.
- No kernel driver/MSR/SMBus/EC/SMART/GPU vendor backend is part of this release.
- Windows API floor macros alone do not prove compatibility with older Windows/CRT combinations.
- XP/v141_xp is not supported by this branch.
- Passing tests do not prove absence of all defects or driver-specific edge cases.

## Проверка финальной упаковки

Финальный ZIP формируется только из исходного дерева без build-каталогов. После упаковки
он распаковывается в новый каталог, `MANIFEST.sha256` проверяется для каждого вложенного
файла, затем из распакованной копии повторяются GCC Release strict-static configure/build
и CTest. Архивы дополнительно имеют внешние SHA-256 checksums. Если эта процедура не
проходит, пакет не считается финальным.
