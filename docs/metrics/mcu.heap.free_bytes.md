# mcu.heap.free_bytes

## Назначение и источник

Свободный heap по SDK на момент общего снимка. Это не вся физическая SRAM.

Единица: `B`. Источник: `esp_get_free_heap_size`. Платформа: **ESP8266_RTOS_SDK**.

Реализация: `sensors/mcu/metrics.c` + `sensors/mcu/esp8266.c`. Вызовы SDK выполняются непосредственно кодом метрик; HAL и OSAL не содержат сервисов получения MCU-метрик. Все выбранные поля MCU используют общий снимок при работе через engine. В low-memory `quanta_metric_read_once` каждый вызов имеет собственный жизненный цикл.

## Запрос

```text
once; hal "none"; select mcu.heap.free_bytes;
```

Этот запрос применим к встроенному C API с ESP-каталогом; desktop CLI не опрашивает ESP по UART. Наличие UART/JTAG-клиента на компьютере не заявляется. Справка: [ESP8266](../ESP8266.md).

## Проверки

SDK seam-тесты `quanta_esp8266_enabled_tests` и `quanta_esp8266_disabled_tests` компилируют тот же код на хосте. Проверка на реальной плате остаётся отдельным этапом приёмки.
