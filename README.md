# INV 0.2.2 — Windows + Linux: инвентаризация и мониторинг

Windows-порт на общей кодовой базе INV 0.1: **70 семейств метрик Windows**, C99-ядро,
CLI, native Unicode TUI, human/table/CSV/Markdown и опциональный Dear ImGui GUI
с программным рендерером и Win32/GDI. Linux-порт сохранён, включая его 52 семейства.
Реестр формируется при компиляции; исполняемая программа не загружает плагины и не
запускает PowerShell, WMI-скрипты или сторонние утилиты для сбора данных.

> **0.2.1 hotfix:** исправлен порядок Windows networking headers для современных Windows SDK (`winsock2.h` → `ws2ipdef.h` → `iphlpapi.h`). Это устраняет ошибку `MIB_IF_TABLE2`/`MIB_IF_ROW2` undeclared, наблюдавшуюся в нативной MSVC-сборке 0.2.0.

> **0.2.2 native-Windows validation fixes:** исправлен тест live-session (`count` считает output frames, а не число samples), из сетевого discovery исключаются NDIS filter interfaces по `MIB_IF_ROW2.InterfaceAndOperStatusFlags.FilterInterface`, и исправлен Windows Markdown-пример `fs.root.*`.

**Это архив исходников, а не поставка готового EXE.** INV 0.2.1 уже был нативно
собран пользователем на Windows двумя MSVC toolchain: CLI/TUI-профиль дал рабочий
`inv.exe`, а GUI-профиль действительно скомпилировал Dear ImGui и прошёл `windows_gui`.
Лог также подтвердил работу inventory, памяти, SMBIOS DIMM, CPU utilization и файловых
экспортов. INV 0.2.2 содержит точечные исправления по этому native run; именно 0.2.2
ещё нужно повторно прогнать на Windows. В среде подготовки Windows SDK/MSVC по-прежнему
нет, поэтому Linux-regression и Windows schema/data checks не подменяются заявлением о
нативной 0.2.2 валидации. Подробный статус — [TEST_REPORT.md](TEST_REPORT.md).

## 1. Сборка Windows: CLI + TUI, без внешних библиотек

Для первой сборки используйте Windows 10/11, **Visual Studio 2022 или новее**
с компонентом «Разработка классических приложений на C++» и Windows SDK, CMake 3.21+.
Python 3.8+ необязателен для сборки/запуска: он добавляет интеграционные тесты и
инструменты документации. Ядро использует подмножество C99; MSVC получает /std:c11
ради соответствующего современного C frontend. v141/v141_xp и XP не поддерживаются.

Откройте PowerShell в каталоге распакованного проекта:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DINV_WITH_GUI=OFF -DINV_STRICT_STATIC=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

.\build\Release\inv.exe --version
.\build\Release\inv.exe list --compiled
.\build\Release\inv.exe run --query-file queries/windows/01-inventory.invq --redact
```

Для 32-битного EXE используйте отдельный каталог и `-A Win32`. x64 и Win32 нельзя
переключать в одном CMake build tree. CMake генерирует настоящую `.sln` с проектами;
её также можно открыть в Visual Studio. Debug/Release выбираются через `--config`.

Альтернативы: `tools\build_windows.cmd OFF x64`, либо `./tools/build_windows.ps1`.
При запрете запуска PowerShell-скриптов используйте явные команды CMake; менять
политику исполнения ради проекта не требуется. Также поставляется CMakePresets.json.

## 2. GUI: Dear ImGui + CPU renderer + Win32/GDI

```powershell
cmake -S . -B build-gui -G "Visual Studio 17 2022" -A x64 -DINV_WITH_GUI=ON -DINV_STRICT_STATIC=ON
cmake --build build-gui --config Release --parallel
ctest --test-dir build-gui -C Release --output-on-failure

