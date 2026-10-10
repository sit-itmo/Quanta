# Миграция INV / Quanta 0.3.0 → 0.4.0

## Рабочая папка

Распакуйте полный архив в новую папку, перенесите только собственные правки после сравнения.
Не копируйте generated CMake files, sdkconfig или старые provider headers в новый source tree.
Имена `quanta_*` из 0.3.0 сохранены там, где контракт не менялся. Совместимость ABI не обещается.

## Providers удалены

Удалены `provider.h`, `quanta_provider_desc`, `quanta_provider_context`, `.provider` в descriptors
и `.provider` в selectors. Новый context — `quanta_metric_context`, private state — `private_data`.
Descriptor содержит прямые `init/enumerate/collect/destroy/read` callbacks. Catalogue допускает
только уникальные metric keys. Альтернативный способ требует нового имени метрики.

```text
# Больше не принимается:
select provider("os", os.arch);

# Теперь:
hal "none";
select os.arch;
```

Нельзя механически заменить provider на HAL: это **разные оси**. Старый provider "os" —
реализация метрики; новый HAL "none" — отключённые hardware primitives. `os.arch` работает
напрямую через native acquisition. Fallback и providers не возвращаются как hidden aliases.

`quanta_catalog_find(catalog, key)` теперь имеет два аргумента.
`quanta_engine_metric_acquisitions(engine, key)` заменяет статистику опроса по provider ID:
счётчик общего snapshot доступен через выбранное имя метрики.

## HAL/OSAL/native code

Все высокоуровневые acquisition files перенесены из hal в `sensors/<group>/<platform>.c`.
`quanta_native_*` — private функции метрик. Типы снимков — `sensors/private/`.
SMBIOS decoder — `sensors/board/`, Windows PDH/math — sensors, CPUID affinity helpers — cpuid.
Настоящий минимальный HAL теперь находится в `hal/` и использует отдельный `quanta_hal *`.

Метрика может обходиться без HAL. `void *private_data` используется только для её собственного
платформенного состояния; HAL handle не заменяется произвольным `void *`.

## ESP8266

Активный путь `firmware/esp8266`, SDK component называется quanta. Нет зависимости от
исторического baseline ZIP. Все 9 ключей MCU сохранены, но VDD units и factory chip ID получили
semantic_version=2. VDD выключен по умолчанию ради неизвестной схемы TOUT; исходный lab профиль
сохранён отдельно для сознательного включения. [Полная инструкция](ESP8266.md).

Serial app печатает больше MCU полей, сохраняет 6 decimal digits для float и обрабатывает
крайние int64/uint64. Прежний source полностью доступен в read-only historical ZIP для merge.

## Что не изменялось намеренно

Исторический `Quanta_Design.md`, логотип, исходная архитектурная картинка и лицензия сохранены.
Основные существующие metric keys, CLI CSV/TXT/TUI/GUI и обычные query без provider() сохраняются.
Два новых legacy physical SMBIOS ключа добавлены отдельно, не вместо native board/bios.
