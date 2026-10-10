# Как добавить показатель

1. Выберите устойчивое dotted имя, тип, единицу и точную формулу. PROPERTY,
   GAUGE, COUNTER не смешивайте с ONCE/POLL/ON_EVENT. Документируйте namespace,
   права, область применимости и характерные ошибки.
2. Напишите C getter с сигнатурой inv_read_fn. На входе `out_value->type` уже
   установлен; верните значение в соответствующем поле union и INV_OK либо
   точный статус. Не форматируйте байты в «1.5 GiB» и не печатайте в stdout.
3. Добавьте запись INV_METRIC в `metrics/<group>.def`. Поля: key/title/description,
   doc_path, semantic_version, type/kind/refresh/unit, min/recommended/stale,
   flags/counter_bits/label_schema/dependencies/provider/read/arg.
4. Добавьте `docs/metrics/<key>.md`, обновите каталог документации и выполните
   `python3 tools/embed_docs.py`. Этот генератор читает .def и только перечисленные
   там документы; бинарник не парсит Markdown при старте.
5. Добавьте fixture и проверку getter-а; выполните обе сборки (включено/выключено).

Пример уже реализованной простой метрики: `process.self.rss_bytes` в
`src/providers/process.c` + `metrics/process.def`. Пример группового источника:
`src/providers/memory.c`. Динамических экземпляров: `network.c` / `hwmon.c`.

## Новый provider

`inv_provider_desc` находится в `src/core/internal.h`. Provider может иметь
open/discover/refresh/close. refresh создаёт один исходный снимок; read извлекает
из него нужные поля. open/close симметричны даже после частичной ошибки.
Для уникального провайдера выделите новый индекс и увеличьте INV_PROVIDER_COUNT;
registry_validate проверяет коллизии. Добавьте CMake-флаг, .def include с #if,
условный source и тесты. В этой версии provider slots намеренно статические.

Discover вызывает sink->emit для каждого найденного inv_instance. entity_key
определяет идентичность у данного провайдера; display labels — отдельно. Новый
ресурс с похожим именем не должен унаследовать старую историю. Возвращайте ошибку
при неполном перечислении: оно не считается массовым удалением устройств.
Фильтруйте inv_metric_wanted до дорогих действий. Не делайте скрытых root-запросов.

API библиотеки v0.1 пока не обещает стабильность layout/ABI для внешних бинарных
потребителей: это статически собранный проект. Внешний устойчивый интерфейс —
имена/семантика метрик, документированный INVQ и CSV. Перед изменением значения
существующего ключа измените semantic_version и документацию.
