# Quanta — Design Document

## 1. Назначение проекта

**Quanta** — кроссплатформенная система для сбора, хранения, обработки и отображения системных метрик.

Основная идея проекта — предоставить единый интерфейс для получения метрик от:

- операционной системы;
- аппаратного обеспечения;
- драйверов;
- внешних устройств;
- заранее сохранённых наборов данных.

На первом этапе поддерживаются **Linux** и **Windows**. Архитектура должна позволять добавлять другие платформы без переработки ядра.

---

## 2. Основные принципы

1. Основной код проекта пишется на **чистом C**.
2. В ядре не используются тяжёлые зависимости вроде Boost, Qt и аналогичных библиотек.
3. GUI является опциональным и может использовать **C++ + Dear ImGui + OpenGL/framebuffer**.
4. Сбор данных полностью отделён от их отображения.
5. Конкретный набор доступных метрик определяется платформой, HAL/OSAL и параметрами сборки.
6. Система должна безопасно получать данные и не нарушать нормальную работу ОС и оборудования.
7. Один и тот же показатель может иметь несколько способов получения.
8. Все метрики имеют единое именование и доступны через query-язык.

---

## 3. Архитектура

Quanta состоит из трёх основных уровней.

```text
+------------------------------------------------------+
|                    Frontends                         |
| CLI | GUI | TUI | CSV | TXT | Report | value         |
+--------------------------+---------------------------+
                           |
                           v
+------------------------------------------------------+
|                    quanta_core                       |
| Registry | Query | Scheduler | History | Providers   |
+--------------------------+---------------------------+
                           |
              +------------+------------+
              |                         |
              v                         v
+--------------------------+  +-------------------------+
|           OSAL           |  |           HAL           |
| Windows / Linux / ...    |  | HW / Driver / UART ...  |
+--------------------------+  +-------------------------+
```

### quanta_core

Статическая библиотека на C.

Отвечает за:

- регистрацию метрик;
- выбор provider-а;
- выполнение query;
- планирование опроса;
- хранение истории;
- преобразование значений;
- работу с сохранёнными blob;
- взаимодействие с frontend.

### Frontend

Frontend не должен знать, каким способом была получена метрика.

Планируемые варианты:

- `quanta_cli`
- `quanta_gui`
- `quanta_tui`
- `quanta_csv`
- `quanta_txt`

Одна и та же query должна по возможности работать с разными frontend.

### HAL

Hardware Abstraction Layer предоставляет низкоуровневый доступ к оборудованию.

Для одной ОС могут существовать разные HAL:

```text
Windows:
    native
    vendor-driver
    quanta-driver

Linux:
    sysfs
    hwmon
    procfs
    privileged
```

Разный HAL может предоставлять разный набор метрик.

### OSAL

OS Abstraction Layer содержит минимальный набор платформенных функций:

- время;
- потоки;
- синхронизация;
- память;
- файлы;
- события;
- системная информация.

---

## 4. Метрики

Каждая метрика должна иметь стабильное имя.

Примеры:

```text
cpu.utilization
cpu.frequency_hz
os.memory.utilization
os.memory.available_bytes
device.net.rx_bytes_total
sensor.temperature
board.model
os.name
```

Для метрик с несколькими экземплярами используются labels:

```text
cpu.utilization{cpu="0"}
device.net.rx_bytes_total{interface="eth0"}
sensor.temperature{sensor="cpu_package"}
```

### Описание метрики

```c
typedef enum inv_value_type
{
    INV_VALUE_I64,
    INV_VALUE_F64,
    INV_VALUE_STRING,
    INV_VALUE_BYTES
} inv_value_type;
```

Метрика содержит:

```text
Name (queriable name)
Type (f64, i64, string[MAX_SIZE](with,Length))
Description (Text/Language)
Documentation (path to md file)
Unit (Hz, ms,....)
Refresh Policy (ONCE, POLL, EVENT)
Refresh Cost (SENSITIVE, EXPENSIVE, ESTIMATED)
Provider (labels of supported providers)
```

---

## 5. Значение метрики

```c
typedef struct metric_value
{
    u8 type;
    u32 flags;

    union
    {
        int64_t s64;
        double f64;

        struct
        {
            size_t size;
            char data[MAX_LEN];
        } string;

        struct
        {
            size_t size;
            uint8_t data[MAX_LEN];
        } bytes;

    } as;

} metrci_value;
```

В дальнейшем flags можно использовать как:

```text
errors
status
quality
provider
```

---

## 6. Provider

Одна метрика может иметь несколько источников в рамках одной платформы.

Например:

```text
sensor.temperature

providers:
    linux.hwmon
    quanta.driver
    linux.proc
```

Query может использовать provider по умолчанию или указать его явно.

Это позволяет сравнивать методы получения данных и использовать fallback.

---

## 7. Система опроса

Quanta не должна независимо читать каждый датчик при каждом запросе frontend.

Используется центральный scheduler.

Он:

1. анализирует активные query;
2. определяет необходимые метрики;
3. объединяет совместимые операции;
4. учитывает стоимость опроса;
5. обновляет данные с необходимой частотой;
6. сохраняет историю только там, где она нужна.

Например, один системный вызов может получить сразу набор CPU-показателей, после чего ядро распределит данные между несколькими метриками.

