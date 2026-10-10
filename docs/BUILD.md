# Сборка Quanta 0.8.0

Новое дерево и новый build directory. CMake 3.21+, C11 compiler; для GUI — C++11.
Обычный `cmake -S . -B build/default` включает все доступные native-группы,
TUI, GUI, встроенную документацию и тесты. Backend по умолчанию — **OPENGL**.
Отсутствующая обязательная GUI-зависимость — ошибка configure, а не скрытая урезанная сборка.

## Linux / Ubuntu

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends build-essential cmake libglfw3-dev libx11-dev
cmake -S . -B build/default
cmake --build build/default --parallel
ctest --test-dir build/default --output-on-failure
./build/default/bin/quanta examples/queries/04-dashboard-gui.q
```

Используется установленный `glfw3` CMake package (3.3+) и `OpenGL::GL`.
OpenGL development headers/libraries обычно приходят с `libglfw3-dev`; при их
отсутствии установить `libgl-dev`. Не нужно собирать GLFW из исходников на Linux. `--no-install-recommends` не устанавливает
необязательные для Quanta Vulkan/Wayland development packages.
Доступность OpenGL 3.3 проверяется при создании окна, не во время configure.
В WSL нужен рабочий WSLg/X-сервер. Сервер без дисплея может собирать GUI, но не запускать
его как интерактивное окно. Для CPU-варианта ниже достаточно X11.

Ninja не обязателен: Linux presets используют Unix Makefiles. Python не нужен для
компиляции или формирования документации: сгенерированные данные уже включены.

## Windows / Visual Studio 2022

Установить Desktop development with C++ и Windows SDK.
[CMake Windows x64 Installer](https://cmake.org/download/) добавить в PATH.
**GLFW 3.4 уже включён целиком в `vendor/glfw`, ImGui — в `vendor/imgui`.**
Ничего дополнительно распаковывать, скачивать или устанавливать через vcpkg не нужно.
CMake собирает локальные исходники; сеть при конфигурации и сборке не используется.
[Происхождение и проверка GLFW](../vendor/GLFW.md).

```powershell
cmake -S . -B build/default -G "Visual Studio 17 2022" -A x64
cmake --build build/default --config Release --parallel
ctest --test-dir build/default -C Release --output-on-failure
.\build\default\bin\Release\quanta.exe examples/queries/windows/04-dashboard-gui.q
```

`-G`/`-A` — выбор генератора/архитектуры, не preset. При настроенном default generator
достаточно обычного configure без них. Сборка Debug в том же дереве:

```powershell
cmake --build build/default --config Debug --parallel
ctest --test-dir build/default -C Debug --output-on-failure
```

Configuration у build и CTest должна совпадать. GLFW строится STATIC, не берётся готовая
`.lib` от другого compiler/CRT. `QUANTA_GLFW_DIR` позволяет указать другой **локальный**
каталог. vcpkg app-local hooks отключены; Git/PowerShell 7/Python не нужны для обычной сборки.
Обычный MSVC profile использует `/MD`, strict profile — `/MT`; системные DLL/драйвер OpenGL
остаются внешними. Нативная Windows-проверка этой ревизии не выполнялась.

## Framebuffer по отдельному ключу

```bash
cmake -S . -B build/framebuffer -DQUANTA_GUI_BACKEND=FRAMEBUFFER
cmake --build build/framebuffer --parallel
ctest --test-dir build/framebuffer --output-on-failure
```

На Windows явно выбрать VS/x64 при необходимости, build `--config Release`, CTest `-C Release`.
Presets: `linux-framebuffer`, `windows-framebuffer`. Linux packages:

```bash
sudo apt-get install --no-install-recommends build-essential cmake libx11-dev
```

В этой ветке не ищутся и не линкуются GLFW/OpenGL. CPU-рендерер использует адаптированный
quad-путь portable_gui, проверяемые ImGui 1.92 текстуры и triangle fallback.
Linux presentation — X11/Xlib RGB24 TrueColor + 32-bit RAM-буфер; Windows — GDI.
Окно ограничено 4096×4096, текстуры — 8192×8192; буфер проверяется перед выделением.
Это не `/dev/fb0`, DRM/KMS или UEFI backend; [границы переноса](../vendor/PORTABLE_GUI.md).
Ввод мыши/клавиатуры и resize поддержаны; CPU-backend не объявляет полноценную
GLFW-совместимую clipboard/IME/platform-интеграцию. Native Wayland presentation у него нет.

## Русская документация и шрифт GUI

Markdown и встроенный вывод `describe` используют UTF-8. Для отображения русских
описаний в панели Details GUI укажите свой локальный TTF-шрифт с поддержкой кириллицы
через `--font /path/to/font.ttf`. Встроенный шрифт Dear ImGui не гарантирует наличие
этих символов. Файлы шрифтов в архив не включены; это необязательный ресурс пользователя,
не зависимость сборки.

## Без GUI и минимальные профили

```bash
cmake --preset linux-no-gui
cmake --build --preset linux-no-gui --parallel
ctest --preset linux-no-gui --output-on-failure
```

Windows: заменить на `windows-no-gui`. Такие presets выключают только GUI, оставляя
TUI и все native-метрики. Эквивалент: `-DQUANTA_WITH_GUI=OFF`; C++ и graphics не нужны.
`linux-minimal`/`windows-minimal` дополнительно выключают TUI и расширенные группы.
`portable-posix` — C-only core/query/file + common-метрики на минимальном POSIX runtime.
`QUANTA_WITH_GUI`/`QUANTA_WITH_TUI` принимают ON/OFF/AUTO. AUTO — только явный opt-in
для допуска отсутствующих frontend; default native — ON.

## Тесты без дополнительных обязательных зависимостей

CTest входит в CMake. Native C/C++ unit tests собираются при `BUILD_TESTING=ON`.
Python3 ищется **необязательно**: при отсутствии CMake выводит сообщение и не регистрирует
Python CLI/source/documentation checks. Это не «все тесты пройдены», а сокращённый набор.

Полная проверка на Ubuntu может дополнительно использовать:

```bash
sudo apt-get install python3 xvfb
```

Python и Xvfb — тестовые инструменты, не зависимости приложения. GUI window smoke
добавляется на Linux, когда есть оба. Framebuffer pixel tests не требуют X-сервера.
Windows GUI test требует доступного desktop. При cross-compiling host не запускает target.

Обычная полная сборка запускает CTest через `quanta_verify` (ALL), ошибки прерывают build.
Сборка только target `quanta` не запускает весь suite. Опции:

```text
-DQUANTA_TESTS_ON_BUILD=OFF   тесты собраны, но выполняются вручную
-DQUANTA_PYTHON_TESTS=OFF     только native-тесты, даже если Python установлен
-DBUILD_TESTING=OFF          только приложение и библиотеки
```

## Sanitizer, static, установка

```bash
cmake --preset linux-asan -DQUANTA_GUI_BACKEND=FRAMEBUFFER
cmake --build --preset linux-asan --parallel
ctest --preset linux-asan --output-on-failure

