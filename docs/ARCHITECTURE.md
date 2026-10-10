# Архитектура Quanta 0.6.0

Этот документ описывает действующее решение. `Quanta_Design.md` — неизменённый исходный
дизайн; положения о providers, отдельных программах и прежнем дереве каталогов заменены
уточнениями владельца проекта, реализованными в 0.4.0/0.5.0.

## Границы модулей

| Каталог | Ответственность | Что сюда не входит |
|---|---|---|
| `core/` | Каталог, query, план, опрос, идентичность, значения, история, live-очередь | Native collectors и UI |
| `osal/` | Runtime-контракт, stdio, часы, потоки, блокировки; общий код helpers | CPU load, working set, PDH, SMBIOS |
| `hal/` | Register, physical memory, PortIO, PCI, GPIO, affinity, info | Декодеры таблиц, метрики и query |
| `sensors/` | Конкретные методы получения метрик и чистые декодеры | Providers/fallback как выбираемая абстракция |
| `frontends/` | Файловый вывод, TUI, ImGui GUI и их backends | Алгоритмы получения измерений |
| `app/` | CLI, загрузка query, применение overrides, выбор frontend | Второй parser или дублирование scheduler |

В CMake есть три статические библиотеки: `quanta_core`, `quanta_sensors`, `quanta_frontends`.
HAL и OSAL компилируются внутрь `quanta_core`, но их исходники сохраняют понятные отдельные
каталоги. Отдельной библиотеки `quanta_query` или `quanta_hal` нет. Product executable один:
`quanta`. Тестовые executable не являются пользовательскими утилитами.

## Один include и минимальный runtime

Все публичные заголовки находятся в `include/quanta/`. Для их подключения нужен только
`-Iinclude`; ни `windows.h`, ни `pthread.h` не проникают в публичные типы. Общий приватный
`include/private/support.h` и модульные `private/` не являются частью публичного API.

**Полный перечень зависимостей — [osal.h](../include/quanta/osal.h), не скрытый список в разных
заголовках.** Переносимые исходники вызывают `quanta_*` wrappers, а не libc/native API напрямую.
Для стандартных операций используются aliases, например:

```c
#define quanta_fread  fread
#define quanta_fwrite fwrite
#define quanta_fseek  fseek
#define quanta_free   free
```

Сохраняется стандартная семантика: fread/fwrite возвращают число элементов, fseek принимает
`long`, realloc при ошибке не освобождает прежний буфер. `quanta_calloc` реализован через
malloc/memset с проверкой переполнения. ASCII-классификация query не зависит от locale.
Загрузка query и создание результата принимают UTF-8 пути; Windows fopen обёрнут
преобразованием в wide path. Остальные stdio aliases сохраняют правила CRT. Файл результата
создаётся атомарно с отказом при существующем имени; на portable POSIX это C11 `fopen(...,"wbx")`.

Контракт шире пяти функций: нужны realloc, операции строк/байтов, bounded printf и numeric
parsing, состояния потока/ошибок, элементарная floating-point math. Их aliases перечислены
в osal.h. `qsort/getenv` там отдельно отмечены как потребности отображения. Default locale
hook использует `setlocale(LC_NUMERIC,"C")`; embedding-приложение отвечает за свою locale-политику.
Нельзя применять `-ffast-math`: NaN/Inf должны обнаруживаться как некорректные данные.

Платформенная часть реализует monotonic time/sleep, optional UTC time, mutex/condition/thread
и открытие файлов. `quanta_mutex/condition/thread` содержат только непрозрачный `void *handle`.
После init/start объект имеет единственного владельца, не копируется; после destroy/join
handle обнуляется. Mutex нерекурсивный. Condition wait всегда используется в цикле проверки
предиката и возвращается с захваченным mutex. Остановка — кооперативная, без thread kill.

Обработчик Ctrl+C и process ID — **optional hooks**, не обязательные OS-примитивы минимального
порта. Portable-реализация возвращает success/0/0. Нет обязательных каталогов, sockets,
процессов, mmap, terminal/window API. Native sensors/HAL и UI-backends могут использовать их
непосредственно, не расширяя контракт core. `read_text`, `read_u64`, UTC formatting и path
helper реализованы один раз в `osal/common.c`; это не новые обязанности каждого backend.

## Core и владение данными

`core/src/query.c` содержит единственный parser. Программа сначала читает текст, разбирает
query, применяет CLI overrides, валидирует итоговый план и выбирает compiled frontend.
Проверка доступности GUI/TUI живёт в dispatcher, а не в parser ядра.

| Файл core/src | Назначение |
|---|---|
| `query.c` | Синтаксис и преобразование query в план |
| `registry.c` | Поиск, проверка дескрипторов и разрешение select |
| `metric.c` | Общие helpers метрик, low-memory single read |
| `engine.c` | Создание, владение ресурсами, завершение |
| `discovery.c` | Экземпляры, labels, появление/исчезновение устройств |
| `scheduler.c` | Планирование и общий опрос совместимых полей |
| `value.c`, `text.c`, `status.c` | Типы, unit/rate/delta, безопасный текст и статусы |
| `history.c` | Ограниченная история и последнее успешное значение |
| `live.c` | Рабочий поток, очередь событий, запрос остановки |

