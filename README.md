# Quanta 0.8.0

<p align="center"><img src="docs/assets/logo.jpg" width="280" alt="Quanta"></p>

**Сбор, обработка и отображение аппаратных и системных метрик на C.**
Проект Мастерской системного программирования. Одна программа `quanta`: CLI, файл,
CSV/Markdown/value, TUI и GUI на Dear ImGui. Основные платформы — Linux и Windows;
отдельно сохранён активный порт ESP8266/FreeRTOS.

## Цель и архитектура

Query, каталог, планировщик и история отделены от способов измерения и отображения.
**Одно имя метрики — один способ получения**, без выбираемых providers и скрытого
резервного метода. HAL предоставляет аппаратные примитивы; сборщики ОС/CPUID/SDK
могут его не использовать. Минимальный контракт исполнения описан в
[include/quanta/osal.h](include/quanta/osal.h). Публичные заголовки находятся в
`include/quanta/`, парсер query входит в `core/`.

```text
app → frontend (file / TUI / ImGui) → core (query / catalog / scheduler / history / live)
                                      ↓
                            sensors (metric callbacks)
                              ↙                 ↘
                      primitive HAL       native OS / CPUID / SDK
```

[Архитектура](docs/ARCHITECTURE.md) · [Понятия и чтение кода](docs/CODE_GUIDE.md) ·
[Query](docs/QUERY.md) · [HAL](docs/HAL.md) · [Добавление метрик](docs/METRICS.md).
[Исходный дизайн](docs/Quanta_Design.md) и [схема](docs/assets/architecture-original.jpg)
сохранены как историческая основа: providers и несколько программ в них уже неактуальны.

## Что входит в эту версию

**GLFW 3.4 теперь включён целиком в `vendor/glfw`**, из переданного исходного архива.
ImGui 1.92.9b и его GLFW/OpenGL3-адаптеры уже находятся в `vendor/imgui`.
В Windows дополнительные загрузки библиотек не нужны: используется локальное дерево.
На Linux по-прежнему применяется системный GLFW, устанавливаемый пакетами.

Комментарии собственных исходников, заголовки файлов, тесты и скрипты — **на английском**.
Документация Quanta в Markdown, включая описания метрик, — **на русском**.
Исходники, лицензии и собственные справочники сторонних библиотек сохранены на языке
оригинала без изменений; русские инструкции по интеграции находятся в `vendor/*.md`.
Правило языка, целостность GLFW/ImGui и актуальность встроенной документации проверяются тестами.

## Linux / Ubuntu: полная сборка по умолчанию

GUI по умолчанию: **Dear ImGui + GLFW + OpenGL 3.3**.

```bash
sudo apt-get update
sudo apt-get install --no-install-recommends build-essential cmake libglfw3-dev libx11-dev

cmake -S . -B build/default
cmake --build build/default --parallel
ctest --test-dir build/default --output-on-failure

./build/default/bin/quanta --list-modes
./build/default/bin/quanta examples/queries/04-dashboard-gui.q
```

При отсутствии файлов разработки OpenGL CMake выдаёт сообщение с указанием пакета
`libgl-dev`. Для запуска окна нужен графический сеанс и поддержка OpenGL 3.3; в WSL —
работающий WSLg/X-сервер. **Ninja, Python, Git, GLAD/GLEW, SDL и сетевые загрузчики
не обязательны для обычной сборки.** Дополнительные проверки используют Python/Xvfb,
когда они установлены. Сама конфигурация и сборка ничего из сети не скачивают.

## Windows: локальные зависимости уже включены

