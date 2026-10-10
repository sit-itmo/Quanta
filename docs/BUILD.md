# Сборка Quanta 0.6.0

Распаковать в **новую папку**. Старые CMake cache и объектные файлы не переносить.
Нужны CMake 3.21+, C11 compiler, а для GUI — C++11 compiler. При `BUILD_TESTING=ON`
нужен Python 3. **Исходники Dear ImGui 1.92.9b уже включены** в `vendor/imgui`.
CMake ничего не скачивает, не требует Git/submodules и не запускает PowerShell-скрипты.

## Linux / Ubuntu: обычная полная сборка, без preset

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3 libxcb1-dev
# Для автоматического теста настоящего окна в headless среде:
sudo apt install xvfb

cmake -S . -B build/default
cmake --build build/default --parallel
ctest --test-dir build/default --output-on-failure

./build/default/bin/quanta --version
./build/default/bin/quanta --list-modes
./build/default/bin/quanta examples/queries/04-dashboard-gui.q
```

Обычный configure включает **все имеющиеся для текущей ОС группы метрик, TUI, GUI,
встроенные тексты документации и тесты**. Для single-config generator без указанного
build type выбирается Release. Для Debug: `-DCMAKE_BUILD_TYPE=Debug` при configure.
`--list-modes` должен перечислять `file`, `tui`, `gui` (`cli` — alias файлового режима).

GUI по умолчанию **обязателен для полной native-конфигурации**: если нет C++ compiler
или XCB headers/library, configure сообщает причину и останавливается, а не выдаёт
сокращённую сборку за полную. GUI не запускается во время configure. Для запуска нужен
локальный X11/Xwayland display; WSL должен иметь работающий графический сервер/WSLg и
корректный `DISPLAY`. В этой реализации нет native Wayland backend, TCP/SSH X11 forwarding.
Для headless сервера без графического окружения используйте профиль без GUI.

## Windows / Visual Studio 2022: обычная полная сборка

Нужны Visual Studio 2022 (Desktop development with C++), Windows SDK, CMake и Python 3
в PATH. Используйте PowerShell или Developer Command Prompt; PowerShell 7 не требуется.
Ничего получать через `fetch_imgui.py` больше не нужно — такого скрипта в дереве нет.

```powershell
cmake -S . -B build/default -G "Visual Studio 17 2022" -A x64
cmake --build build/default --config Release --parallel
ctest --test-dir build/default -C Release --output-on-failure

.\build\default\bin\Release\quanta.exe --list-modes
.\build\default\bin\Release\quanta.exe get os.name os.arch --hal none
.\build\default\bin\Release\quanta.exe examples/queries/windows/04-dashboard-gui.q
```

`-G/-A` лишь явно выбирают генератор и x64, это не preset. При подходящем default generator
достаточно `cmake -S . -B build/default`. Debug собирается в том же дереве:

```powershell
cmake --build build/default --config Debug --parallel
ctest --test-dir build/default -C Debug --output-on-failure
```

Открывайте сгенерированную solution из `build/default`. `--config` при сборке и `-C` при
CTest должны совпадать. Дерево не зависит от vcpkg, его app-local integration отключена
для генерируемых VS проектов. Системные внешние build hooks/настройки пользовательской
VS не являются частью Quanta. Нативный MSVC/WinAPI-прогон этой поставки здесь не выполнен;
подготовленная CI-матрица не означает, что она уже запускалась.

## Без GUI, но со всеми native-метриками и TUI

Linux:

```bash
cmake --preset linux-no-gui
cmake --build --preset linux-no-gui --parallel
ctest --preset linux-no-gui --output-on-failure
./build/linux-no-gui/bin/quanta --list-modes
```

Windows:

```powershell
cmake --preset windows-no-gui
cmake --build --preset windows-no-gui --parallel
ctest --preset windows-no-gui --output-on-failure
```

Эти presets отключают **только GUI**. Они не требуют C++ compiler, ImGui или XCB,
но сохраняют все native collectors и TUI. Эквивалент без preset:
`cmake -S . -B build/no-gui -DQUANTA_WITH_GUI=OFF`.

Другие профили: `linux-default`/`windows-native` — полные; `linux-minimal`/
`windows-minimal` — базовые OS/common метрики без GUI/TUI; `linux-hwmon` — OS + hwmon;
`portable-posix` — только переносимый runtime/common metrics, без native Linux API.

## Один executable, выбор интерфейса и HAL

```bash
quanta get os.uptime --format value
quanta get os.uptime --format csv
quanta dashboard.q --cli --format table
quanta dashboard.q --tui
quanta dashboard.q --gui
quanta dashboard.q --mode cli --hal none
quanta --list-hals
```

CLI overrides имеют приоритет над query. Недоступный GUI/TUI возвращает ошибку, а не
незаметно заменяется текстовым режимом. `--hal none` не отключает native OS collectors.
GUI/TUI можно явно задавать `ON`, `OFF`, `AUTO`; `AUTO` оставлен для осознанной сборки с
fallback при отсутствии зависимостей, но **больше не является native default**.

## Как подключён GUI

Четыре upstream ImGui `.cpp` собираются в STATIC `imgui_vendor`; C++ язык включён в
корневом CMake, чтобы его runtime корректно доходил до app/tests через static libraries.
Ядро остаётся C11, и core-only targets по-прежнему линкуются C-компилятором.

Quanta использует собственный CPU renderer, Win32/GDI либо XCB window backend. SDL,
GLFW, OpenGL/Vulkan/DirectX SDK не нужны. ImGui 1.92 dynamic texture create/update/destroy,
Alpha8/RGBA32, filtering и `VtxOffset` реализованы и проверяются отдельным тестом.
CMake не собирает неиспользуемые примеры и чужие platform backends из ImGui.

Встроенный шрифт ImGui работает сразу. `--font` читает подходящий **локальный** TTF,
например для кириллицы; внешние файлы шрифтов не входят в поставку. Для замены исходников
предусмотрен `QUANTA_IMGUI_DIR`, но adapter принимает только проверяемую версию 1.92.9b.
Версия, лицензия и контрольные суммы: [vendor/IMGUI.md](../vendor/IMGUI.md).

## Проверки, static и переносимый профиль

```bash
cmake --preset portable-posix
cmake --build --preset portable-posix --parallel
./build/portable-posix/bin/quanta examples/queries/00-portable.q

