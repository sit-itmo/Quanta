# mcu.cpu.freq_mhz

## Назначение и источник

Сконфигурированная частота configCPU_CLOCK_HZ / 1000000. Это не измерение частоты и не getter runtime-переключений. Значение помечено QUALITY_ESTIMATED.

Единица: `MHz`. Источник: `configCPU_CLOCK_HZ`. Платформа: **ESP8266_RTOS_SDK**.

Реализация: `sensors/platform/esp8266/mcu_metrics.c` + `sensors/platform/esp8266/mcu.c`. Вызовы SDK выполняются непосредственно кодом метрик; HAL и OSAL не содержат сервисов получения MCU-метрик. Все выбранные поля MCU используют общий снимок при работе через engine. В low-memory `quanta_metric_read_once` каждый вызов имеет собственный жизненный цикл.

## Запрос

```text
once; hal "none"; select mcu.cpu.freq_mhz;
```

Этот запрос применим к встроенному C API с ESP-каталогом; desktop CLI не опрашивает ESP по UART. Наличие UART/JTAG-клиента на компьютере не заявляется. Справка: [ESP8266](../ESP8266.md).

## Проверки

SDK seam-тесты `quanta_esp8266_enabled_tests` и `quanta_esp8266_disabled_tests` компилируют тот же код на хосте. Проверка на реальной плате остаётся отдельным этапом приёмки.