cmake -S . -B build/static -DQUANTA_STRICT_STATIC=ON -DQUANTA_WITH_GUI=OFF
cmake --build build/static --parallel
sh tools/verify_static.sh build/static/bin/quanta

cmake --install build/default --prefix ./stage
```

Sanitizer preset требует Clang/Clang++ и runtime; по умолчанию он выбирает OpenGL,
выше явно показан проверенный framebuffer-вариант. Для strict Linux без динамических
зависимостей используйте no-GUI: native graphics/драйверы не становятся автономным static EXE.
При MSVC strict CRT режим у GLFW и Quanta согласован.

`quanta_core` — C core/query + HAL/OSAL; `quanta_sensors` — встроенный каталог;
`quanta_frontends` — отображение. GUI добавляет `imgui_vendor` и **одну** backend-библиотеку.
Публичные заголовки устанавливаются из единого `include/quanta/`; приватные не устанавливаются.
Готового exported `find_package(Quanta)` пока нет.

Сохранённые инструменты: `tools/embed_docs.py` (ручная регенерация/проверка docs),
`verify_static.sh`, `verify_windows_imports.py`. Fetch/build-wrapper scripts не возвращались.
Опции метрик: `QUANTA_WITH_MEMORY/CPU/DMI/NETWORK/HWMON/CPUID/FILESYSTEM/PROCESS`.
GUI не включается в отдельную SDK-сборку ESP8266: [ESP8266.md](ESP8266.md).