.\build-gui\Release\inv.exe run --query-file queries/windows/04-dashboard-gui.invq
.\build-gui\Release\inv.exe run --query-file queries/windows/12-gui-table.invq
```

**Первый GUI configure требует сети.** Dear ImGui в архив не включён: CMake загружает
v1.91.9b, commit `f5befd2d29e66809cd1110a152e375a7f1981f06`, а не master/latest.
Четыре исходника ImGui компилируются в STATIC library. Обычная CLI/TUI-сборка не
загружает ничего и не требует ImGui. Для изолированной GUI-сборки заранее перенесите
проверенную копию этой ревизии, например в `C:/deps/imgui`, и используйте:

```powershell
cmake -S . -B build-gui -G "Visual Studio 17 2022" -A x64 -DINV_WITH_GUI=ON -DINV_IMGUI_DIR=C:/deps/imgui -DINV_FETCH_IMGUI=OFF
```

GUI имеет карточки, числовые значения, полосы, круговые индикаторы, временные графики,
таблицу с сортировкой, фильтр, паузу отображения и сведения об источнике/серии.
Рисование примерно ограничено 30 кадрами/с и не вызывает опрос датчиков на каждом
кадре. Используется GDI DIB, не OpenGL/DirectX/GLFW/SDL/XCB. XCB нужен только Linux GUI.
Поддерживаются resize, клавиатура/мышь, Unicode input, system-DPI scaling. Это не
per-monitor-DPI renderer и не полноценный docking/multi-viewport backend.

Для кириллицы в GUI укажите собственный установленный TTF, например:

```powershell
.\build-gui\Release\inv.exe run --query-file queries/windows/04-dashboard-gui.invq --font C:/Windows/Fonts/segoeui.ttf
```

Путь должен существовать; шрифты не включены в архив. Набор доступных глифов зависит
от шрифта. Default ImGui font не содержит полного набора кириллицы. GUI не требует
администратора; EXE имеет manifest `asInvoker`. Сборка сохраняет консольную подсистему,
чтобы тот же EXE оставался удобным CLI и нормально работал в pipeline.

## 3. Примеры, которые стоит запустить

Во всех командах ниже `$Inv` — путь к выбранной CLI/TUI или GUI-сборке:

```powershell
$Inv = ".\build\Release\inv.exe"

# Короткий отчёт, память и модули памяти
& $Inv run --query-file queries/windows/01-inventory.invq --redact
& $Inv run --query-file queries/windows/02-memory.invq
& $Inv run --query-file queries/windows/10-dimm-inventory.invq --redact

# Интерактивные экраны
& $Inv run --query-file queries/windows/03-dashboard-tui.invq
& $Inv run --query-file queries/windows/07-network-tui.invq
& $Inv run --query-file queries/windows/08-cpu-cores.invq

# Запись на 30 секунд и одноразовый Markdown
& $Inv run --query-file queries/windows/05-record-csv.invq --output metrics.csv
& $Inv run --query-file queries/windows/09-report-markdown.invq --redact --output report.md

