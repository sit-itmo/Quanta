# Query и запуск

Это ограниченный query language Quanta v1, не PromQL. Расширение примеров — `.q`.

```text
version 1;
mode cli;
format table;
every 1s;
count 5;
history 1m;
layout grid(columns=2);
select os.name as text(title="Operating system");
select os.memory.utilization as bar(min=0, max=1, title="RAM");
select optional cpu.utilization as plot(min=0, max=1);
select optional rate(device.net.rx_bytes_total) as value(unit="KiB/s");
```

`mode file` и `mode cli` обозначают неинтерактивный frontend. `mode tui` требует сборку с TUI;
`mode gui` — с GUI. Layout и plot задают presentation metadata; текстовый вывод не рисует графики.

## CLI

```bash
quanta --version
quanta list --compiled
quanta describe os.memory.available_bytes
quanta get os.name os.arch --format table
quanta run --query 'once; select os.name;'
quanta run --query-file examples/queries/01-inventory.q --redact
quanta examples/queries/01-inventory.q --redact
quanta explain --query 'select os.uptime; select os.uptime as plot;'
```

В Windows используйте `quanta.exe` или полный путь. Для PowerShell в примере `--query` подходит
одинарно-кавычная строка. Shorthand без `run --query-file` распознаёт путь с расширением `.q`.

## HAL и labels

```text
hal "none";
once;
select os.arch;
select optional device.net.rx_bytes_total{ifname="eth*"};
```

Для периодической скорости: `every 1s; count 5; select rate(device.net.rx_bytes_total);`.
`hal "name";` допускается один раз. `--hal name` переопределяет query HAL; повторный/пустой CLI
аргумент — ошибка. По умолчанию native. Список: `quanta --list-hals`, сведения:
`quanta hal-info --hal native`. Неизвестное имя не приводит к fallback.

`provider(...)` удалён и отклоняется с migration diagnostic. Одно имя — одна реализация.
Другой способ аппаратного измерения выбирается другим metric key, не параметром provider.
HAL none отключает hardware primitives, но прямые native OS/CPUID/SDK метрики продолжают работать.

## Частота, ошибки, rate

`every` принимает `ms`, `s`, `m`, `h`; допустимы дробные значения. Есть `once`, `count`, `for`,
`history`, per-select `every`. `once` нельзя сочетать с многократным сбором. Минимальный допустимый
интервал query — 10 ms, но конкретный metric/device может требовать более редкого опроса.
По умолчанию интервал поднимается до допустимого с диагностикой; `strict` превращает это в ошибку.

Первое вычисление скорости/CPU utilization — `WARMUP`, не успешный ноль. Используйте периодический
сбор либо `--warmup 1200ms`; более медленному датчику может понадобиться больше времени.
`optional` разрешает отсутствие metric/instance, но не делает неправильный синтаксис корректным.
`--fail-on-error` позволяет считать ошибки измерений неуспехом запуска.

## Форматы и файлы

`--format human|table|csv|md|value`. `quanta_cli` — тот же CLI; `quanta_csv` по умолчанию выбирает
CSV, `quanta_txt` — table. В этих двух executable format default имеет приоритет над `format`
в query-файле; явный `--format` переопределяет его. Все используют один parser/core.

`value` печатает по строке на активную серию, без заголовков и единиц. Это канонические значения,
не отображаемые GiB/проценты. Недоступное значение — `--`; строки очищаются для безопасного
однострочного вывода. Для нескольких series порядок сам по себе не является долгосрочным ID;
для интеграции с метаданными используйте CSV. Diagnostic messages идут в stderr.

```bash
quanta get os.uptime --format value
quanta_csv get os.name os.arch --output report.csv
quanta_txt get os.name os.arch
quanta get os.name --format md --output report.md
```

Выходной файл создаётся эксклюзивно: существующий файл не перезаписывается. CSV содержит
идентичность, labels, timestamps, status и value; labels — JSON-строка внутри CSV quoting.
`--redact` скрывает значения SENSITIVE descriptors, но не является универсальной анонимизацией
labels, имён interfaces, query title и всех диагностик.

## Пока отсутствует

`--record capture.qblob` и `--input capture.qblob` отклоняются как неизвестные опции.
`05-record-csv.q` — исторически названный пример **CSV-экспорта**, не blob recording.
Импорт сохранённых измерений, replay, бинарный формат и Pipe acquisition — [TODO](TODO.md).
