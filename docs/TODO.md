# TODO 0.4.0

## P0 — приёмка на целевых платформах

- [ ] ESP8266 SDK v3.4: clean cross-build/link, size/map, flash, serial log со всеми 9 MCU keys.
  Проверить длительный heap/min-heap, stack high-water mark, cold reboot, SDK threading.
- [ ] Проверить VDD33 только на плате с TOUT unconnected и PHY setting=255; сравнить с прибором.
- [ ] Windows x64 VS2022 Debug/Release и минимальный preset: native collectors, HAL GroupAffinity,
  CLI/TUI, optional GUI. CI workflow подготовлен, результат запуска не присваивается заранее.
- [ ] GUI Linux/Windows: window open/close, fonts, resizing, worker shutdown; зависимости опциональны.

## P1 — аппаратное направление

- [ ] Windows driver HAL: версионированный ограниченный IOCTL ABI, контроль доступа, read-only
  default, write policy и адресные allowlists. Сейчас никакого драйвера в проекте нет.
- [ ] Linux GPIO character-device primitives; platform register families при реальной доступности.
- [ ] Точная информация о разрешённых физических диапазонах без предположения «вся RAM безопасна».
- [ ] SMBIOS UEFI locator вне legacy window; корректный bounded incremental table read для SMBIOS3,
  чтобы не требовать readable maximum-size range целиком; аппаратная проверка текущего legacy path.
- [ ] ACPI decoder/metrics в sensors, использующие HAL primitives либо отдельный native method key.
- [ ] Новые аппаратные метрики с отличающимися method-specific именами и unit/accuracy metadata.

## P2 — исходный функциональный roadmap

- [ ] Blob recording/replay, versioned binary format, corruption/limits tests.
- [ ] File/Pipe ingestion измерений; UART/JTAG транспорт внешних устройств. Serial ESP output
  пока не является протоколом desktop acquisition.
- [ ] Event subscriptions, poll cost budget, shared multi-client scheduler.
- [ ] Универсальная bounded string/bytes history, schema/ABI version policy, exported CMake package.
- [ ] RSS/heap/stack/per-operation timing на MCU и desktop: воспроизводимый ресурсный отчёт.

## Не TODO: намеренно удалённое

Providers, provider fallback/priority, several runtime implementations under one metric key
не должны возвращаться скрыто. Выбор low-level HAL — другая задача и уже реализован.
NoOS и ESP32 не объявляются портами этой ревизии только из-за наличия ESP8266/FreeRTOS.
