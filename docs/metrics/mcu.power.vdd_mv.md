# mcu.power.vdd_mv

## Назначение и источник

SDK возвращает единицы 1/1024 V. Версия семантики 2 переводит их в mV с округлением: (raw * 1000 + 512) / 1024. Raw 0 и 65535 дают NO_DATA. По умолчанию вызов отключён: UNSUPPORTED, а не фиктивный ноль. Для включения нужны CONFIG_QUANTA_MEASURE_VDD33=y, PHY vdd33_const=255 и физически неподключённый TOUT; см. ../ESP8266.md.

Единица: `mV`. Источник: `esp_wifi_get_vdd33`. Платформа: **ESP8266_RTOS_SDK**.

Реализация: `sensors/platform/esp8266/mcu_metrics.c` + `sensors/platform/esp8266/mcu.c`. Вызовы SDK выполняются непосредственно кодом метрик; HAL и OSAL не содержат сервисов получения MCU-метрик. Все выбранные поля MCU используют общий снимок при работе через engine. В low-memory `quanta_metric_read_once` каждый вызов имеет собственный жизненный цикл.

## Запрос

```text
once; hal "none"; select mcu.power.vdd_mv;
```

Этот запрос применим к встроенному C API с ESP-каталогом; desktop CLI не опрашивает ESP по UART. Наличие UART/JTAG-клиента на компьютере не заявляется. Справка: [ESP8266](../ESP8266.md).

## Проверки

SDK seam-тесты `quanta_esp8266_enabled_tests` и `quanta_esp8266_disabled_tests` компилируют тот же код на хосте. Проверка на реальной плате остаётся отдельным этапом приёмки.
