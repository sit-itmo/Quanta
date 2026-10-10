# Тесты и проверка поставки 0.6.0

## Запуск

```bash
cmake -S . -B build/default
cmake --build build/default --parallel
ctest --test-dir build/default --output-on-failure
```

Обычная полная native-сборка сама запускает CTest. В Visual Studio multi-config нужны
согласованные `--config Release` при build и `-C Release` при CTest. `check` зависит от
программы и всех тестовых executable; нет запуска black-box tests до успешной линковки.
Сборка одного `quanta` либо `QUANTA_TESTS_ON_BUILD=OFF` не выполняет ALL-проверку.

## Что исправлено по пользовательским логам

| Исходный симптом | Исправление |
|---|---|
| Linux: `cc` линкует GUI; unresolved `operator new/delete`, exception runtime | CXX включён в верхнем CMake scope для всех indirect consumers; app действительно линкуется C++ driver |
| Windows TUI: не найден `windows_internal.h` | Приватный OSAL include path добавлен frontend target, включая Win32 GUI |
| Windows: не объявлен `RRF_SUBKEY_WOW6464KEY` | 64-битное представление открывается через RegOpenKeyExW/KEY_WOW64_64KEY; чтение через обычный RegGetValueW; handle закрывается |
| MSVC: tentative uninitialized static const в catalog test | Forward helper вместо предварительного определения объекта |
| После build failure: `FileNotFoundError`, tests Not Run | Исправляются причины сборки, а не скрываются тесты отсутствующих программ |
| Windows integration ищет examples внутри tests | Исправлен root после переноса скрипта в integration |
| Получение ImGui через python3/fetch script | Library sources включены локально, загрузчик удалён |

`pwsh.exe`/InitializeDefaultDrives в логе сами по себе не доказывают конкретный источник
внешнего hook. В новых VS проектах отключена неиспользуемая vcpkg/app-local integration;
собственные build wrappers не используются. Исполнение MSBuild в исходном окружении
пользователя всё равно требуется для окончательной проверки Windows.

Дополнительно устранена несовместимость прежнего GUI adapter с **переданной 1.92.9b**:
новый texture protocol, font atlas, reset/sampler callbacks, новая сигнатура PathStroke.
Проверки vendor/adapter компилируются с `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`.

## Организация тестов

CTest — единственный runner, без стороннего test framework. Проверки не исчезают с NDEBUG.

```text
tests/unit/                 catalog, history, OSAL, HAL, metric helpers, core, ImGui renderer
tests/platform/linux/       parsing, discovery, device identity
tests/platform/windows/     native tests, pure-data tests, schema fixtures
tests/platform/esp8266/     SDK seam, только host
tests/integration/          CLI/HAL/форматы, native, TUI, реальное GUI-окно
tests/source_contract.py    include/runtime/query/MCU/vendor regression checks
```

`catalog/history/osal/metric_def` линкуют **только C core**, даже при включённом GUI.
Windows schema на Linux проверяет каталог/queries, не исполняет WinAPI.
`source_contract.py` — regression check, не формальный анализ всех зависимостей C.

**ImGui renderer:** 38 проверок с настоящими upstream sources без window server.
Проверяются создание RGBA32/Alpha8 texture, частичные updates и source pitch, сохранение
texture ID, отклонение update вне границ, sampler/reset, alpha blend, шрифтовый atlas
при разных размерах, >65536 vertices через VtxOffset, destruction до/после upload,
shutdown и повторная очистка. Проверяются pixels и состояния, не только exit code.

**GUI window:** Linux smoke запускает именно Quanta на собственном Xvfb и проверяет
завершение по query deadline. При отсутствии Xvfb этот тест не добавляется, но renderer
unit test остаётся. Windows smoke на целевой ОС создаёт окно, меняет размер, посылает
Unicode/WM_CLOSE и проверяет завершение. Это не подмена frontend файловым режимом.

