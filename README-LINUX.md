# INV 0.2.2 — инвентаризация и мониторинг Linux

C99-ядро, реестр из **52 семейств метрик**, язык запросов INVQ, CLI, TUI,
CSV/Markdown и опциональный Dear ImGui GUI. Несколько представлений одной
метрики используют общий источник измерений. Опрос не зависит от частоты
перерисовки интерфейса.

**Статус проверок версии 0.2:** см. [TEST_REPORT.md](TEST_REPORT.md).
Это сохранённая инструкция Linux-порта; новые Windows-команды находятся в основном README.
Полная интеграция ImGui здесь не проверялась.

## 1. Быстрый старт на Ubuntu

Команды выполняются в каталоге распакованного проекта. Программа ничего не
устанавливает автоматически, не повышает права и не запускает внешние сборщики.

```bash
sudo apt update
sudo apt install build-essential cmake python3

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DINV_WITH_GUI=OFF -DINV_STRICT_STATIC=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./tools/verify_static.sh ./build/inv

./build/inv --version
./build/inv run --query-file queries/01-inventory.invq --redact
./build/inv run --query-file queries/02-memory.invq
```

Python нужен для дополнительных интеграционных тестов и вспомогательных
инструментов, а не для исполнения программы или обычной сборки C-ядра.
CMake — 3.21 или новее. Компилятор — с поддержкой C99, для GUI также C++11.
По умолчанию используется строгая статическая линковка. Если статическая
библиотека отсутствует, сборка завершается ошибкой, не переходит молча на `.so`.

## 2. TUI

```bash
./build/inv run --query-file queries/03-dashboard-tui.invq
./build/inv run --query-file queries/07-network-tui.invq
./build/inv run --query-file queries/08-core-cpu.invq
```

Работает в настоящем терминале с ANSI-последовательностями. При перенаправлении
stdout или `TERM=dumb` запуск отклоняется. Управление:

| Клавиша | Действие |
|---|---|
| `q` | Завершить |
| `p` | Заморозить/возобновить отображение; сбор продолжается |
| `r` | Перечислить устройства заново и обновить статические свойства |
| `/`, затем Enter | Фильтр по имени/меткам/заголовку |
| Esc | Снять фильтр / выйти из ввода |
| ↑/↓ или `k`/`j`, `g` | Прокрутка / начало |
| `s` | Изменить сортировку |
| `?` | Справка |

TUI использует ASCII-геометрию: не-ASCII символы в экранных строках заменяются
на `?`, чтобы не ломать ширину ячеек. UTF-8 сохраняется в отчётах и доступен в GUI
с подходящим шрифтом. `gauge` в терминале отображается полосой, а не кругом.

## 3. GUI: Dear ImGui + программный рендерер + XCB

Исходники приложения полностью включены. **Сами исходники Dear ImGui в архив
не включены**: первый GUI configure загружает строго заданную ревизию
`v1.91.9b`, commit `f5befd2d29e66809cd1110a152e375a7f1981f06`.
После загрузки пересборки возможны без сети.

```bash
sudo apt install libxcb1-dev
cmake -S . -B build-gui -DCMAKE_BUILD_TYPE=Release \
  -DINV_WITH_GUI=ON -DINV_STRICT_STATIC=ON
cmake --build build-gui --parallel
./tools/verify_static.sh ./build-gui/inv
./build-gui/inv run --query-file queries/04-dashboard-gui.invq
./build-gui/inv run --query-file queries/11-gui-table.invq
```

Вариант с явной загрузкой зависимости перед конфигурацией:

```bash
python3 tools/fetch_imgui.py
cmake -S . -B build-gui -DINV_WITH_GUI=ON -DINV_FETCH_IMGUI=OFF
cmake --build build-gui --parallel
```

Для изолированной машины перенесите исходники этой ревизии ImGui и укажите
`-DINV_IMGUI_DIR=/absolute/path/to/imgui -DINV_FETCH_IMGUI=OFF`.
Полный перечень зависимостей и лицензий: [THIRD_PARTY.md](THIRD_PARTY.md).

GUI создаёт настоящее ImGui-окно с карточками, линиями истории, круговым
индикатором, полосами, таблицей, фильтром, сортировкой, паузой и деталями серии.
Рисование выполняется на CPU примерно с ограничением 30 кадров/с. OpenGL,
GPU-драйверы, GTK, Qt, GLFW, SDL, ncurses и динамические плагины не используются.

Требуется доступный **локальный X11/Xwayland** с `DISPLAY=:N[.screen]` или
`unix:N[.screen]`, TrueColor 24-bit / 32-bit pixel storage. Wayland напрямую,
TCP/SSH-forwarded DISPLAY, IME и clipboard в Linux-порту не реализованы. XCB использует
Unix-сокет и MIT-MAGIC-COOKIE-1 из `XAUTHORITY` или `~/.Xauthority`.
**Не запускайте GUI через sudo** и не отключайте авторизацию командой `xhost +`.
На чистом Wayland без Xwayland используйте TUI/FILE.

