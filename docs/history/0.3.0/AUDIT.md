# Аудит inv-0.2.2 → Quanta 0.3.0

Дата: 2026-10-10. Основание: переданный `inv-0.2.2.zip`, неизменённый
[дизайн-документ](Quanta_Design.md), логотип и схема. Внешняя документация использована
для сверки отдельных Windows API, а не вместо чтения пользовательских исходников.

## Резюме

Входной проект уже содержал значительную реализацию: parser, collectors, engine, CLI,
TUI, optional ImGui frontend, windows data helpers и тесты. Это не пустой каркас.
Главная проблема — смешанные границы core/builtin providers/model/rendering и
несоответствие части объявленных метрик реальным provider callbacks.

Новая поставка сохраняет и перерабатывает эту основу. Добавлены независимый catalog,
явный provider selection, отдельные core modules, typed HAL boundary, исправления acquisition,
дополнительные тесты и CMake-проверка при build. Полный целевой дизайн **не завершён**:
blob/offline/Pipe/fallback/events/frequency/external HAL остаются открытыми задачами.

## Воспроизведённые проблемы исходной версии

Исходники собраны GCC Debug/Ninja, GUI=OFF, INV_STRICT_STATIC=OFF, warnings-as-errors.
Компиляция завершилась успешно. Из 7 CTest suites прошли 4, не прошли 3.
См. [исходный CTest log](audit/logs/original-ctest.log).

| Проблема | Evidence / причина | Изменение |
|---|---|---|
| devices: 2 assertions | новая NIC identity не создавала новую series: `added==2`, `first_id!=last_id` не выполнялись | Linux NIC entity теперь ifindex, rate baseline отделён при замене; старый regression сохранён |
| integration: OS CSV | descriptors `os.name`/`os.arch` и фактический mapper не согласованы | восстановлены snapshot/mapping uname/os-release/arch/release/load/online |
| documentation | генератор требует отсутствующий `docs/metrics/mcu.chip_id.md` | desktop definitions отделены от исторического embedded; генератор следует выбранным native `.def` |

Исторические «passed» отчёты внутри входного ZIP не использованы как доказательство:
входной код был собран и тесты запущены заново.

## Найдено при просмотре кода и исправлено

| Область | Изменение | Проверка / ограничение |
|---|---|---|
| Core dependency | builtin registration вынесен из core, public API отделён от internal/native/UI | core-only catalog/history/OSAL executables линкуются без sensors/output |
| Monolithic engine | lifecycle, discovery, scheduler разделены; channels отдельно от views | shared reads, разные bindings/rate, ONCE, cancellation fixtures |
| Providers | catalog принимает совместимые альтернативы; явный quoted provider selector | default/explicit/missing/duplicate/incompatible tests |
| Lifecycle | plan/paths копируются; close только после попытки open; NULL/partial-safe cleanup | partial-open и повторный run regression |
| Discovery/value | проверка принадлежности catalog/provider, labels, terminated fields, enum/type/finite values | malformed mock provider output tests |
| History | numeric ring, cap/budget, resize с откатом, deep clone, monotonic/sequence checks | отдельный history suite |
| Live | guarded start/pump/finish, backpressure и корректный stop/join | core mock worker tests; это не stress proof всех interleavings |
| Linux CPU | строгий parse /proc/stat, checked counters, guest не суммируется повторно, sysconf ticks | HAL fixtures: valid/duplicate/overflow/malformed |
| Linux memory | строгие line boundaries, kB suffix, overflow, duplicate и relations | parser/negative fixtures |
| Linux network | строгие 16 полей counters, статусы вместо фиктивных нулей, ifindex identity | device hotplug regression; reuse ifindex остаётся ограничением |
| Linux process | RSS в байтах через sysconf pagesize, malformed/overflow checks | отдельный statm fixture |
| Windows process | отдельный process snapshot без зависимости от memory-provider allocation | исходники пересмотрены; нативный запуск не выполнен |
| Windows network | MIB_IF_ROW2 InOctets/OutOctets и остальные реальные поля вместо CounterData | сверка Microsoft API; нативная компиляция не выполнена |
| Windows CPU | прямой PDH fraction отделён от накопленных raw accounting ticks; учёт processor groups | portable math tests/API review; нативная PDH-проверка нужна |
| DIMM | provider mapping фактических SMBIOS slots/handle/locator/bank | portable SMBIOS fixtures; native firmware path не проверен |
| Frontend | 4 executable entrypoints, --format value, .q shorthand, layer-neutral model | integration + frontend_modes + TUI PTY |
| CMake | explicit source lists, target-local options, selected providers, auto CTest | GCC/Clang/ASan/minimal matrix |

