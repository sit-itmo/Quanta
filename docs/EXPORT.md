# Форматы и статусы

human/table/md: полный срез текущего кеша в каждом кадре, с возрастом. При ошибке
можно видеть last-good, но статус/возраст не дают принять его за свежее значение.
CSV: новые результаты и удаления серии, без дублирования кеша по heartbeat.

CSV header:

```text
event,session_id,frame_id,sequence,timestamp_utc,monotonic_ns,series_id,source_series_id,generation,key,labels,type,unit,status,value,window_ns,read_duration_ns,quality,source,message
```

`event` — sample/remove. Labels — JSON-object внутри CSV quoted field. Type
u64/s64/f64/bool/string/bytes; unit каноническая. Удаление — GONE без value.
`timestamp_utc` — wall-clock времени кадра. `monotonic_ns` — время измерения,
не UTC. Не сравнивайте monotonic между загрузками/машинами. `sequence` относится
к физическому источнику; два output view могут иметь один source_series_id.
`window_ns` — окно rate/CPU; 0 для неоконной величины. Сессия/ID не используются
как постоянный аппаратный серийный номер.

`quality` — битовая маска; 1 = ESTIMATED. CSV сохраняет u64 десятичным целым,
не через double. Ошибочный value пуст, числовой 0 не пуст. Bool — true/false.
Bytes сериализуются шестнадцатерично. CSV не локализует десятичную точку.

Защита формул: строки, начинающиеся с =, +, -, @, TAB, CR, получают префикс `'`.
Это относится к строковым значениям, не к отрицательным числам. Для точного
небезопасного roundtrip явно укажите `--csv-unsafe-strings` и не открывайте такой
файл в spreadsheet-программе без отдельной проверки. CSV labels — объект, а не
формула; заголовки/keys/units контролируются кодом реестра.

`--redact` маскирует значения с INV_SENSITIVE: hostname, board.serial, MAC.
Это маскирование вывода, не запрет сбора и не гарантированное удаление секрета
из памяти. Другая метаинформация может косвенно идентифицировать машину; отчёт
всё равно нужно просмотреть перед публикацией.

В human/terminal управляющие символы нейтрализуются, повреждённый UTF-8 не
выполняется как терминальные последовательности. Markdown экранирует HTML,
разделители таблиц и форматирующие символы.

Статусы: OK, UNSUPPORTED, NOT_PRESENT, PERMISSION_DENIED, NO_DATA, WARMUP,
GONE, INVALID_DATA, IO_ERROR, TIMEOUT, CANCELLED, NO_MEMORY, LIMIT, BAD_QUERY.
TIMEOUT существует в общем контракте/errno mapping, но универсального hard
watchdog для getter-ов v0.1 нет. Встройте его в конкретный provider при необходимости.

MD предназначен прежде всего для `once`. При периодическом MD последовательно
пишутся отдельные таблицы. Wide-CSV, JSON export, интерактивный live-log и запись
во второй sink в этой версии не реализованы.

## Windows 0.2

Файлы и байтовый stdout — UTF-8 с LF; query может начинаться с UTF-8 BOM.
Выходной файл создаётся эксклюзивно и наследует ACL каталога. Утверждения о chmod/0600
в Linux-разделе не относятся к Windows. PowerShell-редирект может перекодировать
вывод в зависимости от версии оболочки; для сохранения исходных байтов используйте --output.
