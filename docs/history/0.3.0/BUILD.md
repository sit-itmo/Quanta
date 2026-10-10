# Сборка и проверка

Запускайте команды из корня Quanta 0.3.0. Основные требования: CMake >= 3.21,
C11 compiler, системные development headers. В Linux presets используется Ninja.
Python 3 нужен для integration/documentation tests и optional dependency helper, но не для core.
C++ compiler и ImGui нужны только при GUI=ON. Входной архив и зависимости GUI не распаковываются
автоматически в процессе обычной CLI/TUI-сборки.

## Linux / Ubuntu

Установка инструментов (отдельное действие пользователя, сборка сама не использует sudo):

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3
```

Проверьте `cmake --version` (не ниже 3.21), затем:

```bash
cmake --preset linux-default
cmake --build --preset linux-default --parallel
ctest --preset linux-default --output-on-failure
./build/linux-default/bin/quanta --version
./build/linux-default/bin/quanta examples/queries/01-inventory.q --redact
```

Presets: `linux-default` (Release), `linux-minimal` (только OS, без TUI и embedded docs),
`linux-hwmon` (минимальный + hwmon/docs), `linux-asan` (Clang Debug, ASan+UBSan).
Для sanitizer-профиля установите `clang`; он не совместим со strict static executable.

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan --parallel
ctest --preset linux-asan --output-on-failure
```

Обычный вариант без presets, в том числе для другого generator:

```bash
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUANTA_WARNINGS_AS_ERRORS=ON
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Для Make: замените `-G Ninja` на `-G "Unix Makefiles"` и используйте **другой** build directory.
При смене compiler/architecture/generator также создавайте новый каталог сборки.

## Windows / Visual Studio 2022

Нужны Visual Studio 2022 (или Build Tools) с компонентом **Desktop development with C++**,
Windows SDK, CMake >= 3.21 и Python 3. Команды рассчитаны на PowerShell;
CMake/Python должны находиться через PATH. Не нужно устанавливать Qt, Boost, vcpkg или driver.

```powershell
cmake --preset windows-native
cmake --build --preset windows-native --parallel
ctest --preset windows-native --output-on-failure

