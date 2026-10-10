# windows.memory.commit.limit_bytes

System commit limit.

Единица: `B`. Источник: `GlobalMemoryStatusEx / GetPerformanceInfo`.
Метки: `нет`. Тип и расписание берутся из скомпилированного реестра;
команда `quanta describe windows.memory.commit.limit_bytes` показывает актуальные метаданные сборки.

## Windows backend (требует нативной проверки)

Физические total/available получены одним GlobalMemoryStatusEx; used = total − available,
utilization = used / total. Available включает страницы, которые Windows считает доступными,
а не только полностью свободные страницы; это платформенная семантика, не алгоритм Linux MemAvailable.
Остальные поля берутся одним GetPerformanceInfo. CommitTotal/Limit/Peak — системный commit,
не размер/занятость pagefile. SystemCache, KernelPaged и KernelNonpaged возвращаются в страницах;
умножение на PageSize проверяется на переполнение. ProcessCount, ThreadCount, HandleCount —
системные счётчики объектов, а не значения только процесса INV. Два API не образуют атомарный
снимок всей ОС; формула physical used использует только один ответ GlobalMemoryStatusEx.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getperformanceinfo
