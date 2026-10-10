# Стиль C и правила изменений

C11 без compiler extensions в основном коде. C++ только для optional GUI. Исходники UTF-8,
LF, 4 пробела, Allman braces, ориентир 100 колонок; `.clang-format` фиксирует настройки.
Generated `sensors/metric_docs.c` обновляется только через `tools/embed_docs.py`.

Публичные имена — `quanta_`, macros — `QUANTA_`, types/functions/fields — snake_case.
Public header самостоятельно подключает необходимые public headers и поддерживает `extern "C"`.
Приватные native/platform types не помещаются в public core API. Не использовать include-order
как скрытую зависимость и `void *` вместо известного типизированного HAL context.

Функция отвечает за одну операцию; локальные static helpers ограничены translation unit.
Сложная ownership/concurrency/overflow логика получает комментарий «почему», а не перевод
каждой строки C. Не переписывать весь файл ради unrelated исправления.

Явно определять владельца каждого allocation и borrowed pointer. Проверять capacity до записи,
overflow до умножения, enum/type/status при приёме внешнего provider output. NULL/partial cleanup
должен быть безопасным. При ошибке не публиковать successful zero или stale value как новую выборку.

Реальные OS/HAL reads не подмешиваются в parser/registry/history tests. Новый bugfix сопровождается
минимальным regression fixture. Проверки тестов не должны исчезать в Release/NDEBUG.
Не suppress-ить предупреждения глобально, чтобы скрыть ошибку своего кода.
