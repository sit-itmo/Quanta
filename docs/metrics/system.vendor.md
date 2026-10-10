# system.vendor

System manufacturer.

Единица: `без единицы`. Источник: `GetSystemFirmwareTable RSMB / bounded SMBIOS parser`.
Метки: `нет`. Тип и расписание берутся из скомпилированного реестра;
команда `quanta describe system.vendor` показывает актуальные метаданные сборки.

## Windows backend (требует нативной проверки)

Данные приходят из GetSystemFirmwareTable(RSMB), без dmidecode, WMI, PowerShell и драйвера.
Парсер ограничивает длины и проверяет структуру, строки и двойные NUL. SMBIOS types 0/1/2/17
дают BIOS, систему, первую baseboard и установленные модули памяти соответственно.
Это сведения, сообщённые прошивкой; их точность, уникальность и наличие не гарантируются.
Заполнители производителя сохраняются дословно; неизвестные поля не становятся нулевыми значениями.
Структурная ошибка всей таблицы не выдаёт ранее разобранные поля как достоверный снимок.
UUID использует порядок байтов SMBIOS 2.6+; all-zero / all-FF означает NO_DATA.
Пустые слоты type 17 не создают DIMM-серий. Метки модулей: handle, locator, bank.
size_bytes — ёмкость установленного устройства, не доступная ОС память; speed_mt_s —
Speed/Extended Speed из type 17 в MT/s (заявленная возможность, не текущая частота и не Configured Speed).
Серийники и UUID имеют флаг SENSITIVE. Идентичность DIMM по handle устойчива только в рамках
текущей таблицы прошивки/сеанса, не является глобальным идентификатором между компьютерами.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable
