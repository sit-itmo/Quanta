# process.self.io.read_bytes_total

INV read transfer bytes (not necessarily disk I/O).

Единица: `B`. Источник: `GetProcessMemoryInfo / GetProcessIoCounters`.
Метки: `нет`. Тип и расписание берутся из скомпилированного реестра;
команда `quanta describe process.self.io.read_bytes_total` показывает актуальные метаданные сборки.

## Windows backend (требует нативной проверки)

Измеряется только сам процесс INV, не любой произвольный PID.
rss_bytes = WorkingSetSize, private_bytes = PrivateUsage (private committed bytes),
handles = GetProcessHandleCount, pid = GetCurrentProcessId. Working set и private commit
не являются одним и тем же показателем и не должны складываться.
io.read_bytes_total / io.write_bytes_total — ReadTransferCount/WriteTransferCount из
GetProcessIoCounters: это учтённый процессный I/O, не обещание исключительно дисковой нагрузки.
Значения поступают из нескольких WinAPI, не из атомарного меж-API снимка.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