Core можно линковать без встроенного каталога и UI. `quanta_engine_create_with_catalog`
получает свой каталог; `quanta_engine_create` из sensors использует встроенный. Дескрипторы
каталога должны жить всё время работы engine. Engine владеет своими копиями планов, путей,
очередями и состоянием сбора; shutdown завершает worker до освобождения его данных.
Детальные ownership-правила документированы непосредственно в публичных заголовках.

Статус измерения не подменяется числом: permission denied, unsupported, warmup, no data и
другие ошибки сохраняются отдельно. Первый rate не считается успешным нулём. Значения
проверяются на тип/единицы/диапазоны. Числовая история ограничена размерами из конфигурации;
универсальное хранилище строк/bytes/blob ещё не реализовано.

## Метрики и дерево sensors

**Одно имя — один метод.** Дескриптор прямо содержит callbacks `init/enumerate/collect/read/destroy`.
Платформенное состояние принадлежит реализации и хранится в `private_data`. Нет provider name,
списка альтернативных implementations, приоритета или fallback.

Несколько полей одного native snapshot всё ещё опрашиваются совместно. Это внутреннее
объединение scheduler, не новая выбираемая сущность provider. Повторные presentation-select
одной серии не обязаны повторно читать устройство: канал получения и вид отображения разделены.

```text
sensors/
  registry.c, metric_docs.c
  common/                runtime, HAL SMBIOS, чистые decoder/normalize
  os/
    *_metrics.c          общие дескрипторы и lifecycle OS-групп
    linux/               procfs/sysfs и Linux API
    windows/             WinAPI/PDH и native helpers
  arch/x86/              CPUID + платформенные affinity adapters
  platform/esp8266/      SDK MCU и OS-поля embedded-порта
  private/               внутренние snapshot-типы и объявления
```

Декларации `metrics/*.def` задают key, type, unit, callbacks, политику обновления и документацию.
Типовые initializer helpers находятся в `include/quanta/metric_def.h`: [METRICS](METRICS.md).
Они не добавляют runtime/генератор и сохраняют исходные поля дескриптора.
`selected.def` формирует каталог только из включённых групп. Документация метрик —
[metrics/INDEX.md](metrics/INDEX.md). `sensors/metric_docs.c` генерируется из этих Markdown-файлов.

Общие `quanta.version`, `hal.name`, `hal.capabilities`, `hal.logical_cpus` не требуют native
OS acquisition. Неизвестное число CPU даёт UNSUPPORTED, а не фиктивный ноль. SMBIOS decoder
и поиск через HAL read_mem находятся в common; native SMBIOS-метод — в OS-каталоге, с другим key.
Подробнее о реальных ограничениях — [HAL](HAL.md).

## Headless, GUI/TUI и ESP

`portable-posix` — рабочий эталон минимального порта: stdio + clock_gettime/nanosleep + pthread,
без native OS sensors. Включены общие метрики, file frontend, весь query/core и test fixtures.
Не используется Linux-доступ к /proc, /sys, устройствам и терминалу. Это подтверждает границу
переносимости, но не предоставляет OS-specific метрик там, где нет соответствующего OS API.

GUI/TUI выбираются в единой программе; native-сборка по умолчанию требует оба (`ON`).
`AUTO` явно разрешает сокращённый набор при отсутствии зависимостей, `ON` требует зависимость,
`OFF` исключает её. ImGui имеет собственные view, software renderer и Win32/XCB window backend;
TUI имеет общий код и POSIX/Windows terminal backend. Core не требует C++ и не открывает окна.
Сборка без UI не требует их headers/libraries. Если GUI включён при сборке, его link-time
зависимости остаются у executable и при запуске с `--cli`; это не динамические плагины. `--list-modes` показывает результат конфигурации.

Dear ImGui **1.92.9b** встроен как неизменённые исходники в `vendor/imgui`. C++ включается
в верхнем CMake scope при GUI, а не только внутри `frontends`: так C++ runtime доходит
до sibling app/tests. `imgui_vendor` — дополнительная STATIC зависимость GUI, не часть core.
Renderer владеет копиями Alpha8/RGBA32 текстур и обслуживает create/update/destroy протокол
1.92; рост атласа не оставляет dangling pointer. Renderer shutdown выполняется до
уничтожения ImGui context. Стандартные sampler/reset callbacks и VtxOffset обрабатываются
программно. Native backends передают события ввода и показывают framebuffer.

ESP SDK-компонент берёт файлы и публичные размеры из одного `cmake/esp8266_sources.cmake`.
Host SDK-stubs находятся только в тестах. На плате используются настоящие SDK headers/calls.
Serial app использует `quanta_metric_read_once`, не создавая большой UI engine; полный core
и query остаются доступными. [ESP8266](ESP8266.md).

## Правила изменений

C11, UTF-8/LF, четыре пробела, snake_case, публичный префикс `quanta_`; C++ только в GUI.
`.clang-format` задаёт Allman и ориентир 100 колонок. Header сам включает нужные public headers.
Платформенный API не добавляется в support.h ради удобства одного collector.

Проверять overflow до умножения, capacity до записи, статус до публикации. Cleanup должен
работать после частичного init. Комментировать владение, конкурентный доступ и причины
ограничений, а не переводить каждую строку. Исправление ошибки сопровождается fixture;
проверки не должны исчезать под NDEBUG. Порядок тестов — [TESTING](TESTING.md).