.\build\windows-native\bin\Release\quanta.exe --version
.\build\windows-native\bin\Release\quanta.exe get os.name os.arch
.\build\windows-native\bin\Release\quanta.exe examples\queries\windows\01-inventory.q --redact
```

Для Debug используйте явно конфигурацию (preset по умолчанию Release):

```powershell
cmake --build build/windows-native --config Debug --parallel
ctest --test-dir build/windows-native -C Debug --output-on-failure
.\build\windows-native\bin\Debug\quanta.exe --version
```

Без presets:

```powershell
cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build/vs --config Release --parallel
ctest --test-dir build/vs -C Release --output-on-failure
```

Генерируются `build/vs/Quanta.sln` и проекты. Source tree не содержит вручную поддерживаемых sln/vcxproj.
Профиль `windows-minimal` проверяет минимальный состав. `--config`/`-C` должны совпадать:
Visual Studio — multi-config generator. Формальное ограничение в CMake — MSVC >= 19.28;
компиляция более старым MSVC не поддерживается этой конфигурацией.

**Нативная Windows-сборка в среде подготовки не выполнена.** Сверка API, portable Windows-data
fixtures и schema-on-Linux не заменяют компиляцию/запуск WinAPI-кода. Workflow в
`.github/workflows/ci.yml` подготовлен, но выполнение GitHub Actions здесь не производилось.

## CTest при сборке

`include(CTest)` включает BUILD_TESTING по умолчанию. Target `check` зависит от test executables
и всех четырёх CLI, вызывает CTest с timeout каждого suite 120s. Target `quanta_verify ALL`
делает его частью обычной **нативной полной** сборки. Даже no-op build повторяет тесты.
Ошибка теста возвращает ошибку build. Сборка только конкретного library target тесты не запускает.

```bash
cmake --build build/debug --target check
```

- `-DQUANTA_TESTS_ON_BUILD=OFF`: тесты собираются, запускайте `ctest` вручную.
- `-DBUILD_TESTING=OFF`: test executables/check не создаются; для быстрой сборки зависимого проекта.
- При cross-compilation auto-run отключён. Тесты нужно запускать на target, а не на build host.
- Без Python остаются C suites; конфигурация явно сообщает, какие Python tests пропущены.
  Для полного набора установите Python и повторно выполните configure.

C-тесты используют CHECK/REQUIRE, не исчезающие при `NDEBUG`. Release действительно проверяется.

## Build options

| Опция | Default | Значение |
|---|---|---|
| `BUILD_TESTING` | ON | CTest/tests |
| `QUANTA_TESTS_ON_BUILD` | ON | автоматический запуск |
| `QUANTA_WITH_TUI` | ON | терминальный frontend |
| `QUANTA_WITH_GUI` | OFF | C++/ImGui/native window |
| `QUANTA_WITH_MEMORY/CPU/DMI/NETWORK/HWMON/CPUID/FILESYSTEM/PROCESS` | ON | отдельные native providers |
| `QUANTA_EMBED_DOCS` | ON | встроенные тексты metric documentation |
| `QUANTA_WARNINGS_AS_ERRORS` | OFF | `/WX` или `-Werror` |
| `QUANTA_SANITIZE` | OFF | Linux ASan+UBSan |
| `QUANTA_STRICT_STATIC` | OFF | Linux `-static`; MSVC static CRT |
| `QUANTA_FETCH_IMGUI` | OFF | явное разрешение network fetch только для GUI |

OS provider обязателен. CPUID дополнительно ограничен x86 архитектурой. Не существующие
`windows-driver`/`windows-full` presets не добавлены ради видимости: соответствующих backends нет.

## Optional GUI

Это сохранённый adapter Dear ImGui 1.91.9b, не обновление на произвольную последнюю версию.
В архиве нет исходников ImGui. Предоставьте каталог с `imgui.cpp`, `imgui_draw.cpp`,
`imgui_tables.cpp`, `imgui_widgets.cpp` и соответствующими заголовками/лицензией:

```bash
# Linux, optional system dependencies:
sudo apt install g++ libxcb1-dev
cmake -S . -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQUANTA_WITH_GUI=ON -DQUANTA_IMGUI_DIR=/absolute/path/to/imgui \
  -DQUANTA_FETCH_IMGUI=OFF
cmake --build build/gui --parallel
./build/gui/bin/quanta examples/queries/04-dashboard-gui.q
```

Вместо внешнего каталога helper `python3 tools/fetch_imgui.py` явно скачивает pinned источники
в `vendor/imgui`; он требует сеть. Проверка commit/version не эквивалентна независимому security audit.
CMake network fetch также возможен, но только с `-DQUANTA_FETCH_IMGUI=ON`.

Windows:

```powershell
cmake -S . -B build/gui-vs -G "Visual Studio 17 2022" -A x64 `
  -DQUANTA_WITH_GUI=ON -DQUANTA_IMGUI_DIR=C:/deps/imgui -DQUANTA_FETCH_IMGUI=OFF
cmake --build build/gui-vs --config Release --parallel
.\build\gui-vs\bin\Release\quanta.exe examples\queries\windows\04-dashboard-gui.q
```

GUI использует software raster, не OpenGL. Linux backend рассчитан на локальное X11/XWayland-соединение;
SSH forwarding/произвольный network X11 не поддержан. Шрифты не включены, `--font` — локальный файл
пользователя. GUI в этой поставке не прошёл нативную сборку/визуальную приёмку.

## Static executable и установка

`quanta_core` всегда STATIC. Это не означает статическую линковку всех системных библиотек EXE.
Default executable — обычная системная линковка. Для Linux strict static нужны соответствующие
static system archives; для GUI это также касается XCB и транзитивных зависимостей.

```bash
cmake -S . -B build/static -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUANTA_STRICT_STATIC=ON
cmake --build build/static --parallel
sh tools/verify_static.sh build/static/bin/quanta
cmake --install build/linux-default --prefix ./stage
```

На Windows strict profile задаёт `/MT` или `/MTd`, но OS DLL по-прежнему используются.
Установка в `stage` не требует повышенных прав. Готовые binaries не входят в source archive.