Для кириллических подписей можно передать свой локальный TTF:

```bash
./build-gui/inv run --query-file queries/04-dashboard-gui.invq \
  --font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

Этот файл не поставляется с проектом; путь — пример, он должен существовать.
Доступность символов определяется выбранным шрифтом.

## 4. Отчёты и запись CSV

```bash
./build/inv list --compiled
./build/inv list 'sensor.*' --probe
./build/inv describe os.memory.used_bytes
./build/inv get os.name os.kernel.release cpu.vmx --format table
./build/inv get cpu.utilization --warmup 1200ms --format table

./build/inv run --query-file queries/05-record-csv.invq --output metrics.csv
./build/inv run --query-file queries/09-report-markdown.invq \
  --redact --output report.md
./build/inv run --query-file queries/06-sensors.invq
```

`--output` создаёт **только новый** файл с правами `0600`. Существующий файл
не перезаписывается. При повторе задайте другое имя или осознанно удалите старый.
Без `--output` данные идут в stdout, диагностика — в stderr.

CSV — длинный формат: одна строка на новый результат отдельной серии. Первое
значение `rate()` и `cpu.utilization` обычно имеет статус `WARMUP`, а не ноль.
`--warmup` предназначен только для одноразового отчёта: прогрев подавляет
промежуточный вывод. CPU и скорость сети измеряются по фактическому интервалу.

```bash
./build/inv run -q 'mode file; format csv; every 500ms; count 10; select cpu.utilization; select os.memory.used_bytes;'
./build/inv run -q 'mode file; format table; once; select optional sensor.temperature{chip="coretemp"};'
./build/inv explain --query-file queries/03-dashboard-tui.invq
```

`count` — число кадров выдачи, не обязательно число измерений каждой серии.
При разных интервалах у серий будет разное число CSV-строк. Для записи заданного
периода предпочтительнее `for 30s` / `--duration 30s`.

## 5. Что собирается

| Группа | Примеры |
|---|---|
| ОС | Название, ядро, архитектура, hostname, uptime, load average, online CPUs |
| Память | Total / available / free / used, доля использования, swap |
| CPU | Общая и покомпонентная загрузка, накопленные CPU ticks |
| CPUID (x86) | Vendor/brand, VMX/SVM, SSE2/AVX/AVX2, hypervisor bit |
| DMI | Производитель/модель/серийник платы, BIOS vendor/version/date |
| Сеть | MAC, состояние, байты/пакеты/ошибки, rate/delta |
| hwmon | Доступные стандартные каналы температуры и вентиляторов |
| Файловая система | Размер, доступное и занятое место корневой ФС |
| Сам процесс | Приблизительный RSS процесса inv |

52 — количество **семейств** при включённых x86-провайдерах. Экземпляров может
быть больше: каждое ядро, сетевой интерфейс и канал датчика — отдельная серия.

Не все сведения доступны на всех машинах. `board.serial` часто требует прав,
а в VM/контейнере может вовсе отсутствовать. Это отражается статусом.
Отсутствие hwmon не заменяется вымышленной температурой. VMX/AVX — признаки
оборудования на выбранном разрешённом логическом CPU, **не** разрешение их
использования ОС. Для CPUID выведен `cpu.cpuid_reference`.

Память/CPU из `/proc` могут описывать хост, а не лимит контейнера. В v0.1 нет
cgroup-нормализации. Формулы и ограничения: [docs/metrics/INDEX.md](docs/metrics/INDEX.md).

## 6. Сборочные профили и разработка

```bash
# Малый бинарник; OS-провайдер остаётся всегда.
cmake -S . -B build-small -DINV_WITH_TUI=OFF -DINV_WITH_GUI=OFF \
  -DINV_WITH_DMI=OFF -DINV_WITH_HWMON=OFF -DINV_WITH_NETWORK=OFF \
  -DINV_WITH_CPUID=OFF -DINV_EMBED_DOCS=OFF
cmake --build build-small --parallel

# Проверки памяти: санитайзеры требуют нестрогой статической сборки.
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DINV_SANITIZE=ON -DINV_STRICT_STATIC=OFF -DINV_WITH_GUI=OFF
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

Все переключатели, граничные случаи и формат экспорта описаны в
[BUILD.md](docs/BUILD.md), [INVQ.md](docs/INVQ.md),
[ARCHITECTURE.md](docs/ARCHITECTURE.md), [EXPORT.md](docs/EXPORT.md) и
[EXTENDING.md](docs/EXTENDING.md). Готовые запросы находятся в `queries/`.

Статический ELF переносим в пределах совместимой архитектуры/ABI и системных
требований использованного toolchain. Это не обещание «один бинарник для любого
Linux». Для распространения на старых системах собирайте на соответствующем
старом toolchain и проверяйте на целевой системе. Эта поставка содержит исходники,
а не распространяемый готовый glibc-бинарник.

Лицензия своего кода — MIT. Перед распространением собственного статического
бинарника проверьте также требования лицензий всех слинкованных библиотек.
