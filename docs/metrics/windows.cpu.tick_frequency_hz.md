# windows.cpu.tick_frequency_hz

GetSystemTimes raw counter frequency.

Единица: `Hz`. Источник: `GetSystemTimes / PDH Processor Information`.
Метки: `нет`. Тип и расписание берутся из скомпилированного реестра;
команда `quanta describe windows.cpu.tick_frequency_hz` показывает актуальные метаданные сборки.

## Windows backend (требует нативной проверки)

На системе с одной процессорной группой GetSystemTimes даёт aggregate kernel + user;
idle уже входит в kernel и второй раз не добавляется. Загрузка вычисляется по двум снимкам:
1 − Δidle / Δ(kernel+user). Начальный результат и сбросы — WARMUP.
Единица сырых cpu.time_total / cpu.idle_total — 100-нс ticks; частота явно задана
windows.cpu.tick_frequency_hz = 10 000 000. Это не USER_HZ Linux.
На системах с несколькими группами сырые aggregate tick-метрики возвращают UNSUPPORTED,
а общая и поядерная загрузка используют PDH Processor Information / % Processor Time.
Метки cpu.core.utilization: cpu="group,number", group и number. Локализация PDH выполняется
через PdhAddEnglishCounterW и локализованный wildcard; имена групп не зависят от языка интерфейса.
Метрика измеряет долю занятого процессорного времени, а не Processor Utility с поправкой на частоту.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes
