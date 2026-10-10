# Quanta 0.4.0

<p align="center"><img src="docs/assets/logo.jpg" width="280" alt="Quanta"></p>

**Сбор, обработка и отображение аппаратных и системных метрик на C.**
Проект Мастерской системного программирования. Основные desktop-платформы — Linux и Windows;
активный embedded-порт — **ESP8266 / ESP8266_RTOS_SDK / FreeRTOS**.

## Цель и архитектура

Аппаратные метрики используют минимальные примитивы HAL. Код метрик, которым аппаратный
доступ не нужен, обращается к CPUID, WinAPI, procfs/sysfs или SDK непосредственно.
OSAL предоставляет инфраструктуру: время, потоки, синхронизацию и файлы.
Frontend не знает, как получено значение.

**Одно имя метрики — одна реализация.** Выбираемой сущности provider больше нет.
Другой метод измерения получает другое имя; HAL выбирается отдельно, один на query/engine.

```text
CLI / CSV / TXT / TUI / optional ImGui
                 |
       Core: registry, query, scheduler,
             value, history, live queue
                 |
       Прямые реализации метрик (sensors/)
             /                   \
 hardware primitives HAL     Native API / CPUID / SDK

OSAL: инфраструктура core и frontends, не каталог системных метрик.
```

[Исходный дизайн](docs/Quanta_Design.md) сохранён без изменения.
**[Архитектурное решение 0.4.0](docs/ARCHITECTURE.md) заменяет его положения о providers**
и уточняет границы HAL/OSAL. [Исходная схема](docs/assets/architecture-original.jpg):

<img src="docs/assets/architecture-original.jpg" width="850" alt="Исходная архитектурная схема Quanta">

## Что есть в этой ревизии

Прямой каталог метрик, query, групповой опрос совместимых полей, bounded history, rate/delta,
labels и обновление устройств. Четыре executable — `quanta`, `quanta_cli`, `quanta_csv`,
`quanta_txt`; TUI включён по умолчанию, GUI опционален.

Низкоуровневый HAL: регистры, физическая память, PortIO, PCI, GPIO, affinity, capability/info.
Доступность зависит от backend; неподдерживаемые операции не имитируются.
Все штатные метрики работают без hardware writes; CLI не разрешает такие записи.

ESP8266: сохранены **все 9 `mcu.*` метрик**, OS-метрики порта, компонент SDK, serial-приложение
и FreeRTOS OSAL. Это активные файлы сборки, а не только исторический ZIP.
Имеются host-тесты того же source manifest с подменой SDK; реальная Xtensa-сборка и плата
требуют отдельной приёмки. [Подробности ESP8266](docs/ESP8266.md).

## Быстрый запуск: Linux

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build python3
cmake --preset linux-default
cmake --build --preset linux-default --parallel
ctest --preset linux-default --output-on-failure
./build/linux-default/bin/quanta --version
./build/linux-default/bin/quanta --list-hals
./build/linux-default/bin/quanta hal-info --hal none
./build/linux-default/bin/quanta get os.name os.arch --hal none
```

Полная нативная сборка по умолчанию сама запускает CTest; ошибка теста прерывает сборку.
Python добавляет integration/documentation-проверки. Без Python C-тесты остаются, но набор
проверок неполон и CMake явно сообщает об этом.

## Быстрый запуск: Windows

Visual Studio 2022 с C++ build tools и Windows SDK, CMake 3.21+, Python 3; PowerShell:

```powershell
cmake --preset windows-native
cmake --build --preset windows-native --parallel
ctest --preset windows-native --output-on-failure
.\build\windows-native\bin\Release\quanta.exe get os.name os.arch --hal none
```

Native Windows CI настроен, но в отчёте этой поставки его запуск не заявляется.
[Полная инструкция Linux/Windows/GUI/static](docs/BUILD.md).

## HAL и query

```text
version 1;
hal "none";
once;
select os.name;
select os.arch;
```

`--hal NAME` имеет приоритет над `hal "NAME";` в query. `native` означает непривилегированный
backend текущей платформы; `none` отключает аппаратные примитивы, но не метрики с native API.
Неизвестный HAL — ошибка, не тихий переход на другой способ.

Пример двух разных методов: `board.model` (native) и
`smbios.legacy_mem.board.model` (legacy SMBIOS через HAL `read_mem`). Второй метод ограничен
legacy-окном firmware и не является универсальным UEFI locator. [HAL](docs/HAL.md), [query](docs/QUERY.md).

## Дерево исходников

```text
core/        Публичный C API, private structures, registry/scheduler/history
query/       Parser и валидация
sensors/     Прямые реализации метрик и их native/platform acquisition
metrics/     Дескрипторы .def и выбор набора на платформе
hal/         Только низкоуровневые аппаратные примитивы
osal/        Инфраструктура Linux, Windows, FreeRTOS/POSIX threads
frontends/   CLI, форматы вывода, TUI, optional GUI
firmware/    Активное ESP8266 SDK-приложение и SDK component
cmake/       Явные списки, опции компиляции, единый ESP source/ABI manifest
examples/    Query-примеры
tests/       C/CLI/SDK-seam тесты, без опасных аппаратных записей
docs/        Архитектура, миграция, метрики, аудит и результаты
```

[Полное дерево](docs/SOURCE_TREE.txt), [карта переноса](docs/audit/SOURCE_MAP.json),
[сравнение исходных архивов](docs/AUDIT.md).

## TODO

- [ ] Реальная сборка ESP8266 SDK + flash/serial/heap/stack приёмка; native Windows и GUI.
- [ ] Windows driver HAL, Linux GPIO, прочие регистры и более полная карта физических диапазонов.
- [ ] UEFI SMBIOS/ACPI locator и method-specific аппаратные метрики.
- [ ] Blob record/replay, File/Pipe/UART/JTAG acquisition, event subscriptions, cost budget.

Разделение по приоритетам и критерии готовности — в [TODO](docs/TODO.md).
Provider fallback не является TODO: его удаление — осознанное архитектурное решение.

## Проверки и ограничения

[TEST_REPORT](docs/TEST_REPORT.md) разделяет выполненные Linux/ASan/SDK-seam проверки и
непроведённые аппаратные испытания. В архиве есть исходные логи. Host SDK stubs не входят в
firmware и не доказывают линковку с настоящим SDK. Физический HAL тестируется на собственных
RAM/file fixtures, не на реальных `/dev/mem`, MSR или PCI устройствах.

Версия 0.4.0 меняет C API и query с `provider()`. Это **полный архив для новой папки**, не overlay.
[Миграция](docs/MIGRATION.md), [безопасность](docs/SECURITY.md), [стиль C](docs/CODING_STYLE.md).

## Авторы и лицензия

Исходная авторская атрибуция: **INV contributors**, сохранена в [MIT LICENSE](LICENSE).
Проект Quanta развивается в Мастерской системного программирования. Имена индивидуальных
авторов/maintainers в предоставленных исходниках не установлены; их следует добавить после
подтверждения участниками. Не приписываем неподтверждённое авторство.

Сторонние SDK/ImGui/toolchains имеют собственные условия — [THIRD_PARTY](THIRD_PARTY.md).
SDK, драйверы, библиотеки ImGui и файлы шрифтов в архив не включены.
