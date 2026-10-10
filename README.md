# Quanta 0.6.0

<p align="center"><img src="docs/assets/logo.jpg" width="280" alt="Quanta"></p>

**Сбор, обработка и отображение аппаратных и системных метрик на C.**
Проект Мастерской системного программирования: Linux, Windows и активный порт ESP8266/FreeRTOS.

## Цель и архитектура

Единый query, каталог и scheduler отделяют получение измерений от отображения.
Аппаратные метрики используют минимальный HAL; другие реализации обращаются к API ОС,
CPUID или SDK. **Одно имя метрики — один метод получения**: без выбираемых providers,
приоритетов и скрытой подмены метода. HAL выбирается независимо.

**Одна программа — `quanta`.** Текст, CSV, Markdown, value, TUI и GUI — её режимы.
Query находится в core, публичные заголовки — только в `include/quanta/`.
OSAL фиксирует минимальный контракт памяти, stdio, времени, потоков и блокировок.
GUI/TUI имеют свои платформенные backends, а ядро не зависит от C++.

```text
app: CLI / query file → frontend dispatcher → file / TUI / ImGui
                                              ↓
                       core: query, registry, scheduler, history, live
                                              ↓
                         metric implementations: sensors
                                ↙                         ↘
                      primitive HAL                native OS / CPUID / SDK
```

Текущее описание: [ARCHITECTURE](docs/ARCHITECTURE.md), [OSAL-контракт](include/quanta/osal.h),
[HAL](docs/HAL.md), [Query](docs/QUERY.md), [добавление метрик](docs/METRICS.md).
[Исходный дизайн](docs/Quanta_Design.md) и
[предоставленная схема](docs/assets/architecture-original.jpg) сохранены как историческая
основа; их providers и отдельные executable не описывают текущую реализацию.

## Быстрый запуск: полная сборка по умолчанию

**Dear ImGui 1.92.9b уже находится в `vendor/imgui`.** Обычный CMake без preset включает
все native-группы метрик, GUI, TUI и тесты. Сетевые загрузчики не нужны.

Linux / Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3 libxcb1-dev xvfb
cmake -S . -B build/default
cmake --build build/default --parallel
ctest --test-dir build/default --output-on-failure
./build/default/bin/quanta --list-modes
./build/default/bin/quanta examples/queries/04-dashboard-gui.q
```

Для окна нужен локальный X11/Xwayland. Xvfb используется только автоматическим window test.
Windows: Visual Studio 2022 с Desktop C++ tools и Windows SDK, CMake, Python 3 в PATH:

```powershell
cmake -S . -B build/default -G "Visual Studio 17 2022" -A x64
cmake --build build/default --config Release --parallel
ctest --test-dir build/default -C Release --output-on-failure
.\build\default\bin\Release\quanta.exe --list-modes
.\build\default\bin\Release\quanta.exe examples/queries/windows/04-dashboard-gui.q
```

Полная сборка сама запускает CTest; ошибка теста делает сборку неуспешной. Сборка одного
target `quanta` не запускает все тесты. [Полная инструкция и ограничения](docs/BUILD.md).

**Без GUI, но со всеми native-метриками и TUI:**

```bash
cmake --preset linux-no-gui
cmake --build --preset linux-no-gui --parallel
ctest --preset linux-no-gui --output-on-failure
```

На Windows — те же команды с `windows-no-gui`. Эквивалент без preset:
`cmake -S . -B build/no-gui -DQUANTA_WITH_GUI=OFF`. В такой сборке не нужен C++/XCB/ImGui.
`linux-minimal` и `windows-minimal` дополнительно сокращают набор метрик и убирают TUI.

## Query, форматы и HAL

```text
version 1;
mode cli;
hal "none";
every 1s;
count 3;
select os.name;
select os.uptime;
```

```bash
quanta get os.uptime --format value
quanta get os.uptime --format csv
quanta dashboard.q --tui
quanta dashboard.q --gui
quanta dashboard.q --cli --format table --hal native
quanta --list-hals
```

CLI имеет приоритет над query. `hal none` отключает аппаратные примитивы, не native
OS-метрики. Недоступный GUI/TUI возвращает ошибку. Выходной файл создаётся без перезаписи.

## Исходники и метрики

```text
include/quanta/             Public C API, osal.h, hal.h, metric_def.h
core/src/                   Query и переносимый механизм обработки
osal/                       Общий runtime и OS implementations
hal/                        Аппаратные primitives/backends
sensors/common/              HAL-метрики и общие декодеры
sensors/os/{linux,windows}/  Native collectors
sensors/arch/x86/            CPUID
sensors/platform/esp8266/    MCU/SDK-метрики
metrics/                    Компактные .def; registry остаётся обычным C
frontends/                  File, TUI, GUI, native windows + CPU renderer
app/                        Единственный executable
firmware/esp8266/            SDK component и serial app
vendor/imgui/               Локальные unmodified upstream library sources
cmake/, tests/, docs/, examples/
```

Пример `.def`:

```c
QUANTA_METRIC(os_memory_total_bytes,
    QUANTA_META("os.memory.total_bytes", "Physical memory"),
    QUANTA_GAUGE_BYTES(250),
    QUANTA_SNAPSHOT(quanta_memory, "total"),
    QUANTA_SOURCE("", "/proc/meminfo"))
