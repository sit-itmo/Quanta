# Архитектура 0.4.0: метрики напрямую, HAL отдельно

## Статус решения

Это реализация уточнения владельца проекта после 0.3.0. Исходный `Quanta_Design.md` сохранён
побайтно как исходный документ, но его раздел 6, `provider()` в разделе 8 и provider selection
больше не описывают текущий API. Ниже — действующий контракт.

## Разделение ответственности

| Слой | Содержит | Не содержит |
|---|---|---|
| HAL | register/memory/PortIO/PCI/GPIO/affinity и capabilities | SMBIOS/ACPI decoder, CPU utilization, process working set, query |
| OSAL | часы, sleep, threads, mutex/condition, file helpers, инфраструктура | Каталог OS-метрик, PDH, чтение загрузки CPU |
| sensors | Дескрипторы, lifecycle/read, native acquisition и декодирование таблиц | Выбираемые providers и fallback |
| core | Registry, plan, scheduler, identity, value/history, live queue | Конкретные CPU/SDK/WinAPI collectors, отрисовка |
| frontends | Ввод query, форматирование/вывод и UI adapters | Алгоритм аппаратного измерения |

Некоторые display/file adapters остаются в `frontends/`, а не искусственно переносятся в OSAL.
Платформенный SDK или WinAPI вызывается в реализации метрики непосредственно, когда HAL не нужен.
Например, `process.self.*` остался как существующая функциональность; набор новых OS-метрик
ради новой архитектуры не добавлялся.

## Метрика — одна реализация

`quanta_metric_desc` содержит имя, тип, единицу, политику/интервалы, labels, флаги и прямые callbacks:
`init`, `enumerate`, `collect`, `destroy`, `read`. `arg` выбирает поле общего снимка.
Глобально повторяющееся имя недопустимо даже с другими callbacks: registry возвращает ошибку.

`dependencies` — информационный текст о нужном API/ресурсе. Это не ID, не объект выбора и не
приоритет. Нет `provider.h`, `quanta_provider_desc`, `.provider`, списка candidates или fallback.

Другой алгоритм — другое имя. Реальный пример текущей реализации:

- `board.model`: Linux sysfs / Windows firmware API в native-файлах метрики;
- `smbios.legacy_mem.board.model`: decoder в sensors читает legacy SMBIOS через HAL `read_mem`.

Эти имена существуют одновременно, не являются aliases, не подменяют друг друга после ошибки.
Существующие переносимые имена сохранены: платформенные Linux/Windows варианты компилируются
взаимоисключающе, а не выбираются во время исполнения как разные providers.

## Как не потерять batch acquisition

Наивное удаление providers привело бы к отдельному системному чтению на каждое поле. Здесь scheduler
приватно группирует выбранные descriptors с одинаковым набором lifecycle callbacks
`init/enumerate/collect/destroy`, если есть `init` или `collect`.

Вместо публичного provider существует внутренний `quanta_acquisition` — исключительно механизм
владения снимком/планирования. У него нет имени, метода выбора, приоритета и альтернатив.
`read` и `arg` могут различаться: так разные поля получают общий снимок.

Callbacks должны быть пригодны для совместного состояния при совпадающем lifecycle tuple.
Нужны независимые состояния — используйте разные lifecycle wrappers. Это явный контракт,
а не автоматическое угадывание совместимости по строке источника.

`quanta_metric_context` даёт только выбранные descriptors этой группы, `paths`, заимствованный
`quanta_hal *`, время и `void *private_data`. Полный query/registry/UI в native acquisition не
передаётся. Типизация платформенного состояния выполняется в private headers реализации метрики.

## Core по файлам

| Файл | Ответственность |
|---|---|
| `registry.c` | Уникальность/валидность descriptors и поиск |
| `metric.c` | Сопоставление selectors, простая discovery, low-memory `read_once` |
| `engine.c` | Создание, владение plan/paths/HAL, группировка и cleanup |
| `discovery.c` | Instances, labels, identity/generation, добавление/исчезновение |
| `scheduler.c` | Polling, refresh, batch, публикация samples |
| `value.c` | Валидация, преобразования единиц и значений |
| `history.c` | Ограниченное хранение, last/last-good, rate/delta |
| `live.c` | Worker и ограниченная очередь для UI |
| `query/src/query.c` | Parser и проверка plan |

Одна серия acquisition может обслуживать несколько presentation bindings, например value+plot.
У каждого binding собственная presentation metadata; из-за второго представления аппаратное
чтение не удваивается.

## HAL selection и владение

Для query действует один HAL: `hal "name";`. CLI `--hal name` переопределяет его; повторяющиеся
statements/CLI flags запрещены. Выбор задаёт доступные аппаратные операции, а не метод OS-метрики.
`hal "none";` не запрещает прямые обращения native метрик к ОС/SDK.

`quanta_engine_create_with_options` допускает заимствованный, заранее созданный HAL. Он должен
жить дольше engine. Непустое `plan.hal` обязано совпадать с **каноническим** `info.name`; для
инъекции проще оставить `plan.hal` пустым. Alias `native` не угадывается при сравнении с внешним
handle. Без инъекции engine сам открывает read-only HAL и закрывает его после destroy метрик.

Plan и строки paths копируются; catalogue/descriptors заимствуются и обязаны пережить engine.
`destroy` вызывается и после частичного `init`; реализации обязаны переносить NULL/partial state.
Engine выполняется один раз; только stop/refresh допускаются параллельно run. Освобождение —
после join worker. HAL handles и affinity tokens требуют внешней сериализации.

## Embedded

`firmware/esp8266/components/quanta` собирает тот же core/query, MCU/OS metric code и HAL/OSAL
через `cmake/esp8266_sources.cmake`. Этот manifest также используется host SDK-seam тестами.
Размеры публичных массивов заданы **PUBLIC compile definitions**, поэтому app и библиотека
не получают разные ABI.

Serial app использует `quanta_metric_read_once`: независимый init/collect/read/destroy для одного
неразмеченного descriptor, без большого desktop plan и UI queue. Query/engine остаются доступны
для встраивания, но serial app не превращён в shell и не обещает UART transport desktop-клиента.
Ограничения и таблица переноса — [ESP8266](ESP8266.md).
