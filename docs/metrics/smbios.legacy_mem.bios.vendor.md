# smbios.legacy_mem.bios.vendor

Производитель BIOS, SMBIOS type 0, vendor string.

## Способ получения

Отдельный метод, не замена `board.model`/`bios.vendor`: код метрики сканирует физический диапазон `0xF0000..0xFFFFF` с выравниванием 16 байт, распознаёт SMBIOS 2.x/3.x entry points и читает таблицу только через `quanta_hal_read_mem`. Парсер находится в `sensors/board`, не в HAL. Строковый property, ONCE, флаг EXPENSIVE.

SMBIOS 3.x entry point имеет приоритет. Проверяются обе контрольные суммы 2.x, длины, переполнения и лимит таблицы 1 MiB. Для 3.x запрашивается объявленный максимальный размер таблицы; недоступность части диапазона даёт ошибку чтения, без частичного успешного снимка. Нужна читаемость всего объявленного диапазона. Метод не ищет UEFI-only entry points за пределами legacy-window.

Нет fallback к sysfs/WinAPI. При отсутствии READ_MEM — UNSUPPORTED; при запрете ОС/allowlist — PERMISSION_DENIED; при отсутствии сигнатур — NOT_PRESENT; при повреждении — INVALID_DATA; превышении бюджета — LIMIT. Не выполняет записи и не загружает драйверы. Применим к x86 desktop-сборке с QUANTA_WITH_DMI.

```text
hal "linux-dev";
once;
select optional smbios.legacy_mem.bios.vendor;
```

`linux-dev` требует доступных device nodes и прав ОС; capability не гарантирует доступ. Не запускайте все метрики с повышенными правами ради этого примера без необходимости. Контракт проверен на RAM-fixture, не на реальной физической памяти. См. [HAL](../HAL.md).
