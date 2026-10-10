# Quanta 0.5.0

<p align="center"><img src="docs/assets/logo.jpg" width="280" alt="Quanta"></p>

**Сбор, обработка и отображение аппаратных и системных метрик на C.**
Проект Мастерской системного программирования: Linux, Windows и активный порт ESP8266/FreeRTOS.

## Цель проекта

Единый query, каталог и scheduler отделяют получение измерений от их отображения.
Аппаратные метрики используют минимальный HAL; другие реализации могут непосредственно
обращаться к API ОС, CPUID или SDK. **Одно имя метрики — один метод получения**, без выбираемых
providers и автоматической подмены метода. HAL выбирается независимо.

**Один исполняемый файл — `quanta`.** Текст, CSV, Markdown, value, TUI и Dear ImGui GUI —
режимы этой программы, а не отдельные копии executable. GUI/TUI включаются в `AUTO`, когда
доступны их зависимости. CMake ничего не скачивает; запрос недоступного режима — явная ошибка.

```text
quanta: CLI options / query file
                 |
          frontend dispatcher
       file / TUI / ImGui GUI
                 |
      core: query, registry, scheduler,
            value, history, live
                 |
      metric implementations (sensors)
         /                      \
    hardware HAL          native OS / CPUID / SDK

OSAL: общий контракт runtime, файлов, часов, потоков и блокировок.
GUI/TUI: собственные платформенные backends вне переносимого ядра.
```

Действующее описание — **[ARCHITECTURE](docs/ARCHITECTURE.md)**;
точный runtime-контракт — **[include/quanta/osal.h](include/quanta/osal.h)**.
[Исходный дизайн](docs/Quanta_Design.md) сохранён без изменения как историческая основа:
его providers, отдельные executable и дерево include не описывают текущую реализацию.
[Предоставленная исходная схема](docs/assets/architecture-original.jpg) также сохранена как справочная.

## Быстрый запуск

Linux / Ubuntu, из новой распакованной папки:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3
cmake --preset linux-default
cmake --build --preset linux-default --parallel
ctest --preset linux-default --output-on-failure
./build/linux-default/bin/quanta --list-modes
./build/linux-default/bin/quanta get os.name os.arch --hal none
./build/linux-default/bin/quanta get os.uptime --format csv
```

Windows: Visual Studio 2022 с C++ build tools и Windows SDK, CMake и Python в PATH.

```powershell
cmake --preset windows-native
cmake --build --preset windows-native --parallel
ctest --preset windows-native --output-on-failure
.\build\windows-native\bin\Release\quanta.exe get os.name os.arch --hal none
```

Обычная полная нативная сборка **сама запускает CTest**. Ошибка теста прерывает сборку.
Для тестовой конфигурации Python обязателен: наборы не пропускаются молча.
Сборка одного target `quanta` не запускает все тесты; для этого есть target `check`.
[Полные инструкции, GUI, presets, установка](docs/BUILD.md).

## Query и режимы

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
quanta dashboard.q --tui
quanta dashboard.q --gui
quanta dashboard.q --cli --format csv --output report.csv
quanta dashboard.q --hal native
quanta --list-hals
quanta hal-info --hal none
```

CLI имеет приоритет над соответствующей директивой query. `mode cli` и `mode file` — один
неинтерактивный frontend. `hal none` отключает аппаратные примитивы, но не native OS-метрики.
Выходной файл создаётся без перезаписи существующего. [Query и форматы](docs/QUERY.md),
[HAL и ограничения аппаратного доступа](docs/HAL.md).

## Переносимая конфигурация и ESP8266

`portable-posix` проверяет работу без native-метрик, GUI/TUI и Linux API: только stdio/C runtime,
POSIX часы/pthreads как пример реализации OSAL, HAL без аппаратных возможностей и общие метрики.
Query, scheduler, история, потоки и файловый frontend остаются настоящими, не заглушками.

```bash
cmake --preset portable-posix
cmake --build --preset portable-posix --parallel
./build/portable-posix/bin/quanta examples/queries/00-portable.q
```

Это **не обещание получить Windows working set или Linux CPU load только через fread**:
такие метрики остаются в соответствующих OS-папках. Для собственного runtime реализуется
контракт `osal.h` и, при необходимости, HAL. Это ещё не готовый noOS/ESP32-порт.

ESP8266: все девять `mcu.*` ключей, OS-метрики, serial-приложение и SDK-компонент сохранены в
активной сборке. SDK source/ABI manifest общий для firmware и host-тестов.
Файловая система платы в примере не подключена; VDD33 выключен по умолчанию.
[Сборка, ограничения и аппаратная приёмка ESP8266](docs/ESP8266.md).

## Дерево исходников

```text
include/quanta/         Единственный публичный C API, включая osal.h и hal.h
include/private/        Общий приватный support.h; не устанавливается как public API
core/src/               Query, registry, scheduler, history, live; core/private/ — внутренние типы
osal/                   Общие stdio helpers; Linux, Windows, POSIX, FreeRTOS, portable
hal/                    Общий wrapper и аппаратные backends
sensors/common/         HAL-метрики и чистые общие алгоритмы
sensors/os/             Дескрипторы OS-групп; linux/ и windows/ — native-реализации
sensors/arch/x86/        CPUID и архитектурно-зависимые части
sensors/platform/esp8266/ SDK-метрики MCU и OS-поля платформы
metrics/                Декларации .def, не ещё один слой providers
app/                    Единственный main и обработка аргументов
frontends/              Общий файловый вывод, TUI и GUI с отдельными backends
firmware/esp8266/        SDK-компонент и serial-приложение
cmake/                  Небольшие build helpers и ESP manifest
examples/queries/        Рабочие примеры query
tests/                 unit/, platform/, integration/ и проверки исходной структуры
docs/                  Текущие руководства и документация метрик
```

## TODO и состояние проверок

- [ ] Нативная Windows-приёмка, настоящая SDK/Xtensa-сборка и плата ESP8266, GUI-приёмка.
- [ ] Windows driver HAL, Linux GPIO, более полное обнаружение SMBIOS/ACPI.
- [ ] Blob record/replay, File/Pipe/UART/JTAG acquisition, event subscriptions и poll budget.

[Приоритеты и критерии готовности](docs/TODO.md). Providers и отдельные product executables
не являются отложенной функциональностью: они намеренно удалены.
[TESTING](docs/TESTING.md) содержит организацию тестов, результаты этой ревизии и границы проверки.

Архив предназначен **для новой папки и полной пересборки**. В 0.5.0 перемещены заголовки,
изменён ABI непрозрачных объектов синхронизации, удалены копии `quanta_cli/csv/txt`.
Старые бинарные объекты, CMake cache и ESP ABI-настройки с новым деревом не смешивать.

## Авторы и лицензия

Исходная авторская атрибуция **INV contributors** сохранена в [MIT LICENSE](LICENSE).
Quanta развивается в Мастерской системного программирования. Имена отдельных авторов и
сопровождающих в переданных материалах не указаны; персональные имена следует добавить
после подтверждения участниками.

[Сторонние компоненты](THIRD_PARTY.md): SDK, toolchain и ImGui распространяются отдельно.
В архиве нет драйверов, SDK, исходников ImGui или файлов шрифтов.
