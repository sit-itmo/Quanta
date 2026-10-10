# Ограничения безопасности baseline

Текущие collectors не устанавливают driver, не меняют MSR/PortIO/physical memory и не вызывают
внешние shell/monitor tools для получения метрик. `/etc/os-release` разбирается как текст, не shell.
Default Windows manifest — asInvoker. Недоступные поля возвращают статусы; root/admin не требуется
для обычной базовой проверки и не используется автоматически.

Чтение procfs/sysfs, PDH, firmware и иных native interfaces не гарантирует безусловную мгновенность.
Общий engine stop кооперативный; он не прерывает произвольный заблокированный kernel/API вызов.
Политика ограничений дорогих/потенциально blocking providers требует дальнейшей работы.

Экспорт может содержать hostname, серийные номера, UUID, MAC, interface labels и другие сведения.
`--redact` применён к SENSITIVE descriptor values, это **не** полный anonymizer. Проверяйте
экспорт перед публикацией; логи из пользовательских реальных машин по умолчанию в архив не добавлены.

Выходные файлы создаются без перезаписи существующих. Provider/query строки проходят bounded
validation и безопасное форматирование; это не доказательство отсутствия всех уязвимостей.
Sanitizer и negative fixture tests покрывают выполненные пути, не произвольное устройство/драйвер.

Подключение нового driver/external provider требует explicit capabilities, timeout/cancel contract,
read-only default и независимых mocks. Привилегированные записи не являются обычной метрикой.
