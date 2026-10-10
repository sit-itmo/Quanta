# Отчёт проверок Quanta 0.3.0

Дата: 2026-10-10. Проверки выполнены в Linux x86-64 container, не на Windows и не на Ubuntu
пользователя. Toolchain: GCC 14.2, Clang 17, CMake 3.31.6; точные строки —
[audit/logs/environment.log](audit/logs/environment.log). Инструкция Ubuntu не означает,
что container был именно Ubuntu. GUI=OFF; TUI=ON кроме minimal. Warnings-as-errors включены.

## Исходная версия

`inv-0.2.2` сначала собран отдельно из переданного архива. GCC Debug/Ninja, dynamic executable,
GUI=OFF: компиляция успешна, **4/7 CTest suites прошли**. Не прошли devices, documentation,
integration. Логи configure/build/CTest сохранены в `audit/logs/original-*.log`.
Содержимое прежних test-logs внутри baseline ZIP не выдаётся за новые измерения.

## Итоговая матрица

| Конфигурация | Результат | Evidence |
|---|---|---|
| GCC Debug, полный native Linux | **12/12** | [build + auto CTest](audit/logs/gcc-debug-verified.log) |
| Clang Release, полный native Linux | **12/12** | [build + auto CTest](audit/logs/clang-release-verified.log) |
| Clang Debug, AddressSanitizer + UndefinedBehaviorSanitizer | **12/12**, diagnostics sanitizer не получены | [build + auto CTest](audit/logs/clang-asan-verified.log) |
| GCC Release, minimal: только OS, TUI=OFF, EMBED_DOCS=OFF | **11/11** | [build + auto CTest](audit/logs/minimal-verified.log) |
| GCC Release, strict static Linux executable | **12/12**; readelf: нет PT_INTERP / DT_NEEDED | [build + static check](audit/logs/static-final.log) |
| 14 public headers, каждый отдельно | C11 GCC и C++17 G++: без ошибок | [header checks](audit/logs/public-headers.log) |

Полные stdout конкретных C tests находятся в `audit/logs/*-ctest-details.log`.
Количество suites — не количество assertions и не процент code coverage. Счётчики core live
тестов могут зависеть от числа pump итераций. Функциональные ветки отключённых providers
в minimal не выполняются; suite name само по себе не доказывает их покрытие.
ASan выполнялся с `detect_leaks=1:halt_on_error=1`, UBSan — `halt_on_error=1`.

## Состав полного Linux набора

| Suite | Проверяемая область | Происхождение |
|---|---|---|
| `quanta_unit_tests` | query/values/units/model/format/raster; 3000 deterministic parser smoke-fuzz inputs | сохранён и адаптирован |
| `quanta_windows_data_tests` | bounded SMBIOS parser и Windows semantic arithmetic | сохранён; выполняется как portable C на Linux |
| `quanta_device_tests` | NIC replacement/rate reset, hwmon fault/poll minimum | сохранён regression, исправления теперь проходят |
| `quanta_linux_hal_tests` | OS mapping/text, CPU parse, meminfo, self/statm; invalid/overflow/duplicate cases | новый |
| `windows_schema` | Windows descriptors и Windows example-query validation | адаптирован; не WinAPI emulator |
| `frontend_modes` | 4 entrypoints, raw value, CSV/TXT, .q path with spaces, explicit provider, rejected blob flags | новый |
| `documentation` | embedded C docs соответствуют native definitions и Markdown | исправлен generator |
| `integration` | CLI collection/CSV/queries/errors/file creation, live CPU/network при включении | сохранён/адаптирован |
| `tui_pty` | терминальная интерактивная сессия, PTY input/exit | сохранён; только TUI build |
| `quanta_catalog_tests` | registry/default/explicit selection, scheduler grouping, malformed providers, ownership/cleanup/cancel/live | новый, core-only link |
| `quanta_history_tests` | bounded ring, time/order/sequence, resize/rollback, clone, typed values | новый, core-only link |
| `quanta_osal_tests` | time/files/path bounds, exclusive output, threads/mutex/condition | новый, core-only link |

Core-only executables сознательно не линкуются с `quanta_sensors`/`quanta_output`. Так скрытая
обратная зависимость core обнаруживается linker-ом, а не только просмотром схемы.
C tests используют CHECK/REQUIRE, поэтому Release с NDEBUG не выключает проверки.

## Воспроизведение матрицы

Обычные команды Linux/Windows приведены в [BUILD](BUILD.md). Для конкретного compiler:

```bash
cmake -S . -B build/gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/gcc-debug --parallel
cmake -S . -B build/clang-release -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/clang-release --parallel
cmake --preset linux-asan
cmake --build --preset linux-asan --parallel
cmake --preset linux-minimal
cmake --build --preset linux-minimal --parallel
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUANTA_STRICT_STATIC=ON -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/static --parallel
sh tools/verify_static.sh build/static/bin/quanta
```

При обычном build с BUILD_TESTING=ON и QUANTA_TESTS_ON_BUILD=ON тесты запускаются автоматически.
`ctest --test-dir <build>` можно повторить отдельно. Все final rebuild logs получены после
последнего изменения исходников/генерации metric docs, а не до форматирования.

## Проверка упаковки

Source archive содержит корневой каталог `Quanta-0.3.0`, без build/cache/object files.
`MANIFEST.sha256` перечисляет файлы поставки (кроме самого manifest). Вложенный original source ZIP
содержит 291 неизменённый исходный non-build файл из входного архива; исходный пользовательский ZIP
не изменён. Source-map и SHA256 входных материалов находятся в `audit/`.

Для окончательного ZIP отдельно выполняется распаковка в каталог с пробелами, проверка manifest
и чистая сборка/CTest через `linux-default`. Итоговый лог этого действия поставляется рядом с ZIP
как `Quanta-0.3.0-archive-check.log`, чтобы не изменять архив после его проверки.

## Что НЕ проверено

**Нативные Windows compiler/runtime tests не выполнены.** Нет доступного MSVC/Windows SDK и
Windows runtime. Portable Windows-data tests и schema-on-Linux не подтверждают работу
GetIfTable2/PDH/GetSystemTimes/GetProcessMemoryInfo, GUI и реального multi-group оборудования.
CI workflow подготовлен, но GitHub Actions здесь не запускался.

**GUI сборка и открытие окна не выполнены.** В исходном архиве нет ImGui sources; доступный fetch
не предоставил dependency. Сохранены frontend sources и инструкции; passing raster C tests
не заменяют compilation и взаимодействие с настоящим XCB/Win32 window.

Не выполнены hardware lab/stress-тесты, длительный мониторинг, все комбинации feature flags,
ARM/FreeRTOS, allocation-failure injection на всех путях и fuzzing с coverage. Успех sanitizer
означает отсутствие обнаруженных ошибок на выполненных тестах, а не доказательство их отсутствия.
Blob/record/offline/Pipe/UART/JTAG не тестируются как готовые функции: они ещё не реализованы.
