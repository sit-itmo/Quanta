# Аудит и сверка исходников: Quanta 0.4.0

## Входные материалы

Сопоставлены три source archive: исходный `inv-0.2.2-windows-src(1).zip`, пользовательский
`inv-0.2.2.zip` с MCU и предыдущий результат `Quanta-0.3.0.zip`.
Точные SHA256/размеры — [INPUTS.json](audit/INPUTS.json). Входные архивы не менялись.

Между pre-MCU и MCU деревьями без build/cache файлов: **52 добавленных, 17 удалённых,
15 изменённых путей**. Значительная часть этой дельты — перемещение Windows/native acquisition
между папками, а не новые аппаратные возможности. Полный список: [ORIGINAL_VS_MCU.json](audit/ORIGINAL_VS_MCU.json).

## Исправление предыдущей поставки

В 0.3.0 MCU/ESP оставались только историческими исходниками, без активного маршрута сборки.
Это не выполняло требование сохранения embedded-функциональности. В 0.4.0 возвращены SDK
component, firmware app/config, MCU/OS acquisition, FreeRTOS OSAL и полноценная связь с новым core.
Историческая копия дополнительно сохранена, но больше не заменяет active implementation.

По literal `.key` во всех платформенных `.def`: pre-MCU 78, MCU 87, 0.3.0 78,
**0.4.0 89**. Ни одного из 87 ключей MCU-архива не потеряно; два новых — legacy physical SMBIOS.
Это объединение имён разных платформ, не число метрик отдельного executable и не утверждение
об успешном чтении каждого аппаратного поля. [METRIC_KEYS.json](audit/METRIC_KEYS.json).

## Применение уточнённой архитектуры

| Требование | Изменение | Проверка |
|---|---|---|
| Удалить providers, не переименовать их | Убраны provider descriptor/context/selector; direct callbacks, global unique keys | Registry/parser/source-boundary tests |
| Не удваивать системные чтения | Private batching по одинаковому lifecycle tuple | Scheduler/shared-field/view tests |
| Минимальный HAL | Hardware-only API и backend files, no high-level metric functions | HAL contract/mock tests, source boundary checks |
| OSAL только инфраструктура | CPU/memory/process/PDH/SMBIOS/CPUID acquisition в sensors | Active paths / include boundary |
| Метрики могут не использовать HAL | Existing native API/SDK collectors обходят HAL | OS query с `hal none`, ESP seam |
| Выбор HAL | Query statement, CLI override, info/list, injectable borrowed handle | CLI, parse, ownership/conflict tests |
| Сохранить MCU | 9 keys, SDK component/app, common ABI manifest | 2 host SDK-seam targets |
| Другой метод — другое имя | Отдельные `smbios.legacy_mem.*` | RAM SMBIOS2/3 fixtures, no fallback |

Private acquisition в scheduler не выбирается пользователем и не имеет provider ID/priority.
Объяснение этого решения и API lifecycle — [ARCHITECTURE](ARCHITECTURE.md).

## Дополнительные исправления

Восстановление ESP выявило malformed defaults (две config строки склеены), неверные единицы
VDD33, зависимость chip ID от runtime Wi-Fi MAC, небезопасные крайние значения serial integer
formatter и ограничение прежнего condition/broadcast adapter. Исправления и изменения semantics
описаны в [ESP8266](ESP8266.md). VDD=off по умолчанию — намеренная защита для неизвестной платы;
исходный lab profile сохранён для сознательного включения после проверки проводки.

Linux HAL не выдаёт чтение нескольких соседних `/dev/port` bytes за одну inw/inl операцию:
этому backend разрешён только width=1; остальные допустимые API widths возвращают UNSUPPORTED.
MSR и PCI работают через собственные bounded file paths; physical offsets 64-bit.
Write policy, перенос allowlist во владение handle, overflow/status/cleanup покрыты fixtures.
Никакие реальные memory/MSR/PCI записи при тестировании не выполнялись.

Проверка распакованного release также выявила забытый `provider()` в `00-baseline.q`.
Пример исправлен, а тест теперь запускает оба базовых query-файла и проверяет отсутствие
устаревшего selector во всех поставляемых `.q`. Затем итоговый архив проверен заново.

## Источники и сохранённые материалы

[Source map](audit/SOURCE_MAP.json) сопоставляет каждый non-build файл MCU-архива с активными
путями либо явно помечает historical-only вспомогательные материалы. Карта не означает
побайтную идентичность refactored файлов. Exact исходные MCU configs сохранены отдельно в
`firmware/esp8266/reference`; исходный дизайн и обе картинки не перерисовывались.

Предыдущие отчёты/логи перемещены в `docs/history/0.3.0/`: они **не являются результатами 0.4.0**.
Старые relative links внутри этих historical copies могут отражать прежнюю структуру.
Текущие результаты находятся в [TEST_REPORT](TEST_REPORT.md), актуальные ограничения — [TODO](TODO.md).

## Граница подтверждения

Code-path restoration, host builds и SDK seam tests выполнены; настоящий ESP SDK link/flash,
работа платы, native Windows и GUI window здесь не подтверждены. Проверки имен/компиляции
не объявляются доказательством аппаратной совместимости. Эти этапы включены в P0 приёмки.
