# Минимальный аппаратный HAL

Публичный API: `include/quanta/hal.h`. Общий wrapper — `hal/common/hal.c`.
Здесь нет SMBIOS/ACPI/CPU-load/working-set API. Все эти алгоритмы находятся выше, в sensors.

## Операции

`get_info`, `read_reg/write_reg`, `read_mem/write_mem`, `read_portio/write_portio`,
`read_pci/write_pci`, `read_gpio/write_gpio`, `set_affinity/restore_affinity`.

Регистры: enum MSR, CONTROL, DEBUG, CYCLE; неподдерживаемый вид даёт `UNSUPPORTED`.
Для Linux MSR CPU задаётся явно как `/dev/cpu/N/msr`; CURRENT_CPU не принимается из-за миграции
потока между процессорами. Control/debug registers из user mode не исполняются.
PortIO/PCI API width — 1, 2 или 4 байта; конкретный backend может поддерживать подмножество.
Linux `/dev/port` backend допускает только width=1: несколько соседних byte reads не равны
одной инструкции inw/inl. Width=2/4 здесь возвращает UNSUPPORTED. PCI адрес: segment:bus:device.function + offset (до 4095).
GPIO API не настраивает pin mode автоматически. Affinity имеет симметричное восстановление;
token начинается с нулей, связан с владельцем-потоком и не допускает двойного restore.

## Backend matrix

| Backend | Что реализовано | Что не обещается |
|---|---|---|
| `portable` (`native`, только portable profile) | Info без аппаратных capabilities | Метрики ОС, hardware I/O, affinity |
| `none` | Info с нулевыми capabilities | Любое hardware I/O |
| `linux-native` (`native`) | Info, affinity | Физическая память, MSR, PCI/PortIO/GPIO |
| `linux-dev` | Info, affinity; file I/O для `/dev/mem`, `/dev/port`, PCI config; MSR на x86 | Наличие nodes, права, загрузка драйверов, GPIO/CR/DR |
| `windows-native` (`native`) | Info, GroupAffinity | Драйвер, ring0, memory/MSR/PCI/PortIO/GPIO |
| `esp8266-native` (`native`) | CPU count, GPIO | Размер доступной RAM, arbitrary physical access, registers, PCI/PortIO |

Linux PCI использует `/sys/bus/pci/devices/ssss:bb:dd.f/config`, не собственный SMBIOS или PCI
enumerator. Backend лениво открывает конкретный файл только при операции. Нет sudo, insmod,
setuid, загрузки module или автоматического расширения прав. Linux GPIO пока unsupported.
ESP8266 допускает GPIO 0–16, исключает flash-линии 6–11, controller=0. Пины должны быть
предварительно безопасно настроены приложением; чтение тоже зависит от конфигурации платы.

Capabilities — **наличие реализации операции**, не результат проверки прав на конкретном адресе.
Так, capability READ_MEM у linux-dev не означает, что ядро разрешит читать RAM. Результат каждого
вызова — отдельный status. Доступность аппаратных таблиц и ресурсов проверяет код метрики.

## Physical memory и ограничения

`read_mem/write_mem` используют **физический адрес**, не адрес процесса. Максимум transfer —
1 MiB, либо меньший предел backend. Проверяются overflow, длина и максимальный представимый
backend адрес. `physical_memory_bytes` — OS-reported ёмкость RAM, не карта разрешённых диапазонов.
`maximum_physical_address` — предел адресования API/backend, **не последний существующий адрес RAM**.

При встраивании приложение может задать `physical_ranges` (до 16 диапазонов). Wrapper копирует
allowlist; весь запрос должен помещаться в одном диапазоне. Пустой список не объявляет память
безопасной: действуют ограничения device/OS. Автоматической карты безопасного MMIO пока нет.

Чтение памяти/регистров может иметь побочные эффекты в MMIO и не всегда атомарно.
Нельзя использовать произвольный адрес из недоверенного текста без отдельной политики приложения.
Штатный query язык не содержит команд произвольного read/write адресов.

## Запись

По умолчанию запрещена. Только embedding-код может передать `QUANTA_HAL_ALLOW_WRITE` при open/attach.
CLI не предоставляет write-enable option; штатные метрики write-функции не вызывают.
Даже с разрешением операция может вернуть UNSUPPORTED/PERMISSION_DENIED/IO_ERROR.
Частичная hardware write не является транзакцией и может иметь эффект при ошибке.

## Выбор

```bash
quanta --list-hals
quanta hal-info --hal linux-dev
quanta get os.arch --hal none
quanta examples/queries/08-hal-none.q --hal native
```

`hal-info` не читает память/регистры/PCI и не проверяет каждый возможный адрес.
`--hal` имеет приоритет над query. Неизвестное имя — ошибка, даже если выбранной OS-метрике HAL
не нужен; так опечатка не маскируется успешным native чтением.

## SMBIOS — в sensors, не в HAL

`board.model` остаётся прежней native-метрикой. Новые имена:

```text
hal "linux-dev";
once;
select optional smbios.legacy_mem.board.model;
select optional smbios.legacy_mem.bios.vendor;
```

Реализация `sensors/common/smbios_memory.c` ищет `_SM_`/`_SM3_` по 16-byte aligned адресам в
**0xF0000–0xFFFFF**, проверяет entry-point checksum и длины, следует физическому указателю и
передаёт таблицу bounded decoder `sensors/common/smbios.c`. SMBIOS3 имеет приоритет перед SMBIOS2;
это обработка формата, не provider selection.

Таблица ограничена 1 MiB; в SMBIOS3 читается объявленный maximum table size целиком, поэтому
весь диапазон должен быть доступен. UEFI-only entrypoint вне legacy-окна пока не находится.
Отсутствие/невалидность/запрет не включает fallback на sysfs. На backend none/native операция
будет UNSUPPORTED. Аппаратная проверка не выполнялась; используются RAM fixtures для 2.x/3.x.

## Добавление backend

Реализовать необходимые callbacks `quanta_hal_operations`, info и private state; остальные
callbacks оставить NULL. `quanta_hal_attach` копирует vtable/info, capabilities вычисляет из
callbacks, а владение private state получает только после успешного attach через `close`.
При ошибке attach state остаётся у вызывающего. При success close вызывается ровно один раз.
Ядро/метрики/GUI не должны просачиваться в эти callbacks.

Первичные справочные материалы, использованные для сверки формата и API:
[Linux PortIO implementation](https://github.com/torvalds/linux/blob/master/drivers/char/mem.c),
[Linux PCI sysfs](https://docs.kernel.org/PCI/sysfs-pci.html),
[Linux firmware DMI scanner](https://github.com/torvalds/linux/blob/master/drivers/firmware/dmi_scan.c),
[ESP8266 v3.4 GPIO](https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.4/components/esp8266/include/driver/gpio.h).
Код декодера и wrapper в проекте не копировался из GPL scanner.

## Безопасность автоматической сборки

Тесты используют собственные RAM/file fixtures; не пишут в настоящие MSR/PCI/MMIO и не требуют
root. Native affinity-тест временно привязывает свой поток в рамках разрешённой mask, затем
восстанавливает её. `list --compiled` ничего не опрашивает; `list --probe` уже запускает
измерения. `--redact` скрывает SENSITIVE values, но не обещает анонимизацию всех labels,
titles и diagnostics. ESP VDD/GPIO требуют отдельной проверки схемы — [ESP8266](ESP8266.md).
