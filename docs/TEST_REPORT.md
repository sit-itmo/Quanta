# Проверки Quanta 0.4.0

Дата: 2026-10-10. Среда — Linux x86-64 container (Debian 13), GCC 14.2, Clang 17,
CMake 3.31.6, Python 3.13.5. Это не Ubuntu пользователя, не Windows и не Xtensa.
[Точные версии и CMake cache options](audit/logs/environment.log).
GUI отключён; TUI включён, кроме minimal. Во всех пяти конфигурациях включён
`QUANTA_WARNINGS_AS_ERRORS=ON`. Логи ниже относятся только к текущей ревизии.

## Выполненная матрица

| Конфигурация | Результат CTest | Build и полный stdout тестов |
|---|---|---|
| GCC Debug, весь Linux-набор | **16/16** | [build](audit/logs/gcc-debug-build.log), [CTest -V](audit/logs/gcc-debug-ctest.log) |
| Clang Release, весь Linux-набор | **16/16** | [build](audit/logs/clang-release-build.log), [CTest -V](audit/logs/clang-release-ctest.log) |
| Clang Debug + ASan + UBSan | **16/16** | [build](audit/logs/clang-asan-build.log), [CTest -V](audit/logs/clang-asan-ctest.log) |
| GCC Release minimal: desktop OS only, TUI=OFF, embedded docs=OFF | **15/15** | [build](audit/logs/minimal-build.log), [CTest -V](audit/logs/minimal-ctest.log) |
| GCC Release, strict static Linux | **16/16** | [build](audit/logs/static-build.log), [CTest -V](audit/logs/static-ctest.log) |

Minimal desktop options не отключают отдельные host-тесты embedded component: они намеренно
собираются со своим ESP source/ABI manifest. Feature-dependent части обычных C-тестов в minimal
не выполняются. Поэтому 15/15 не означает проверку всех отключённых аппаратных групп.

В ASan/UBSan-прогоне использованы `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` и
`UBSAN_OPTIONS=halt_on_error=1`. Диагностик sanitizer в выполненном прогоне не получено;
это не доказательство отсутствия всех возможных ошибок.

Для static executable `readelf` подтвердил отсутствие `PT_INTERP` и `DT_NEEDED`:
[program headers](audit/logs/static-program-headers.log), [dynamic section](audit/logs/static-dynamic-section.log).
Статическая Linux-сборка не означает независимость от ядра/устройств или Windows system DLL.

## Что проверяют 16 наборов

| CTest suite | Основные проверки |
|---|---|
| `quanta_hal_tests` | Hardware-only wrapper, NULL/bounds/overflow, capabilities, write gate, copied physical allowlist, injected HAL; Linux device-file fixtures, affinity restore; SMBIOS 2.x/3.x through mock physical memory |
| `quanta_unit_tests` | Parser, query validation, values/units, frontend model/format/raster, 3000 deterministic parser smoke-fuzz inputs |
| `quanta_windows_data_tests` | Portable SMBIOS parsing и Windows arithmetic; это не выполнение WinAPI |
| `quanta_device_tests` | NIC identity/replacement, rate reset, hwmon faults и minimum poll |
| `quanta_linux_metrics_tests` | Native OS/CPU/memory/process input fixtures, invalid/overflow/duplicate cases; high-level code находится в sensors, не HAL |
| `windows_schema` | Windows registry и query examples на Linux со stubs; не эмулятор Windows |
| `hal_cli_architecture` | HAL list/info/query/CLI override, invalid/duplicate names, legacy provider() rejection, запускаемые baseline examples; active source boundaries и версия ресурсов |
| `frontend_modes` | Четыре executable, value/CSV/TXT, query path с пробелами, непредусмотренные режимы и ошибки |
| `documentation` | Embedded Markdown соответствует исходным metric docs/definitions |
| `integration` | CLI, CSV, queries, output files и доступные native reads |
| `tui_pty` | Настоящая терминальная сессия через pseudo-TTY; запуск, input и exit |
| `quanta_catalog_tests` | Unique metric keys, direct lifecycle, private snapshot batching, discovery/value errors, cleanup/cancel/live; injected HAL borrowing и конфликт имён |
| `quanta_history_tests` | Bounded history, sequence/time order, resize/rollback, clone и typed values |
| `quanta_osal_tests` | Time/files/path bounds, exclusive output, threads/mutex/condition |
| `quanta_esp8266_enabled_tests` | Полный ESP source manifest со SDK stubs, VDD enabled |
| `quanta_esp8266_disabled_tests` | Тот же manifest, VDD disabled и отсутствие вызова VDD API |