cmake --preset linux-asan
cmake --build --preset linux-asan --parallel
ctest --preset linux-asan --output-on-failure

cmake -S . -B build/static -G Ninja -DQUANTA_STRICT_STATIC=ON -DQUANTA_WITH_GUI=OFF
cmake --build build/static --parallel
sh tools/verify_static.sh build/static/bin/quanta
```

`linux-asan` использует Clang/Clang++, ASan/UBSan и включает GUI. Нужны их runtime и
XCB development files. Для статического Linux нужны статические runtime libraries.
На MSVC `QUANTA_STRICT_STATIC=ON` задаёт `/MT` или `/MTd`; системные DLL остаются.
Аудит direct PE imports для такой сборки: `python tools/verify_windows_imports.py <exe>`.
Он не предназначен для доказательства совместимости и не запрещает DLL в обычном `/MD` build.

Обычная полная нативная сборка запускает `check` через ALL-target `quanta_verify`.
Ошибка теста делает build неуспешным. Сборка только target `quanta` этого не делает.
`-DQUANTA_TESTS_ON_BUILD=OFF` оставляет тесты для ручного запуска;
`-DBUILD_TESTING=OFF` исключает тестовое окружение и зависимость от Python полностью.
При cross-compiling host автоматически не запускает target binaries.

GUI renderer unit test выполняется без дисплея. Linux window smoke добавляется при
наличии Xvfb; если Xvfb не установлен, проверка окна не заявляется. Windows window test
требует доступный desktop. Список и результаты: [TESTING](TESTING.md).

## Встраивание и оставшиеся инструменты

`quanta_core` — core/query + HAL/OSAL, без sensors/UI; `quanta_sensors` добавляет каталог;
`quanta_frontends` — вывод. Все три STATIC. Public headers имеют один include root.

```bash
cmake --install build/default --prefix ./stage
python3 tools/embed_docs.py --check
```

Private headers не устанавливаются. Exported `find_package(Quanta)` пока не предоставлен.
В `tools/` оставлены только генератор/проверка документации и два полезных dependency audits;
дублирующие оболочки сборки и сетевой загрузчик удалены. Обычная сборка не требует генерации
исходников Python-скриптами: встроенная документация уже подготовлена; тест лишь проверяет её.

Опции групп: `QUANTA_WITH_MEMORY/CPU/DMI/NETWORK/HWMON/CPUID/FILESYSTEM/PROCESS`.
Другие: `QUANTA_EMBED_DOCS`, `QUANTA_WARNINGS_AS_ERRORS`, `QUANTA_SANITIZE`,
`QUANTA_STRICT_STATIC`, `QUANTA_PORTABLE`. ABI-размеры должны совпадать у всех компонентов.
ESP8266 собирается отдельно своим SDK: [ESP8266](ESP8266.md).
