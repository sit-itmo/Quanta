# Объявление и реализация метрик

Дескрипторы находятся в `metrics/*.def` и `metrics/windows/*.def`. Реализации — в
`sensors/common`, `sensors/os/{linux,windows}`, `sensors/arch/x86`, `sensors/platform/esp8266`.
Общие helpers объявлены в **[include/quanta/metric_def.h](../include/quanta/metric_def.h)**.
Это обычные C11 initializer macros, без генерации исходников и без нового runtime-слоя.

## Типовая запись

```c
QUANTA_METRIC(os_memory_total_bytes,
    QUANTA_META("os.memory.total_bytes", "Physical memory"),
    QUANTA_GAUGE_BYTES(250),
    QUANTA_SNAPSHOT(quanta_memory, "total"),
    QUANTA_SOURCE("", "/proc/meminfo"))
```

Запись разворачивается в прежний `quanta_metric_desc`. `QUANTA_META` задаёт имя, короткую
подпись, описание (по умолчанию совпадает с подписью), semantic version=1 и путь
`docs/metrics/os.memory.total_bytes.md`. Другие формы:

```c
QUANTA_META_DESC("name", "Title", "A longer description")
QUANTA_META_VERSION("name", "Title", "Description", 2)
```

Query name передаётся **явным строковым литералом**: он нужен для автоматического пути
и проверки документации. Собственные нестандартные пути допускаются через исходные
поля `.key/.title/.description/.doc_path/.semantic_version`, без `QUANTA_META`.

## Шаблоны значений

| Helper | Тип и поведение |
|---|---|
| `QUANTA_PROPERTY_TEXT(stale_ms)` | STRING, PROPERTY, ONCE |
| `QUANTA_PROPERTY_BOOL(stale_ms)` | BOOL, PROPERTY, ONCE |
| `QUANTA_PROPERTY_U64(unit, stale_ms)` | U64, PROPERTY, ONCE |
| `QUANTA_GAUGE_BYTES(min_ms)` | U64, gauge в байтах |
| `QUANTA_GAUGE_RATIO(min_ms)` | F64, gauge с unit `1` |
| `QUANTA_GAUGE_SECONDS(min_ms)` | F64, gauge в секундах |
| `QUANTA_GAUGE_U64(unit, min_ms)` | U64, polling gauge |
| `QUANTA_GAUGE_F64(unit, min_ms)` | F64, polling gauge |
| `QUANTA_COUNTER_BYTES(min_ms)` | U64, 64-битный счётчик байтов |
| `QUANTA_COUNTER_U64(unit, min_ms)` | U64, 64-битный счётчик выбранной единицы |

У polling templates рекомендуемый интервал **1000 ms**, stale deadline **5000 ms**;
минимальный допустимый интервал задаётся явно. У property minimum=0, recommended=1000 ms,
а stale задаётся аргументом; 0 означает отсутствие автоматического deadline. Шаблоны
не меняют значение измерения: `GAUGE_RATIO` не делит проценты на 100, `GAUGE_BYTES` не
умножает KiB на 1024. Преобразование выполняет реализация метрики.

Для другого контракта используются независимые низкоуровневые helpers:

```c
QUANTA_VALUE(QUANTA_F64, QUANTA_GAUGE, QUANTA_REFRESH_EVENT, "C"),
QUANTA_TIMING_MS(500, 2000, 10000)
```

Это описание политики, а не реализация подписки на события: callback/планировщик должны
реально поддерживать нужный механизм. Поля `.flags`, `.counter_bits` и прочие нестандартные
свойства остаются явными. Флаги качества и side-effect metadata не подставляются скрыто.

## Привязка реализации

```c
QUANTA_SNAPSHOT(quanta_memory, "total")
```

Разворачивается в `.init=quanta_memory_init`, `.enumerate=quanta_memory_enumerate`,
`.collect=quanta_memory_collect`, `.read=quanta_memory_read`,
`.destroy=quanta_memory_destroy`, `.arg="total"`. Совместимые поля по-прежнему используют
один snapshot. Helper не выбирает provider или fallback и не вводит такую абстракцию.

`QUANTA_INIT_SNAPSHOT(prefix, field)` — тот же набор **без collect**, например для
неизменяемого CPUID snapshot, сформированного при init.

`QUANTA_SINGLE_READ(callback, field)` — один экземпляр через `quanta_discover_single`
и прямой read callback без init/collect/destroy. Так объявлены общие runtime/HAL-info метрики в `runtime.def`.

`QUANTA_SOURCE(labels, dependencies)` задаёт только `.label_schema/.dependencies`.
Он ничего не открывает, не проверяет разрешения ОС и не регистрирует новый источник.

**Один helper владеет одним набором полей. Не переопределяйте эти же поля далее в
инициализаторе**: такое поведение непереносимо между предупреждениями/режимами компиляторов.
Для нестандартного поля выбирайте низкоуровневую форму вместо типового helper. Исходная
развёрнутая запись `QUANTA_METRIC(id, .key=..., .type=..., ...)` остаётся допустимой.

## Порядок добавления

1. Выбрать уникальное method-specific имя и подходящую группу. Другой способ измерения
   того же показателя получает другое имя, а не альтернативный provider.
2. Реализовать callbacks в своей OS/arch/platform-папке либо через HAL в common.
   HAL — аппаратные примитивы, OSAL — инфраструктура; не переносить collectors в эти слои.
3. Добавить запись `.def`, описание `docs/metrics/<name>.md`, подключение новой группы
   к registry/CMake, если группа ранее отсутствовала. Проверить диапазоны, единицы,
   labels, ошибки доступа и semantic version при изменении значения существующего ключа.
4. Обновить встроенные тексты: `python3 tools/embed_docs.py` (на Windows `python ...`).
   Добавить unit/fixture тесты и запустить обычную CMake-сборку с CTest.

Для новой метрики SDK используйте тот же ABI/source manifest, что firmware и host tests.
`mcu.power.vdd_mv` и `mcu.chip_id` сохранили semantic version=2; helper с версией 1 здесь
не должен применяться по невнимательности.

## Проверка перехода 0.5.0 → 0.6.0

Переведены все **137 объявлений в 21 `.def`**. Скомпилированное сравнение до/после для
Linux, Windows и MCU дало **3014 совпадающих полей, 0 расхождений**, включая callbacks,
units, timing, flags, labels, источники и semantic versions. Имена/контракт метрик не
изменены ради сокращения файла. `metric_def` — отдельный core-only regression suite.
