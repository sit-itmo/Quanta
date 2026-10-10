# device.net.tx_link_bps

Transmit link speed.

Единица: `bit/s`. Источник: `GetIfTable2 (64-bit counters; GUID+LUID identity)`.
Метки: `ifname,ifindex,guid`. Тип и расписание берутся из скомпилированного реестра;
команда `inv describe device.net.tx_link_bps` показывает актуальные метаданные сборки.

## Windows (0.2)

Один GetIfTable2 обслуживает все выбранные интерфейсы. Из user-facing discovery исключаются
строки с MIB_IF_ROW2.InterfaceAndOperStatusFlags.FilterInterface: WFP/Npcap/VirtualBox и другие
NDIS filter modules не выдаются как отдельные NIC. Физические и логические интерфейсы, включая
виртуальные/VPN/loopback, остаются видимыми. Метки: ifname (Alias, UTF-8), ifindex,
guid. История привязана к GUID + NET_LUID, а не к имени или позиции перечисления.
Переименование меняет метки; изменение LUID создаёт новую серию. Повторное использование
той же идентичности драйвером нельзя безусловно обнаружить: уменьшение счётчика сбрасывает rate.
RX/TX bytes — InOctets/OutOctets, packets = unicast + non-unicast; errors — InErrors/OutErrors.
Счётчики — uint64; rate делит целочисленную разность на реальное монотонное время.
MAC — текущий PhysicalAddress, не обещание аппаратного permanent address; у некоторых интерфейсов
его нет (NO_DATA). Link speed — ReceiveLinkSpeed/TransmitLinkSpeed, сообщённые драйвером,
не измеренная пропускная способность. Нулевое значение link speed сохраняется как ответ API.

Описание API: https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2