## Соответствие 17 разделам дизайна

«Частично» означает, что механизм/интерфейс существует, но весь объём раздела не реализован.

| § дизайна | Статус | Реализация / открытый остаток |
|---|---|---|
| 1. Назначение | Частично | Linux collection/processing/display; Windows код, внешние/сохранённые sources ещё не завершены |
| 2. Принципы | В основном | C11 core, optional C++ GUI, data/presentation separation, query naming; полная safety/cost политика TODO |
| 3. Архитектура | Базовое разделение реализовано | core/query/sensors/HAL/OSAL/frontends; см. target graph; driver HAL ещё нет |
| 4. Метрики | Частично | descriptors/types/units/docs/labels/refresh/flags; naming и string ABI отличаются от примеров |
| 5. Value | Частично, различие ABI | bounded owned values; status/quality в sample, не value; string без explicit length |
| 6. Provider | Частично | default/explicit alternatives, mock coverage; автоматического fallback нет |
| 7. Scheduler | Частично | grouped refresh на engine, period floors, ONCE, numeric history; events/cost budgets/multi-client TODO |
| 8. Query | Рабочий поднабор | select/optional/rate/provider/labels/every/history/mode/layout; не полный PromQL |
| 9. Live/Offline/Record | Только Live | CSV file sink есть; qblob input/record/replay отсутствуют |
| 10. Внешние устройства | Нет реализации | UART/JTAG/transport registration TODO |
| 11. Регистрация | Реализована для native baseline | `.def`, builtin catalog, QUANTA_WITH_* и presets; fictitious driver presets не добавлены |
| 12. Сборка | Реализована | простой CMake, C11, CTest, documented Linux/Windows paths; Windows ещё требует запуска |
| 13. Тесты | Частично по полному дизайну | registry/query/scheduler/history/units/providers/HAL/OSAL/formats; blob tests после blob implementation |
| 14. Документация | Базовый набор есть | descriptor/key/md + generated embedded docs; согласование RU/EN/полнота metadata ещё нужна |
| 15. Безопасность | Базовые ограничения | read-only default, no implicit elevation, bounded parsing; hard timeouts отдельных API TODO |
| 16. Первая версия | Не полностью | CLI/TXT/CSV и больше collectors уже есть; cpu.frequency_hz отсутствует, native Windows приёмка нужна |
| 17. Концепция | Сохранена | общий core/API и независимые способы acquisition/presentation |

## Соответствие исходной картинке

CLI/just values — `frontends/cli` + value format. CSV/TXT/MD — file sink.
TUI и ImGui — самостоятельные frontends. Query — часть core; metric registration — sensors/metrics.
HAL и OSAL — отдельные исходные слои. **Binary serialization и Sources File/Pipe не реализованы.**
На рисунке HAL содержит потенциально опасные read/write операции; они не добавлены в read-only
baseline без отдельного requirements/security/driver проекта. Это явно сохраняется в TODO,
а не заменяется фиктивными API, возвращающими OK.

## Что изменено в интерфейсах сознательно

Префикс `quanta_` вместо `inv_`; исходная совместимость ABI не обещается. Core public catalog
имеет caller-owned lifetime. Type enum содержит U64 и BOOL сверх дизайн-примера; string —
terminated fixed buffer, bytes — explicit length. Core numeric model выделяет history только
для plot, поэтому это не полный универсальный архив всех типов. Preset names описывают только
реально выбранные source sets. Детали — [MIGRATION](MIGRATION.md), [ARCHITECTURE](ARCHITECTURE.md).

## Проверка внешних API: первичные источники

- Microsoft MIB_IF_ROW2: https://learn.microsoft.com/en-us/windows/win32/api/netioapi/ns-netioapi-mib_if_row2
- Microsoft GetSystemTimes (processor-group caveat): https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes
- Microsoft GetProcessMemoryInfo: https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo
- CMake presets schema 3 / CMake 3.21: https://cmake.org/cmake/help/v3.21/manual/cmake-presets.7.html
- Pinned ImGui commit 1.91.9b: https://github.com/ocornut/imgui/commit/f5befd2d29e66809cd1110a152e375a7f1981f06

Эти ссылки подтверждают API/формат/зависимость, не свидетельствуют о запуске проекта на Windows
или прохождении GUI теста. Подробные выполненные проверки — [TEST_REPORT](TEST_REPORT.md).
