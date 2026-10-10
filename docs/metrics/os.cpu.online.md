# os.cpu.online

## Linux / способ получения

Активные логические процессоры — семантическая версия 1.

## Контракт

- Тип: `u64`; вид: `gauge`.
- Базовая единица: `(безразмерное значение)`.
- Политика обновления: `QUANTA_REFRESH_POLL`. Минимум дескриптора: 1000 мс. Запрошенный период ограничивается снизу либо отклоняется в строгом режиме.
- Метки: `(нет)`.
- Источник: uname(), sysconf(), /etc/os-release, /proc/uptime и /proc/loadavg.

## Значение

sysconf(_SC_NPROCESSORS_ONLN), а не число CPU в маске привязки процесса и не квота CPU в cgroup.

## Ошибки и недоступные значения

Успешно полученные ноль, false и пустая строка отличаются от неудачного чтения.
PERMISSION_DENIED означает ограничение доступа; NOT_PRESENT/GONE — отсутствие ресурса;
INVALID_DATA — неверный формат или выход за допустимый диапазон; NO_DATA — отсутствие
обязательных полей источника. Последний успешный отсчёт можно показывать с исходным
возрастом, но нельзя выдавать за новое успешное измерение.

## Запрос

```text
mode file; format table; once; select optional os.cpu.online;
```

Для производной скорости или загрузки CPU требуется начальный снимок: используйте
`--warmup 1200ms` либо периодический запрос. Фактические минимальные интервалы
конкретных метрик могут потребовать большего ожидания.

## Источник

https://docs.kernel.org/filesystems/proc.html

## Способ получения в Windows (требует нативной проверки)

Источник Windows — Win32 API; `os.arch` — нативная архитектура ОС, а не разрядность EXE.
`os.name` возвращает ProductName из реестра дословно (или Microsoft Windows при его отсутствии).
Строка ProductName может отставать от маркетингового названия; для диагностики смотрите
`os.kernel.release` (RtlGetVersion: major.minor.build). `os.kernel.version` — BuildLabEx,
либо номер NT build и service pack. `os.hostname` — DNS hostname текущего компьютера.
`os.uptime` — GetTickCount64 / 1000, не длительность жизни процесса; быстрый запуск и
гибернация могут сохранять сеанс ядра. `os.cpu.online` — GetActiveProcessorCount для всех групп.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
