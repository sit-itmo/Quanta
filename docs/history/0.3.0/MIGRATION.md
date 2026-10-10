# Миграция с inv-0.2.2

Эта ревизия — новый полный source baseline, **не overlay поверх старого дерева**.
Распакуйте в новый каталог: иначе останутся старые `src/`, generated files и build cache.

## Совместимость

Product/version: INV 0.2.2 → Quanta 0.3.0. `inv_*`/`INV_*` заменены на `quanta_*`/`QUANTA_*`;
это осознанное source/ABI breaking change, compatibility aliases не поставляются.
CLI executable: `inv` → `quanta`/`quanta_cli`. Примеры `.invq` перенесены в `.q`;
`--query-file` читает текст независимо от расширения, shorthand распознаёт `.q`.

Публичные API core теперь в `core/include/quanta/`. Convenience API builtin catalog — в
`sensors/include/quanta/sensors.h`; внешнему provider core-only достаточно `<quanta/quanta.h>`
и `quanta_engine_create_with_catalog`. Структуры plan/series/value изменены; требуется пересборка
всех потребителей, нельзя смешивать старые объекты/headers.

## Карта модулей

| Было | Стало |
|---|---|
| `src/core` и общий internal header | `core/src`, `core/private`, `core/include/quanta` |
| monolithic engine | `engine.c`, `discovery.c`, `scheduler.c` |
| registry вместе с core | generic `core/src/registry.c` + builtin `sensors/registry.c` |
| `src/query` | `query/src` как часть target core |
| `src/providers` | `sensors/<domain>/provider.c` |
| `src/hal` | `hal/<platform>` и typed headers |
| `src/osal` | `osal/<platform>` и public/private API |
| frontend model/history/worker | neutral `core/history.c`, `core/live.c`; rendering остаётся frontend |
| `queries` | `examples/queries` |
| старые docs/logs/patches/embedded | исторический source archive `docs/baseline` |

Подробная файл-к-файлу карта: [audit/SOURCE_MAP.json](audit/SOURCE_MAP.json).
Она указывает назначение/историческое сохранение, а не обещает побайтовое совпадение новых файлов.

## Что не потеряно и что не является текущим портом

В `docs/baseline/inv-0.2.2-source.zip` сохранены все файлы входного дерева, кроме каталогов `build`.
В частности, там остаются прежние docs, patches, reports и FreeRTOS/ESP32 исходники.
Старые отчёты в baseline — исторические, не evidence новых тестов. ESP-IDF generated binaries,
cache и object files удалены из distribution; оригинальный пользовательский ZIP не изменялся.

Новый CMake поддерживает Linux/Windows. Старый embedded порт не был перенесён на новый ABI и
не объявлен работоспособным. Его возвращение требует отдельной адаптации и test matrix.

## Изменившееся поведение

Default CMake больше не принуждает к полностью статическому EXE и не скачивает ImGui.
CTest выполняется при обычном build. Удалены stale `metrics/catalog.json`: authoritative catalog
задаётся `.def` и параметрами сборки, просмотр — `quanta list --compiled`.
NIC с новым ifindex — новая серия/rate baseline; ошибки malformed источников больше не маскируются
под успешные значения. `value` — новый формат. `provider("id", ...)` — явный выбор, не fallback.