# Доступные thermal zones, только сам процесс, справка и план
& $Inv run --query-file queries/windows/06-thermal-zones.invq
& $Inv run --query-file queries/windows/11-process.invq
& $Inv describe windows.memory.commit.used_bytes
& $Inv explain --query-file queries/windows/03-dashboard-tui.invq
& $Inv get cpu.utilization --warmup 1200ms --format table
```

`--output` создаёт **только новый файл**; повторный запуск с существующим именем
завершится ошибкой без изменения файла. На Windows файл наследует обычные ACL
каталога (это не аналог POSIX 0600). Чувствительные отчёты сохраняйте в своём
профиле с подходящими правами. `--redact` маскирует помеченные значения, но не
обещает анонимизацию всех названий/меток интерфейсов.

CSV — UTF-8, десятичная точка, длинный формат с отдельными полями status/unit/time;
для точного машинного экспорта используйте `--output`, а не перекодирующие команды
оболочки. BOM в файле запроса допускается; встроенный NUL и слишком большие запросы
отклоняются. Диагностика идёт в stderr, не в CSV. Пути и аргументы преобразуются
между UTF-16 Windows и UTF-8 ядра. В строках INVQ обратный слеш экранируется: `\\`.
Проще использовать `C:/path/file.csv`; в отдельных CLI-аргументах Windows-путь
передаётся как обычный аргумент и дополнительного INVQ-экранирования не требует.

Короткий запрос из PowerShell:

```powershell
& $Inv run -q 'mode file; format table; every 1s; count 5; select cpu.utilization; select os.memory.used_bytes;'
```

Сложные запросы с внутренними кавычками удобнее хранить в `.invq`: правила передачи
кавычек native executables различаются между версиями PowerShell. Все 13 примеров
находятся в `queries/windows/`. Общая грамматика — [docs/INVQ.md](docs/INVQ.md).

## 4. TUI и управление

Windows TUI использует отдельный native console screen buffer и WriteConsoleOutputW:
ANSI/VT100, ncurses и смена языка системы не нужны. Кириллица передаётся как Unicode;
полная геометрия сложных emoji/combining/CJK-графем не обещается. При штатном выходе
восстанавливаются исходный активный экран и режим ввода. Принудительное убийство
процесса/закрытие консоли ОС не дают универсальной гарантии cleanup.

| Клавиша | Действие |
|---|---|
| q / Ctrl+C | Завершить |
| p | Пауза отображения; сбор продолжается |
| r | Перечислить устройства и обновить свойства |
| /, Enter, Esc | Ввод фильтра, применить, очистить |
| ↑/↓, j/k, PgUp/PgDn, Home/End | Прокрутка |
| s / ? | Сортировка / справка |

Запускайте TUI в обычном Windows console/PowerShell/cmd; GUI в локальном desktop-сеансе.
Для stdout-pipeline/редиректа TUI намеренно отклоняется — используйте FILE. Работа в
Windows Terminal/ConPTY и удалённых оболочках требует проверки конкретного хоста;
автоматического fallback с потерей интерактивных возможностей нет.

## 5. Особенности Windows-данных

| Группа | Источник и особенности |
|---|---|
| ОС | Win32 system APIs, registry ProductName/BuildLabEx, RtlGetVersion |
| Физическая память | GlobalMemoryStatusEx; used = total − available |
| Commit/cache/kernel memory | GetPerformanceInfo; commit не выдаётся за Linux swap |
| CPU | GetSystemTimes и PDH Processor Information; учтены processor groups |
| CPUID | Аппаратные bits одного закреплённого reference CPU, не разрешение ОС |
| Плата/BIOS/система/DIMM | Bounded C parser raw SMBIOS types 0/1/2/17 |
| Сеть | GetIfTable2, GUID/LUID, 64-bit counters и link speed; NDIS filter-module interfaces скрыты из пользовательского discovery |
| Температуры | Только доступные Windows thermal zones через PDH |
| Диск | Квотное представление тома установки Windows, не все тома |
| Процесс | Working set/private commit, handles и process I/O текущего INV |

`Temperature` thermal-zone counter измеряется в kelvin, в INV переводится в degC.
Зона не объявляется CPU package/core. Универсальные температуры CPU/GPU, вентиляторы,
SMART, EC/SMBus и чтение MSR в этом порте не реализованы: собственного привилегированного
драйвера здесь нет. Отсутствие зоны/серийника не превращается в успешный ноль/пустую строку.

На системах с несколькими CPU groups общая загрузка берётся через PDH, а сырые
`cpu.time_total`/`cpu.idle_total` возвращают UNSUPPORTED, чтобы не назвать одну группу
всем компьютером. На одно-групповых системах raw ticks имеют период 100 нс; частота
доступна как `windows.cpu.tick_frequency_hz`, а не заимствуется из Linux USER_HZ.
PDH English-counter + wildcard localization не требует английской Windows, но сами
объекты/экземпляры PDH могут быть отключены или недоступны.

Полный список: [docs/WINDOWS_METRICS.md](docs/WINDOWS_METRICS.md).
Семантика конкретного ключа встроена в `inv describe KEY`.

## 6. Статическая линковка и совместимость

В MSVC-профиле `INV_STRICT_STATIC=ON` выбирает `/MT` или `/MTd` для своего кода,
ImGui и C++ runtime. Отдельные DLL приложения/ImGui не создаются. **Системные DLL
Windows остаются обязательны:** WinAPI import libraries не встраивают Kernel32,
User32, GDI, PDH и прочие службы ОС в EXE. Статический MinGW также может импортировать
системный MSVCRT/UCRT в зависимости от toolchain; это отдельный профиль аудита.

```powershell
python tools/verify_windows_imports.py build/Release/inv.exe --profile msvc
# Либо из Developer Command Prompt:
dumpbin /DEPENDENTS build\Release\inv.exe
```

Python-проверка не запускает EXE и проверяет прямые/delay DLL imports. Она не доказывает
совместимость с любой версией Windows и не сканирует DLL, загружаемые провайдерами самой ОС.

Исходный API floor установлен `_WIN32_WINNT=0x0601` (Windows 7), но **рабочая
совместимость Windows 7/8/10/11 в этой среде не проверена** и зависит также от выбранного
CRT/toolchain. Основная конфигурация для пользовательской проверки — Windows 10/11 x64.
Windows XP, toolset v141_xp, Windows ARM64, старые MSVC и UWP не входят в подтверждённые
цели этой поставки. ARM64-ветви оставлены как заготовленный переносимый путь, не как
проверенный бинарник. Не объявляйте Windows 7 поддерживаемой целью без проверки EXE
на соответствующей системе с выбранным toolchain.

## 7. Разработка и проверки

Windows test targets: `unit`, `windows_data`, `windows_platform`, Python `integration`
и при GUI `windows_gui`. Последний создаёт настоящее окно, посылает resize/Unicode/close,
но не является визуальным UI-аудитом. Для TUI есть ручной checklist в `docs/WINDOWS.md`.
GitHub Actions workflow включён, но не запускался при подготовке архива.

Набор данных меняется переключателями `INV_WITH_MEMORY`, `INV_WITH_CPU`, `INV_WITH_DMI`,
`INV_WITH_CPUID`, `INV_WITH_NETWORK`, `INV_WITH_HWMON`, `INV_WITH_FILESYSTEM`,
`INV_WITH_PROCESS`; GUI/TUI/документы также отключаемы. OS-провайдер остаётся всегда.
Изменения Markdown: `python tools/embed_docs.py`, проверка: `python tools/embed_docs.py --check`.
Ни Python, ни C++ не нужны во время исполнения CLI/TUI.

Linux-сборка и прежние примеры: [README-LINUX.md](README-LINUX.md).
Общая архитектура: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
Лицензии и зависимости: [THIRD_PARTY.md](THIRD_PARTY.md). Свой код — MIT.
