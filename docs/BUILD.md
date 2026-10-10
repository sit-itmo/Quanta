# Сборка и проверка 0.4.0

Это полный новый baseline. Не накладывайте его поверх 0.3.0/INV и не переносите старый CMake cache.
Desktop: C11, CMake 3.21+, C compiler; Python 3 для расширенных тестов. GUI требует C++.
ESP8266 собирается отдельно средствами SDK: [ESP8266.md](ESP8266.md).

## Linux / Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3
cmake --preset linux-default
cmake --build --preset linux-default --parallel
ctest --preset linux-default --output-on-failure
./build/linux-default/bin/quanta examples/queries/00-baseline.q
./build/linux-default/bin/quanta hal-info --hal native
./build/linux-default/bin/quanta get os.name os.arch --hal none
./build/linux-default/bin/quanta_csv get os.uptime
```

Отдельный Debug без presets:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Минимальный OS-only frontend и отдельно hwmon:

```bash
cmake --preset linux-minimal
cmake --build --preset linux-minimal --parallel
cmake --preset linux-hwmon
cmake --build --preset linux-hwmon --parallel
```

Это настройка **набора метрик**, а не выбор способа через providers. HAL остаётся отдельным.
`quanta list --compiled` отражает именно текущий executable, а не все существующие в repository .def.

ASan/UBSan:

```bash
sudo apt install clang
cmake --preset linux-asan
cmake --build --preset linux-asan --parallel
ctest --preset linux-asan --output-on-failure
```

Static без GUI:

```bash
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUANTA_STRICT_STATIC=ON
cmake --build build/static --parallel
readelf -l build/static/bin/quanta
readelf -d build/static/bin/quanta
```

Требуются static runtime libraries выбранного toolchain. Отсутствие PT_INTERP/DT_NEEDED проверяет
связывание этого executable, а не переносимость на любой Linux/kernel/CPU.

## Windows / Visual Studio 2022

Установлены компоненты C++ build tools и Windows SDK, CMake и Python доступны в PATH.
Из PowerShell в корне Quanta:

```powershell
cmake --preset windows-native
cmake --build --preset windows-native --parallel
ctest --preset windows-native --output-on-failure
.\build\windows-native\bin\Release\quanta.exe --version
.\build\windows-native\bin\Release\quanta.exe --list-hals
.\build\windows-native\bin\Release\quanta.exe hal-info --hal native
.\build\windows-native\bin\Release\quanta.exe get os.name os.arch --hal none
```

Debug в том же multi-config дереве:

```powershell
cmake --build build/windows-native --config Debug --parallel
ctest --test-dir build/windows-native -C Debug --output-on-failure
.\build\windows-native\bin\Debug\quanta.exe examples/queries/windows/00-baseline.q
```

Сгенерированную solution можно открыть в Visual Studio. Сборка конкретного target `quanta` не
обязана запускать ALL-цель проверки; используйте полную сборку либо target `check`.
`windows-minimal` — отдельный preset. `QUANTA_STRICT_STATIC=ON` выбирает /MT или /MTd,
но не встраивает системные WinAPI DLL. `windows-native` HAL не содержит драйвера.
Нативный Windows build этой ревизии требует проверки на Windows; Linux schema tests не заменяют его.

## Опциональный GUI

GUI сохранён: Dear ImGui + существующий software rasterizer, Win32 либо XCB adapter.
Это не новый OpenGL frontend. Исходники ImGui не включены в ZIP.

```bash
# Linux:
sudo apt install libxcb1-dev
python3 tools/fetch_imgui.py
cmake -S . -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUANTA_WITH_GUI=ON
cmake --build build/gui --parallel
./build/gui/bin/quanta examples/queries/04-dashboard-gui.q --mode gui
```

Либо укажите собственный `-DQUANTA_IMGUI_DIR=/path/to/imgui`. GUI dependency не загружается
автоматически в обычной сборке; opt-in `QUANTA_FETCH_IMGUI=ON` существует отдельно.
На Windows после получения ImGui configure `windows-native` с `-DQUANTA_WITH_GUI=ON`.
GUI окно этой поставки не проверялось; соответствующие test scripts/CI hooks сохранены.

## Опции, тесты, установка

`QUANTA_WITH_MEMORY/CPU/DMI/NETWORK/HWMON/CPUID/FILESYSTEM/PROCESS`, `QUANTA_WITH_TUI`,
`QUANTA_WITH_GUI`, `QUANTA_EMBED_DOCS`, `QUANTA_WARNINGS_AS_ERRORS`, `QUANTA_SANITIZE`,
`QUANTA_STRICT_STATIC`, стандартный `BUILD_TESTING`.

По умолчанию `BUILD_TESTING=ON`, `QUANTA_TESTS_ON_BUILD=ON`: CTest входит в обычную полную сборку.
CMake при cross-compiling не пытается запускать target executables на build-хосте. ESP smoke
тесты на Linux — отдельные host targets, не попытка выполнить Xtensa binary.
Без Python CMake явно отключает только Python-наборы; для полной приёмки установите Python.

```bash
cmake --build build/linux-default --target check
python3 tools/embed_docs.py --check
cmake --install build/linux-default --prefix ./stage
```

`quanta_core`, `quanta_hal`, `quanta_sensors` — STATIC. Сторонний C-клиент внутри дерева
использует `add_subdirectory` и `target_link_libraries(... quanta_sensors)` или только core
для своего catalogue. Установка копирует headers/libraries, но отдельный exported
`find_package(Quanta)` config пока не подготовлен. Различные MCU/desktop ABI macros смешивать нельзя.

Логи фактически выполненных проверок — [TEST_REPORT](TEST_REPORT.md).