```

Helpers сокращают повторение полей, но не меняют ABI, units или callbacks.
Правила и нестандартные случаи: [METRICS](docs/METRICS.md).

`portable-posix` оставляет настоящее core/query/file и common-метрики без native Linux
collectors и UI. Это пример реализации OSAL, а не готовый noOS-порт.
ESP8266 сохранил все девять `mcu.*` ключей, OS-метрики и активный SDK-компонент.
VDD33 выключен по умолчанию; [SDK-инструкция и аппаратная приёмка](docs/ESP8266.md).

## Изменения 0.6.0 и проверки

Исправлены C/C++ linking при GUI, include path Windows frontend, доступ к 64-битному
представлению реестра без позднего RRF flag, относительный путь Windows integration tests.
Renderer переведён на динамические текстуры ImGui 1.92; добавлены pixel/lifecycle tests.
Встроены библиотечные исходники ImGui, удалены fetch/build wrapper scripts. Все 137 записей
в 21 `.def` переведены на helpers без изменения 3014 сравниваемых полей дескрипторов.

Linux full GUI/TUI, no-GUI, portable, sanitizer и проверка готового архива описаны в
[TESTING](docs/TESTING.md). Linux GUI действительно собирается и запускается на Xvfb;
отдельный renderer test не требует дисплея. **Нативный MSVC/Windows и настоящий ESP SDK/плата
здесь не проверены**. CI содержит Windows Debug/Release, но результат ещё должен быть получен.

Архив распаковывать в новую папку и собирать заново. Он заменяет предыдущий baseline целиком,
не является overlay. Старый CMake cache, особенно настройки AUTO/OFF GUI, не переносить.

## TODO

- [ ] Нативная Windows-приёмка; настоящая SDK/Xtensa-сборка и плата ESP8266; ручная UI-приёмка.
- [ ] Windows driver HAL, Linux GPIO, расширение SMBIOS/ACPI.
- [ ] Blob record/replay, File/Pipe/UART/JTAG acquisition, event subscriptions и poll budget.

[Полный TODO](docs/TODO.md). Providers и отдельные `quanta_cli/csv/txt` намеренно удалены,
а не отложены до следующего выпуска.

## Авторы и лицензия

Исходная атрибуция **INV contributors** сохранена в [MIT LICENSE](LICENSE).
Quanta развивается в Мастерской системного программирования. Имена отдельных авторов и
сопровождающих в переданных материалах не указаны; персональные имена добавляются участниками.

[Сторонние компоненты](THIRD_PARTY.md), [точный состав ImGui и хэши](vendor/IMGUI.md).
В архиве есть библиотечные исходники ImGui и его лицензия; SDK, toolchains, драйверы,
внешние файлы шрифтов и готовые platform binaries не включены.
