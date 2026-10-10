# Сборка Quanta 0.5.0

Распаковать в **новую папку**, не поверх 0.4.0. Нужны CMake 3.21+, C11 compiler;
при `BUILD_TESTING=ON` также Python 3. Для GUI дополнительно C++ compiler и ImGui.
Настройки применяются при configure; выполнение query не загружает отсутствующие модули.

## Linux / Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3
cmake --preset linux-default
cmake --build --preset linux-default --parallel
ctest --preset linux-default --output-on-failure
./build/linux-default/bin/quanta --version
./build/linux-default/bin/quanta --list-modes
./build/linux-default/bin/quanta --list-hals
./build/linux-default/bin/quanta examples/queries/00-baseline.q
```

`linux-default`: все включаемые группы метрик, GUI/TUI=AUTO. Без ImGui сборка остаётся
полноценной CLI/file/TUI-программой. Отсутствие GUI сообщается при configure.

Без presets, Debug:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

## Windows / Visual Studio 2022

Нужны workload разработки C++, Windows SDK, CMake и Python в PATH. Из PowerShell в корне:

```powershell
cmake --preset windows-native
cmake --build --preset windows-native --parallel
ctest --preset windows-native --output-on-failure
.\build\windows-native\bin\Release\quanta.exe --list-modes
.\build\windows-native\bin\Release\quanta.exe get os.name os.arch --hal none
.\build\windows-native\bin\Release\quanta.exe get os.uptime --format csv
```

Debug в том же multi-config дереве:

```powershell
cmake --build build/windows-native --config Debug --parallel
ctest --test-dir build/windows-native -C Debug --output-on-failure
```

Сгенерированную solution можно открыть в Visual Studio. Полная сборка выполняет тесты;
сборка только проекта `quanta` — нет. `windows-minimal` исключает дополнительные группы
метрик и оба UI. `QUANTA_STRICT_STATIC=ON` задаёт /MT или /MTd, но системные WinAPI DLL остаются.
Драйвер в проект не включён. Нативный прогон этой ревизии на Windows пока не выполнен.

## Один executable, выбор интерфейса

```bash
quanta get os.uptime --format value
quanta dashboard.q --cli --format csv --output report.csv
quanta dashboard.q --tui
quanta dashboard.q --gui
quanta dashboard.q --mode cli --hal none
```

Копий `quanta_cli`, `quanta_csv`, `quanta_txt` больше нет. `--format txt` — alias `table`.
CLI overrides имеют приоритет над query. Недоступный GUI/TUI не заменяется file-режимом:
команда сообщает ошибку. `--list-modes` выводит compiled modes (`file` включает alias `cli`).

## GUI: Dear ImGui + software/native backend

CMake использует только локальные исходники. GUI и TUI имеют три значения:
`AUTO` — включить при доступности; `ON` — потребовать; `OFF` — исключить.
Для текущего адаптера поддерживаемая версия ImGui — **1.91.9b**. В архиве её нет.

```bash
# Linux: C++ compiler + локальный X11/Xwayland через XCB
sudo apt install g++ libxcb1-dev
# Явное получение pinned dependency; нужен доступ к сети:
python3 tools/fetch_imgui.py
cmake --preset linux-default -DQUANTA_WITH_GUI=ON
cmake --build --preset linux-default --parallel
./build/linux-default/bin/quanta examples/queries/04-dashboard-gui.q --gui
```

Вместо загрузчика можно предоставить исходники вручную:
`-DQUANTA_IMGUI_DIR=/path/to/imgui-1.91.9b`. На Windows после получения исходников:

```powershell
python tools/fetch_imgui.py
cmake --preset windows-native -DQUANTA_WITH_GUI=ON
cmake --build --preset windows-native --parallel
.\build\windows-native\bin\Release\quanta.exe examples/queries/windows/04-dashboard-gui.q --gui
```

Windows backend — Win32/GDI, Linux — локальный XCB; renderer программный, без обязательного
OpenGL/Vulkan. Нет native Wayland backend и поддержки удалённого X11 в этом адаптере.
Для кириллицы `--font` принимает путь к подходящему локальному TTF; шрифты не распространяются.
Для headless Linux GUI smoke нужен Xvfb; без него приёмка окна выполняется отдельно.
Опции `QUANTA_FETCH_IMGUI` нет: автоматические сетевые зависимости удалены.

## Минимальные и диагностические сборки

```bash
# Минимальный native Linux: базовые OS + общие метрики, без GUI/TUI
cmake --preset linux-minimal
cmake --build --preset linux-minimal --parallel

# Действительно переносимый контракт: без native Linux collectors/HAL
cmake --preset portable-posix
cmake --build --preset portable-posix --parallel
./build/portable-posix/bin/quanta examples/queries/00-portable.q

# Clang AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset linux-asan
cmake --build --preset linux-asan --parallel
ctest --preset linux-asan --output-on-failure
```

`portable-posix` сейчас предоставлен для POSIX host: fopen, clock_gettime/nanosleep, pthread.
Это пример port backend, не Windows/ESP32/noOS эмулятор. Для другой платформы реализуются
`include/quanta/osal.h`, clock/thread/file backend и HAL либо HAL без возможностей.
`linux-hwmon` сохранён как небольшая конфигурация OS + hwmon.

```bash
# Static Linux, без зависимости от GUI libraries
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQUANTA_STRICT_STATIC=ON -DQUANTA_WITH_GUI=OFF
cmake --build build/static --parallel
sh tools/verify_static.sh build/static/bin/quanta
```

Нужны статические runtime libraries toolchain. Отсутствие PT_INTERP/DT_NEEDED означает
статическую линковку этого файла, не гарантирует работу на любом Linux/kernel/CPU.

## CMake, тесты и встраивание

Нативная полная сборка по умолчанию запускает `check` через ALL-target `quanta_verify`.
Ошибка CTest делает build неуспешным. Python обязателен, когда тесты включены. Для чистой
сборки библиотеки/программы без тестового окружения явно задайте `-DBUILD_TESTING=OFF`.
`-DQUANTA_TESTS_ON_BUILD=OFF` сохраняет тесты, но переводит запуск в ручной режим.
При cross-compiling host автоматически не исполняет target binaries.

```bash
cmake --build build/linux-default --target check
python3 tools/embed_docs.py --check
cmake --install build/linux-default --prefix ./stage
```

`quanta_core` содержит query + общие механизмы + HAL/OSAL, без sensors/UI. `quanta_sensors`
добавляет встроенный каталог. `quanta_frontends` добавляет вывод. Все три — STATIC;
внешний C-клиент внутри дерева использует `target_link_libraries(... quanta_core)` либо
`quanta_sensors`. При установке public headers идут в include/quanta; private не устанавливаются.
Готовый exported `find_package(Quanta)` package пока не предоставлен.

Опции групп метрик: `QUANTA_WITH_MEMORY/CPU/DMI/NETWORK/HWMON/CPUID/FILESYSTEM/PROCESS`.
Другие: `QUANTA_EMBED_DOCS`, `QUANTA_WARNINGS_AS_ERRORS`, `QUANTA_SANITIZE`,
`QUANTA_STRICT_STATIC`, `QUANTA_PORTABLE`. Размеры ABI должны совпадать у всех участников;
ESP задаёт их единым SDK source manifest и собирается отдельно: [ESP8266](ESP8266.md).
