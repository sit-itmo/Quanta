# TODO: достижение целевого дизайна

Обозначения: P0 — приёмка/надёжность baseline; P1 — недостающая функциональность дизайна;
P2 — расширение. Список открытых работ, а не скрытые заглушки с успешным статусом.

## P0 — приёмка

| Работа | Что считать готовностью |
|---|---|
| Native Windows | MSVC x64 Debug/Release, full/minimal, native CTest; логи на Windows 10/11, обычный пользователь |
| Windows edge cases | >64 logical CPUs / groups, локализованный PDH, недоступные counters, NIC hotplug, нет firmware table |
| GUI | dependency provision, compilation и окно XCB/Win32; input/resize/close/redraw/font; leaks/raster smoke |
| Расширить negative testing | allocator-failure injection по всем core/provider путям, fuzz discovery/catalog и query с coverage |
| Проверка границ HAL | договориться о timeout/cancel для каждого potentially blocking native read; не только общий stop flag |
| Отсутствие провайдера | проверить hotplug/восстановление после временного CPU/PDH discovery error без ложного удаления |
| Метаданные проекта | подтверждённые авторы/maintainers, GitHub URL, условия публикации предоставленных изображений |

## P1 — обязательные пробелы дизайна

### Сохранённые данные, record и offline

- [ ] Спецификация версионированного `qblob`: endian, lengths, typed values, labels/descriptors,
  timestamps, statuses, limits, corruption detection и совместимость версий.
- [ ] Writer/reader без сериализации C struct/pointer/padding «как есть».
- [ ] `--record`, `--input`; offline playback через тот же sink/query contract без native sensors.
- [ ] File/Pipe **data source** с framing, backpressure, EOF/cancel и ограничениями размера.
- [ ] Roundtrip всех типов/status, damaged/truncated/oversized blobs, неизвестные поля/версии.

Существующий CSV export — отчёт, **не** бинарный capture и не воспроизводимый источник данных.
Чтение файла с текстом query также не считается реализацией File/Pipe acquisition.

### Provider и scheduler

- [ ] Автоматический fallback: приоритеты, допустимые ошибки переключения, hysteresis,
  reset counter/rate/history boundary при смене источника; deterministic mock tests.
- [ ] Event-driven источники: подписка/отписка и ограниченные события; наличие enum ON_EVENT недостаточно.
- [ ] Cost model: EXPENSIVE/ESTIMATED policy, бюджет времени и нагрузок, квоты/отложенный опрос.
- [ ] Общий acquisition/cache для нескольких активных клиентов, а не только одного engine/query.
- [ ] Принудительная отмена/таймаут acquisition там, где это позволяет OS API; документировать исключения.

### Метрики и идентичность

- [ ] `cpu.frequency_hz`: отдельно определить actual/current vs nominal/max, per-core labels,
  Linux cpufreq и Windows источники; не заменять частотой счётчика ticks или CPUID brand.
- [ ] Согласовать canonical naming: сейчас per-core `cpu.core.utilization`, labels сетевых метрик
  `ifname`/`ifindex`, а не примеры `cpu.utilization{cpu=...}` и `interface` из дизайна.
  Нужна политика aliases/deprecation, а не бесшумное разрушение запросов.
- [ ] Несколько native providers одной метрики; текущий механизм проверен mock-каталогом,
  встроенные collectors в основном предоставляют по одному способу.
- [ ] Linux NIC identity: ifindex устойчив только внутри текущего network namespace;
  reuse того же индекса и очень короткий hotplug требуют дополнительного epoch/event evidence.
- [ ] Довести три уровня описания и согласование RU/EN для всех метрик; metadata localization.

### Core ABI / History / OSAL

- [ ] Решить ABI строк: дизайн предлагает length+array, baseline использует terminated array.
- [ ] General typed history, не только numeric plot, retention/decimation без UI-зависимой настройки.
- [ ] Allocator hooks в OSAL, allocation budgets, failure injection; сейчас используется standard allocator.
- [ ] Документированный API-versioning для catalog/provider, при необходимости opaque layouts.

## P2 — расширение

- [ ] UART/JTAG внешние providers с mock transport, таймаутами и отключённым по умолчанию probing.
- [ ] Driver/native HAL variants и соответствующие presets, только после появления работающего кода.
- [ ] MSR/PortIO/physical-memory/control-register доступ — отдельная политика capabilities и прав;
  не включать опасные записи в обычный read-only сбор.
- [ ] Другие Linux архитектуры, ARM Windows, embedded/FreeRTOS: отдельные toolchains и приёмка.
  Старый embedded код сохранён только как исторический baseline, не заявлен текущим портом.
- [ ] Install/export CMake package для внешних потребителей, стабильный binary/plugin ABI.
- [ ] Запросы/агрегации сложнее текущего синтаксиса. Это PromQL-подобный язык, не реализация PromQL.

## Уже закрыто этой ревизией

- [x] Согласованные уровни core/query/sensors/HAL/OSAL/frontend и `quanta_` namespace.
- [x] Core без builtin catalog и renderer dependencies; generic catalog и explicit selection.
- [x] Групповой scheduler, отдельные channels/bindings, корректный lifecycle и partial-open cleanup.
- [x] Numeric history budget/resize/clone и проверки значений/метаданных discovery.
- [x] Typed HAL context без обратной зависимости на provider/query.
- [x] Linux OS mapping, NIC replacement identity и строгий parse CPU/network/memory/process.
- [x] Windows MIB_IF_ROW2 поля, самостоятельный process collector, PDH fractions без ложного delta.
- [x] Настоящие CSV/TXT executable entrypoints, raw value output, query-file shorthand.
- [x] Простые CMake targets, presets, auto CTest и новые regression suites.
