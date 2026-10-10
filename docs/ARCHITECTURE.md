# Архитектура Quanta 0.3.0

Основание: неизменённый [Quanta_Design.md](Quanta_Design.md) и предоставленная схема.
Этот документ описывает фактическое устройство baseline, а не подменяет целевой дизайн.

## Границы модулей и сборочные targets

| Target / слой | Ответственность | Зависит от |
|---|---|---|
| `quanta_core` STATIC, C11 | catalog, query, lifecycle, discovery, scheduler, values, numeric history, live queue | стандартная C library, OSAL; на Linux threads/libm |
| `quanta_sensors` STATIC, C11 | builtin registry, providers, metric docs, native snapshots | `quanta_core`; выбранные HAL/native OS libraries |
| `quanta_output` STATIC, C11 | форматы, presentation helpers, file sink | `quanta_core` |
| `quanta_tui` STATIC, C11 | терминал и управление представлением | `quanta_output` |
| `quanta_gui` STATIC, C++ adapter | ImGui, software raster, native window | `quanta_output`, optional ImGui, XCB или Win32 |
| четыре CLI executables | аргументы, композиция каталога, engine и frontend | sensors, output, выбранные TUI/GUI |

OSAL имеет отдельные исходники, но входит в target core: это не отдельная библиотека ради
одного слоя папок. Аналогично HAL компилируется в sensors. Циклических CMake-зависимостей нет.
`query/src/query.c` физически выделен, но остаётся частью ядра.

## Core по файлам

| Файл | Содержание |
|---|---|
| `core/src/registry.c` | валидация immutable catalog, lookup, default/explicit provider selection |
| `core/src/provider.c` | generic provider helpers, discovery singleton |
| `core/src/engine.c` | создание, владение планом/paths, cleanup, управление, explain |
| `core/src/discovery.c` | экземпляры, labels, channels, bindings, появление/исчезновение |
| `core/src/scheduler.c` | due-time, batch refresh, sample emission, разные интервалы представлений |
| `core/src/value.c` | проверка typed values, numeric conversion, units, rate/delta |
| `core/src/history.c` | last/last-good и ограниченные numeric ring buffers |
| `core/src/live.c` | worker и очередь событий для интерактивного frontend |
| `core/src/text.c`, `status.c` | bounded strings/identity и имена статусов/типов |
| `query/src/query.c` | синтаксис и semantic validation запросов |

`core/include/quanta/` — устанавливаемый API. `core/private/` — внутреннее состояние,
не устанавливается. Публичные заголовки не включают WinAPI, pthread, HAL или UI.

## Сущности и поток данных

`quanta_metric_desc` содержит имя, описание, doc_path, type, unit, refresh policy,
минимальный/рекомендуемый период, labels, flags и provider. `quanta_provider_desc` задаёт
`open → discover → refresh → close`; read-функция дескриптора извлекает поле из batch snapshot.

`quanta_instance` — обнаруженный экземпляр с entity_key и labels. `quanta_channel` — единственный
канал получения конкретного descriptor/entity. `quanta_series`/binding — отдельное представление
канала, например raw-value и plot или rate. Эти сущности намеренно не отождествлены.

Два `select` одной метрики разделяют acquisition channel. Все due channels одного provider
используют один `refresh` на проход scheduler. Rate-baseline находится у binding: медленный
вывод не использует чужую, более частую разность счётчика. Другие независимые engine не разделяют
кеш: межклиентский глобальный scheduler остаётся TODO.

`quanta_sample` несёт значение, статус, quality, sequence и времена. Значение без успешного
статуса не становится успешным нулём. CPU/rate сначала выдают `WARMUP`; counter reset не превращается
в огромное положительное значение. Frontend получает только sink events и metadata, не вызывает HAL.

## Provider selection

Каталог — пользовательский immutable view `quanta_catalog`, а не встроенный глобальный registry.
Несколько descriptors могут иметь одно имя, если совместимы по типу/единицам/kind/labels/версии
семантики и различаются provider. Default — первый descriptor соответствующего ключа.
Явный выбор: `provider("id", metric{labels})`. **Автоматического переключения при ошибке пока нет.**

`quanta_engine_create_with_catalog()` и `quanta_plan_validate_with_catalog()` не требуют sensors.
Convenience API `quanta_engine_create()`, `quanta_registry()` и builtin catalog объявлены
в `sensors/include/quanta/sensors.h` и реализованы в sensors. Тест `test_catalog.c` показывает
полный mock provider и связывается только с `quanta_core`.

## HAL и OSAL

HAL принимает `quanta_hal_context`: borrowed `quanta_paths` и необязательный error-buffer с capacity.
Он не получает `quanta_read_ctx`, query, catalog или frontend object. Providers адаптируют
контекст и преобразуют `quanta_cpu_snapshot`, `quanta_memory_snapshot`, `quanta_network_snapshot`
и другие типизированные данные в метрики. Сырые native структуры не протекают в публичный API core.

HAL здесь — слой acquisition из native API/procfs/sysfs, не обещание доступа ко всем регистрам
процессора. OSAL — время, файлы, сигналы остановки, потоки/locks/conditions, affinity.
Общий allocator abstraction и внешние драйверы пока не реализованы.

## Владение и concurrency

Engine копирует plan и строки paths. Catalog view, массив указателей, descriptors/provider descriptors
и их строки должны жить дольше engine и сохранённых моделей. Это не deep-copy всего каталога.
Аргументы sink callback заимствуются на время вызова; копия series сохраняет borrowed metric pointer.

Один engine запускается один раз. `stop`/`refresh` можно вызывать параллельно с `run`;
другие операции сериализует владелец. Нельзя destroy/run рекурсивно из callback.
Ошибки callback завершают сбор. После попытки open cleanup вызывается также для partial allocation;
close обязан переносить NULL/частичное состояние. Неоткрытые providers не закрываются.

Live worker заимствует engine и plan до join. Очередь bounded, 256 событий, с backpressure;
она не теряет события при обычном pump. `finish(stop=0)` разрешён только после `done()`.
`finish(stop=1)` будит очередь, отменяет и join-ит worker; невостребованные события при отмене
могут быть отброшены. После ошибки pump нужен `finish(stop=1)`.

## History и ограничения

History — часть модели ядра, которую может подключить любой sink/frontend. Выделяется для plot,
а не каждого датчика: максимум 8192 numeric points на серию, общий бюджет history allocations
64 MiB на модель. Сам массив items/engine/очередь в этот budget не входят. Есть pruning по времени,
сохранение last-good, deep clone и безопасный resize с откатом при нехватке памяти.

Это **не** универсальный time-series store: string/bytes history, disk persistence и replay отсутствуют.
Диапазон `history` ограничен 24 часами; selectors — 128; providers — 32; series — 2048;
channels — 4096; query text — 65536 bytes. Изменение ABI capacity macros должно быть одинаковым
для библиотеки и её потребителей; нельзя менять их только в одном translation unit.

## Отличия от полного дизайна

`value` расширен unsigned integer и bool относительно примера дизайна. Строка — bounded
NUL-terminated array, а не length+array; bytes имеют length. Status/quality находятся в sample,
а не внутри value. Эти решения фиксируются как различия ABI, не выдаются за буквальное совпадение.

ON_EVENT присутствует как policy, но полноценного event subscription backend нет. EXPENSIVE/SENSITIVE
и quality metadata есть, но cost-budget planner ещё не реализован. Saved blob/File/Pipe sources,
UART/JTAG и автоматический fallback — отдельные пункты [TODO](TODO.md).