В записанном GCC Debug прогоне HAL: 135 checks, CLI/boundaries: 111,
ESP enabled: 122, ESP disabled: 115 — все без failures.
Количество suites/checks не является процентом code coverage. Часть live/scheduler
счётчиков может зависеть от количества итераций pump, поэтому агрегированное число не используется.

Core public API tests линкуются без `quanta_sensors`/frontend. Минимальный HAL и инфраструктурный
OSAL — разрешённые зависимости core. Tests не требуют root и не выполняют реальные записи
в `/dev/mem`, MSR, PCI или PortIO. Используются собственные RAM/file fixtures и текущая
thread affinity с восстановлением исходной маски. GPIO проверяется через подмену SDK.

## Дополнительные compile checks

**14 публичных заголовков** проверены каждый отдельно через GCC C11 и G++ C++17, с
`-Wall -Wextra -Wpedantic -Werror -fsyntax-only`: [лог](audit/logs/public-headers.log).

Отрицательная compile-проверка включила VDD33 с `PHY vdd33_const=0`: компиляция ожидаемо
отклонена защитным `#error`. Это успешный negative test, не ошибка основной сборки:
[лог](audit/logs/esp8266-invalid-config.log).

## Что означает ESP host test

SDK component и два host targets используют один `cmake/esp8266_sources.cmake`: ядро, query,
registry, все 9 MCU fields, OS fields, FreeRTOS OSAL, HAL GPIO и serial formatter.
`app_main.c` также компилируется, но бесконечный firmware loop на хосте не исполняется.
Проверены 64-bit clock после границы 32-bit ticks, sleep rounding, низкопамятный read-once API,
engine batching и поля/status/errors SDK. Test headers не входят в SDK component.

**Настоящие SDK headers/libraries/toolchain здесь не использовались.** Эти проверки обнаруживают
ошибки собственного кода и контрактов, но не доказывают Xtensa link, ABI SDK или работу железа.
API дополнительно сопоставлены с первичными SDK v3.4 исходниками; ссылки — в [ESP8266.md](ESP8266.md).

## Не выполнено

- Native Windows compile/run с MSVC, Windows HAL affinity и GUI/ImGui window.
- Настоящий ESP8266 SDK configure/build/link/size, flash, serial capture, stack/heap endurance на плате.
- Чтение реальной physical memory/MSR/PCI/PortIO, SMBIOS firmware scan, GPIO и любые аппаратные записи.
- Windows kernel driver: он не реализован и не требовался в этой ревизии.

Windows CI workflow подготовлен в `.github/workflows/ci.yml`, но удалённый run не заявляется.
Инструкции сборки — [BUILD.md](BUILD.md); hardware acceptance — [ESP8266.md](ESP8266.md) и [TODO.md](TODO.md).

## Проверка готового архива

Финальный ZIP сопровождается внешним `Quanta-0.4.0-archive-check.log`: повторная распаковка
в новый путь с пробелами, проверка каждого SHA256 по `MANIFEST.sha256`, clean configure/build,
автоматический CTest и отдельный повторный CTest, version/HAL info/query smoke runs.
Этот внешний лог формируется после упаковки: его нет внутри ZIP, чтобы проверка не меняла
проверяемый архив. Промежуточные ошибки разработки не выдаются за итоговые результаты.

Предыдущие отчёты 0.3.0 находятся в `docs/history/0.3.0/` и не входят в приведённую матрицу.