Нужны Visual Studio 2022 с компонентом Desktop development with C++ и Windows SDK.
**[Скачать CMake для Windows](https://cmake.org/download/)** — выбрать Windows x64 Installer
и добавить CMake в PATH. Минимальная версия проекта — CMake 3.21.

**GLFW отдельно скачивать и распаковывать не надо.** В архиве уже есть
`vendor/glfw/CMakeLists.txt`, `include/GLFW`, `src`, `CMake` и исходная лицензия.
[Состав и контрольные суммы](vendor/GLFW.md).

```powershell
cmake -S . -B build/default -G "Visual Studio 17 2022" -A x64
cmake --build build/default --config Release --parallel
ctest --test-dir build/default -C Release --output-on-failure

.\build\default\bin\Release\quanta.exe --list-modes
.\build\default\bin\Release\quanta.exe examples/queries/windows/04-dashboard-gui.q
```

GLFW собирается статически тем же компилятором и с согласованной политикой CRT;
его примеры, тесты и документация не входят в обычную сборку Quanta. Не требуются
vcpkg, Python или PowerShell 7. Драйвер OpenGL остаётся компонентом системы.
Для Debug заменить `Release` на `Debug` в командах сборки, тестирования и пути запуска.

## Другие варианты сборки

**CPU/framebuffer**, без GLFW и OpenGL:

```bash
cmake -S . -B build/framebuffer -DQUANTA_GUI_BACKEND=FRAMEBUFFER
cmake --build build/framebuffer --parallel
ctest --test-dir build/framebuffer --output-on-failure
```

На Windows к сборке и CTest добавить `--config Release` и `-C Release`.
Есть также профили `linux-framebuffer` и `windows-framebuffer`.
ImGui рисует в RGB32-буфер; показ выполняется через X11/Xlib на Linux и Win32/GDI
на Windows. Для Linux достаточно `build-essential cmake libx11-dev`.
Это не готовый драйвер `/dev/fb0`, DRM/KMS или UEFI: [границы переноса](vendor/PORTABLE_GUI.md).

**Без GUI**, со всеми метриками текущей ОС и TUI:

```bash
cmake --preset linux-no-gui
cmake --build --preset linux-no-gui --parallel
ctest --preset linux-no-gui --output-on-failure
```

Для Windows использовать `windows-no-gui`. Без профиля:
`cmake -S . -B build/no-gui -DQUANTA_WITH_GUI=OFF`.
Этот вариант не требует C++ и графических библиотек.

## Режимы и структура

```bash
quanta get os.uptime --format value
quanta get os.uptime --format csv
quanta dashboard.q --tui
quanta dashboard.q --gui
quanta dashboard.q --cli --format table --hal none
quanta --list-hals
quanta describe os.memory.available_bytes
```

CLI имеет приоритет над query. Графический backend выбирается **при сборке**, а режим
`--gui` — при запуске. Недоступный режим возвращает явную ошибку.

```text
include/quanta/               Публичный API, osal.h, hal.h, metric_def.h
core/                        Query, каталог, планировщик, значения, история
hal/ + osal/                 Аппаратные примитивы и минимальная среда исполнения
sensors/common/              Общие алгоритмы и метрики через HAL
sensors/os/{linux,windows}/   Реализации для конкретной ОС
sensors/arch/x86/             CPUID
sensors/platform/esp8266/     Метрики MCU/SDK
metrics/                     Объявления .def с типовыми обёртками
frontends/                   Текстовый вывод, TUI, общий GUI и графические backend
app/                         Единственная утилита quanta
firmware/esp8266/             SDK-компонент и serial-приложение
vendor/                      Исходники зависимостей и сведения об их происхождении
```

Все девять MCU-метрик и отдельная сборка ESP8266 сохранены:
[инструкция для SDK/платы](docs/ESP8266.md).

## Проверки и TODO

[Тестирование и ограничения проверки](docs/TESTING.md) · [Полная сборка](docs/BUILD.md) ·
[TODO](docs/TODO.md).

Linux framebuffer проверяется с настоящим окном на Xvfb; отдельно проверяются вариант
без GUI, минимальный POSIX-контракт и sanitizer-сборка. Это **не подтверждение**
нативного Windows-прогона, OpenGL-окна или прошивки ESP8266. Точный выполненный набор
и результаты приведены в отчёте, а не выводятся из наличия исходников зависимостей.

- [x] Включить GLFW 3.4 и исключить дополнительную загрузку библиотеки для Windows.
- [x] Английские комментарии и русская проектная документация с автоматическими проверками.
- [ ] Выполнить нативную Windows/OpenGL-приёмку и аппаратную проверку ESP8266.
- [ ] Реализовать запись/воспроизведение измерений, внешние источники и подписки на события.

Обычная полная сборка запускает CTest автоматически. Без Python дополнительные
Python-проверки явно пропускаются; встроенные C/C++-тесты остаются.
Распаковывать в **новую папку**, не поверх 0.7.0 и не со старым CMake cache.

## Авторы и лицензии

Исходная атрибуция **INV contributors** сохранена в [LICENSE](LICENSE).
Проект развивается в Мастерской системного программирования; индивидуальные имена
авторов добавляются участниками.

[Сторонние компоненты](THIRD_PARTY.md), [ImGui](vendor/IMGUI.md), [GLFW](vendor/GLFW.md).
Лицензия переданного portable_gui не указана: это отмечено отдельно, MIT ему
не присваивается. SDK, компиляторы, драйверы, внешние файлы шрифтов и готовые программы
в поставку не входят. Оригинальные тексты лицензий не переводились и не заменялись.