**MCU:** enabled/disabled SDK seam использует тот же список исходников и ABI defines, что
firmware-компонент. Компилируются query, serial formatter и app_main; бесконечный serial
loop на host не исполняется. SDK calls заменены fixtures. Все 9 MCU ключей сохраняются.
Это не Xtensa cross-build, не real SDK link и не проверка платы.

## Выполнено в этой среде

Linux x86-64, GCC 14.2.0, Clang 17.0.0, CMake 3.31.6, Ninja 1.12.1, Python 3.13.5,
XCB 1.17.0 и Xvfb. Это версии проверочной среды, не требования на точные версии установки.

| Конфигурация | Результат |
|---|---|
| Обычный CMake **без preset**, GCC Release, Makefiles, GUI + TUI | **19/19** |
| GCC Debug, GUI + TUI, warnings-as-errors | **19/19** |
| Clang Debug + ASan/UBSan, GUI + TUI, leak detection | **19/19** |
| Ninja Multi-Config Debug, GUI + TUI | **19/19** |
| Ninja Multi-Config Release, GUI + TUI | **19/19** |
| `linux-no-gui`, все native-метрики + TUI | **17/17** |
| `linux-minimal`, без GUI/TUI | **16/16** |
| `portable-posix`, минимальный runtime | **10/10** |
| Static Linux, без GUI | **17/17**, нет PT_INTERP/DT_NEEDED |

Sanitizer suite с `detect_leaks=1:halt_on_error=1` и `UBSAN_OPTIONS=halt_on_error=1`
завершился без зарегистрированных ошибок. Это не доказательство отсутствия всех дефектов.
Ninja Multi-Config проверяет правильную конфигурацию/пути в много-конфигурационной сборке,
**но не заменяет Visual Studio/MSVC**.

Дополнительно проверены:

- 15 публичных headers отдельно в C11 и C++17 (30 компиляций), единственный include root;
- Product-only build без GUI/TUI/tests: нет поиска C++/Python и импорта libstdc++/libxcb;
- Установка и отдельный C-клиент установленного core/query, без sensors/frontends;
- Все 137 дескрипторов до/после перехода на helpers: 3014 полей, 0 расхождений;
- Побайтное совпадение включённых ImGui sources/headers с пользовательским архивом;
- Запуск dashboard в настоящем Linux frontend: [снимок Xvfb](assets/gui-linux.png).

Последний снимок показывает фактическую программу, не макет. Он не проверяет все состояния
UI, monitor DPI, все layouts, пользовательские шрифты или интерактивность каждого элемента.

## Границы проверки

**Нативный Windows/MSVC-прогон здесь не выполнен.** Известные ошибки исправлены в исходниках
и CMake, но runtime collectors, Win32 GUI/TUI, процессорные группы и реальные MSBuild hooks
должны пройти целевую проверку. В CI предусмотрены full/no-GUI/minimal и Debug/Release;
выполнение workflow не заявляется заранее.

**Настоящие SDK/Xtensa-сборка и плата ESP8266 не проверены.** Требуются clean build/link,
size/map, serial, heap/stack и длительный аппаратный прогон: [ESP8266](ESP8266.md).
Опасные записи hardware не выполнялись. Тесты не требуют root/установки драйвера.

## Проверка поставки

MANIFEST.sha256 перечисляет файлы исходного дерева, кроме самого манифеста. Внешний SHA256
проверяет ZIP. Готовый ZIP распаковывается в новый путь с пробелами; манифест и CRC проверяются,
выполняется чистая обычная полная CMake-сборка без preset и повторный CTest. Итоговый лог
прилагается отдельно вместе с журналом матрицы, чтобы не засорять source tree build outputs.

`python3 tools/embed_docs.py --check` проверяет совпадение документации с `.def` и
подготовленным `sensors/metric_docs.c`. Исходный `docs/Quanta_Design.md` сохранён побайтно:
действующие изменения отражены отдельно в ARCHITECTURE, а не внесены незаметно в оригинал.