---

## 8. Query language

Язык запросов создаётся в стиле PromQL, но также описывает частоту сбора и способ отображения.

Пример:

```text
version 1;

mode gui;

every 1s;
history 5m;

layout grid(columns=3);

select cpu.utilization
    as gauge(min=0, max=1, title="CPU busy");

select os.memory.utilization
    as bar(min=0, max=1, title="RAM");

select os.memory.available_bytes
    as value(unit="GiB", title="Available RAM");

select cpu.utilization
    as plot(min=0, max=1, title="CPU history");

select optional sensor.temperature
    as plot(title="Temperature")
    every 2s;

select optional rate(device.net.rx_bytes_total)
    as plot(unit="KiB/s", title="Receive");

select os.name
    as text(title="Operating system");
```

Ключевые элементы:

```text
select
optional
every
history
rate()
provider()
labels
mode
layout
```

---

## 9. Режимы работы

Основная программа может работать в нескольких режимах.

### Live

Сбор метрик с текущей системы.

```bash
quanta query.q
```

### Offline

Чтение ранее записанного blob.

```bash
quanta query.q --input capture.qblob
```

### Record

Сбор метрик в файл.

```bash
quanta query.q --record capture.qblob
```

Таким образом frontend можно тестировать без доступа к реальному оборудованию.

---

## 10. Внешние устройства

HAL должен позволять подключать внешние источники:

```text
UART
JTAG
```

Внешний источник регистрирует метрики точно так же, как локальный provider.

---

## 11. Регистрация датчиков

Все поддерживаемые датчики компилируются в проект.

Единый registry может выглядеть логически так:

```c
#ifdef QUANTA_SENSOR_CPU_USAGE
    QUANTA_REGISTER(cpu_usage);
#endif

#ifdef QUANTA_SENSOR_MEMORY
    QUANTA_REGISTER(memory);
#endif

#ifdef QUANTA_SENSOR_HWMON
    QUANTA_REGISTER(linux_hwmon);
#endif
```

Набор определяется через CMake presets.

Например:

```text
linux-default
linux-minimal
linux-hwmon
windows-native
windows-driver
windows-full
```

---

## 12. Сборка

Система сборки — **CMake**.

Пример структуры проекта:

```text
quanta/
├── CMakeLists.txt
├── CMakePresets.json
│
├── core/
│   ├── include/
│   └── src/
│
├── hal/
│   ├── linux/
│   ├── windows/
│   └── external/
│
├── osal/
│   ├── linux/
│   └── windows/
│
├── sensors/
│   ├── cpu/
│   ├── memory/
│   ├── network/
│   ├── board/
│   └── temperature/
│
├── frontends/
│   ├── cli/
│   ├── csv/
│   ├── txt/
│   ├── tui/
│   └── gui/
│
├── query/
├── tests/
├── docs/
└── examples/
```

---

## 13. Тестирование

Используется **CTest**.

Тесты должны покрывать:

```text
metric registry
query parser
scheduler
history
unit conversion
blob reader/writer
provider selection
HAL mocks
OSAL
frontend formatting
```

Для тестирования оборудования должны существовать mock providers.

---

## 14. Документация метрик

У каждой метрики должно быть три уровня описания.

### Query name

```text
os.memory.available_bytes
```

### Короткое описание

```text
Количество физической памяти, доступной системе без swap.
```

### Полная документация

Markdown-файл:

```text
docs/metrics/os.memory.available_bytes.md
```

Он содержит:

- назначение;
- единицы измерения;
- источник;
- алгоритм вычисления;
- ограничения;
- поддерживаемые платформы;
- возможные providers;
- оценку точности.

---

## 15. Безопасность

Получение метрик не должно:

- менять состояние системы без необходимости;
- выполнять опасные записи в hardware registers;
- зависать при отсутствии устройства;
- блокировать систему надолго;
- требовать повышенных прав без причины.

Потенциально опасные или дорогие providers должны быть явно помечены.

---

## 16. Первая версия

Для первого рабочего прототипа достаточно реализовать:

### Core

- registry;
- value;
- provider;
- scheduler;
- history;
- простой query parser.

### Linux

```text
os.name
os.uptime
cpu.utilization
cpu.frequency_hz
os.memory.total_bytes
os.memory.available_bytes
os.memory.utilization
device.net.rx_bytes_total
device.net.tx_bytes_total
sensor.temperature
```

### Windows

Аналогичный базовый набор через доступные WinAPI/system interfaces.

### Frontend

Сначала:

```text
CLI
TXT
CSV
```

После стабилизации core:

```text
TUI
GUI
```

---

## 17. Итоговая концепция

Quanta должна быть не набором отдельных функций чтения железа, а **универсальной системой метрик**.

Главная цепочка:

```text
HAL / OSAL
     ↓
Providers
     ↓
Metric Registry
     ↓
Scheduler
     ↓
History / Processing
     ↓
Query
     ↓
Frontend
```

Такое разделение позволяет независимо развивать:

- способы получения данных;
- набор датчиков;
- query language;
- форматы экспорта;
- GUI и другие frontend;
- новые платформы.

При этом базовое ядро остаётся компактным, переносимым и написанным на чистом C.
