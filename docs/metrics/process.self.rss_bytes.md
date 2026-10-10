# process.self.rss_bytes

## Linux / способ получения

Резидентная память сборщика — семантическая версия 1.

## Контракт

- Тип: `u64`; вид: `gauge`.
- Базовая единица: `B`.
- Политика обновления: `QUANTA_REFRESH_POLL`. Минимум дескриптора: 250 мс. Запрошенный период ограничивается снизу либо отклоняется в строгом режиме.
- Метки: `(нет)`.
- Источник: резидентные страницы /proc/self/statm × sysconf(_SC_PAGESIZE).

## Значение

Количество резидентных страниц из /proc/self/statm, умноженное на размер страницы. Недорогая приближённая оценка текущей резидентной памяти quanta; это не PSS, байты аллокатора, пиковый RSS или память другого процесса.

## Ошибки и недоступные значения

Успешно полученные ноль, false и пустая строка отличаются от неудачного чтения.
PERMISSION_DENIED означает ограничение доступа; NOT_PRESENT/GONE — отсутствие ресурса;
INVALID_DATA — неверный формат или выход за допустимый диапазон; NO_DATA — отсутствие
обязательных полей источника. Последний успешный отсчёт можно показывать с исходным
возрастом, но нельзя выдавать за новое успешное измерение.

## Запрос

```text
mode file; format table; once; select optional process.self.rss_bytes;
```

Для производной скорости или загрузки CPU требуется начальный снимок: используйте
`--warmup 1200ms` либо периодический запрос. Фактические минимальные интервалы
конкретных метрик могут потребовать большего ожидания.

## Источник

https://docs.kernel.org/filesystems/proc.html

## Способ получения в Windows (требует нативной проверки)

Измеряется только сам процесс INV, не любой произвольный PID.
rss_bytes = WorkingSetSize, private_bytes = PrivateUsage (private committed bytes),
handles = GetProcessHandleCount, pid = GetCurrentProcessId. Working set и private commit
не являются одним и тем же показателем и не должны складываться.
io.read_bytes_total / io.write_bytes_total — ReadTransferCount/WriteTransferCount из
GetProcessIoCounters: это учтённый процессный I/O, не обещание исключительно дисковой нагрузки.
Значения поступают из нескольких WinAPI, не из атомарного меж-API снимка.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
