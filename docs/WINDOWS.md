# Windows: устройство порта, воспроизведение и ограничения

Основные команды сборки и запуска находятся в [README](../README.md). Эта страница
дополняет их техническими деталями. INV 0.2.1 уже был нативно собран пользователем
MSVC как в CLI/TUI, так и в GUI-конфигурации с настоящим Dear ImGui; существенная часть
Windows runtime-тестов прошла. INV 0.2.2 содержит исправления по этому прогону и требует
повторной native проверки. Фактические результаты и границы подтверждённого перечислены
в [TEST_REPORT](../TEST_REPORT.md).

## Сборочная матрица для воспроизведения на Windows

Базовые конфигурации: Visual Studio 2022 v143 или более новый MSVC, x64 и Win32, Release, GUI OFF/ON.
Сначала CLI+TUI без сети; затем GUI с pinned ImGui. CMake `INV_BUILD_TESTS=ON` добавляет
C-тесты; найденный Python добавляет файловые black-box-тесты и GUI smoke.
Для Debug повторите `--config Debug` / `ctest -C Debug`, не перемешивая CRT библиотек.
Для строгого тестирования предупреждений можно включить `INV_WARNINGS_AS_ERRORS=ON`;
MSVC 19.51/19.44 уже использовался в пользовательском native прогоне; `INV_WARNINGS_AS_ERRORS=ON` следует проверить отдельно для 0.2.2.

Минимальная конфигурация:

```powershell
cmake -S . -B build-small -G "Visual Studio 17 2022" -A x64 -DINV_WITH_GUI=OFF -DINV_WITH_TUI=OFF -DINV_WITH_MEMORY=OFF -DINV_WITH_CPU=OFF -DINV_WITH_DMI=OFF -DINV_WITH_NETWORK=OFF -DINV_WITH_HWMON=OFF -DINV_WITH_CPUID=OFF -DINV_WITH_FILESYSTEM=OFF -DINV_WITH_PROCESS=OFF -DINV_EMBED_DOCS=OFF
cmake --build build-small --config Release --parallel
ctest --test-dir build-small -C Release --output-on-failure
```

MinGW-w64 (из соответствующей shell с compiler в PATH):

```text
cmake -S . -B build-mingw -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DINV_WITH_GUI=OFF -DINV_STRICT_STATIC=ON
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
python tools/verify_windows_imports.py build-mingw/inv.exe --profile mingw
```

Linux cross-toolchain recipe: `cmake/toolchains/mingw64.cmake`. Он требует настоящий
установленный MinGW и не является эмулятором WinAPI. Cross-built PE надо исполнять
и тестировать на Windows. В среде подготовки этот toolchain отсутствовал.

## Структура Windows-кода

| Слой | Файлы |
|---|---|
| ОС/потоки | src/platform/windows.c, thread_windows.c |
| Unicode CLI | src/frontends/cli/windows_entry.c |
| Реестр | metrics/windows/*.def |
| Данные | src/providers/windows/*.c |
| PDH localization/wildcards | src/providers/windows/pdh_set.c |
| SMBIOS и формулы | src/providers/common/smbios.c, windows_math.c |
| TUI transport | src/frontends/tui/terminal_windows.c |
| GUI transport | src/frontends/imgui/win32_platform.c |
| Manifest/версия | resources/inv.manifest, inv.rc |

Статические поля кешируются сеансом; `r` вызывает refresh/re-discovery. Поиск устройств
в live-сессии периодический. Все selected поля одного провайдера читаются из его
снимка, но несколько WinAPI внутри провайдера не дают аппаратно атомарный снимок.
Внешние программы, оболочки, сеть для телеметрии и автоматическое elevation не используются.
OS performance API может привлекать установленные системные counter providers: это
не произвольный плагин INV и не обещание, что Windows никогда ничего не загружает.

## Ручной TUI/GUI checklist

Откройте TUI в отдельном обычном console-окне. Проверьте кириллическое имя интерфейса,
изменение размеров, ↑/↓, PgUp/PgDn, Home/End, `/` и Backspace для UTF-8-фильтра,
пауза/resume, `r`, `q` и Ctrl+C. После выхода исходный экран/курсор/ввод должны быть
восстановлены. Проверьте отдельно Windows Terminal/ConPTY и legacy conhost; результаты
одного terminal host не доказывают поддержку другого. Закрытие окна терминала/TerminateProcess
может не дать процессу завершить cleanup — это не тест обычного выхода.

В GUI проверьте таблицу/карточки, графики, круговой индикатор, resize, минимизацию,
закрытие крестиком и Alt+F4, pause и refresh. Для кириллицы укажите локальный TTF.
Проверьте выбранный DPI; backend масштабируется по system DPI, без динамической
перестройки атласа при переносе между мониторами с разным DPI. Максимальная поверхность
4096×4096, software rendering нагружает CPU; история имеет ограничение памяти.

## Доступность и семантика

Физические температуры CPU/GPU могут требовать аппаратно-специфического драйвера.
Здесь использованы только опубликованные Windows thermal-zone counters. Отсутствие
счётчика — штатный случай, не ошибка установки. Не следует заменять его WMI ACPI
температурой и называть её CPU без отдельной идентификации датчика.

Для малого набора CPU используется GetSystemTimes, включая idle в kernel. Для многих
групп PDH Processor Information / % Processor Time. После ошибок/сброса/первого снимка
rate/utilization проходит WARMUP. Raw CPU counters из Windows имеют 100-нс единицы.
Windows commit и Linux swap — разные сущности. Файловая система root — Windows volume
installation с квотой вызывающего пользователя, а не общая статистика всех дисков.

Прошивка может заполнять SMBIOS «To be filled by O.E.M.» или повторять серийники;
программа возвращает источник с его ограничениями, не «исправляет» данные догадкой.
`--redact` помечает serial/UUID/MAC/hostname как чувствительные, но не является полной
анонимизацией всех меток и названий устройств. По умолчанию ничего не отправляется.

PDH-счётчики запрашиваются нейтральными английскими именами, затем локализуются
PdhAddEnglishCounterW → PdhGetCounterInfoW → PdhExpandWildCardPathW → PdhAddCounterW.
Это не hardcode русских названий. Возможные ошибки провайдеров сохраняют свой статус.
Универсального жёсткого прерывания зависшего системного вызова не реализовано.

## Источники семантики API

- Raw SMBIOS: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable
- Physical memory: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex
- System commit: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-performance_information
- CPU/group limitation: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes
- Locale-neutral PDH: https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw
- PDH formatted values: https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcountervalue
- NIC tables: https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2
- Thermal-zone units: https://learn.microsoft.com/en-us/windows-hardware/design/device-experiences/examples--requirements-and-diagnostics
- Disk/quota: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw
- Native console buffers: https://learn.microsoft.com/en-us/windows/console/createconsolescreenbuffer
- DLL import semantics: https://learn.microsoft.com/en-us/windows/win32/dlls/about-dynamic-link-libraries
