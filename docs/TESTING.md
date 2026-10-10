# Тесты и проверка поставки 0.5.0

## Запуск

```bash
cmake --preset linux-default
cmake --build --preset linux-default --parallel   # уже запускает CTest
ctest --preset linux-default --output-on-failure # повторный прогон
cmake --build build/linux-default --target check
```

Windows: те же команды с preset `windows-native`. Для Debug:
`ctest --test-dir build/windows-native -C Debug --output-on-failure`.
`BUILD_TESTING=ON` требует Python 3; CMake не выдаёт сокращённый набор за полную проверку.
Сборка одного target `quanta` либо `QUANTA_TESTS_ON_BUILD=OFF` не запускает ALL-проверку.

## Организация без дополнительного test framework

```text
tests/
  unit/                 catalog, core/metrics, history, HAL, OSAL
  platform/linux/       parsing, discovery, идентичность устройств
  platform/windows/     native tests, pure-data tests, catalog schema
  platform/esp8266/     SDK seam и stubs, только на host
  integration/          единый CLI/HAL/format тест, native smoke, TUI/GUI
  source_contract.py    границы include/runtime, расположение query и MCU
```

Небольшие C suites возвращают обычный код завершения; проверки не зависят от NDEBUG.
CTest — единственный runner. Core-only tests (`catalog/history/osal`) намеренно не линкуют
sensors/frontends. Windows schema на Linux проверяет только каталог и примеры query;
это не эмуляция Windows и не исполнение WinAPI.

CLI/HAL/format проверки объединены в один `integration/cli.py` без повторных запусков
четырёх почти одинаковых программ. Проверяются режимы и их overrides, недоступный GUI/TUI,
HAL none/native/неизвестный, форматы, Unicode query/output path, stdin, NUL/лимиты,
запрет перезаписи output и отсутствие старых product-копий в bin.

OSAL suite проверяет opaque lifecycle, mutex/condition/thread, repeated cleanup,
calloc overflow, realloc preservation, семантику fread/fwrite по элементам, seek/tell,
EOF/clearerr, text boundaries/NUL, часы, UTC leap-year и предельный uint64 timestamp.
`source_contract.py` ищет прямые libc/native вызовы в переносимых слоях и утечку native
headers в public API. Это полезный regression check, **не формальное доказательство**
полноты зависимостей и не полноценный C parser.

ESP VDD-enabled/disabled targets используют тот же source/ABI manifest, что SDK-компонент,
включая query, app_main и serial formatter. Stubs заменяют SDK-вызовы, а не scheduler.
Бесконечный serial loop компилируется, но не запускается на host. Сохраняются все 9 MCU keys;
в каталоге дополнительно 9 OS и 4 общих descriptor. При отсутствии файловой системы stdio
helpers теперь возвращают UNSUPPORTED из ENOSYS, не маскируют его как отсутствующий файл.

## Выполнено для этой ревизии

Рабочая среда: Linux x86-64, GCC 14.2.0, Clang 17.0.0, CMake 3.31.6, Ninja 1.12.1,
Python 3.13.5. Это сведения о тестовой среде, не обязательные точные версии для сборки.

| Конфигурация | Результат |
|---|---|
| GCC Debug, native Linux + TUI, warnings-as-errors | 16/16 |
| Clang Release, native Linux + TUI, warnings-as-errors | 16/16 |
| Clang Debug + ASan/UBSan, leak detection + halt-on-error | 16/16 |
| GCC Release, linux-minimal, GUI/TUI OFF | 15/15 |
| GCC Debug, portable contract, GUI/TUI OFF | 9/9 |
| GCC Release, strict static, без GUI | 16/16; нет PT_INTERP/DT_NEEDED |

В указанных sanitizer-прогонах ошибок не обнаружено. Это не гарантия отсутствия всех ошибок.
Сравнены полностью раскрытые дескрипторы с 0.4.0: Linux 54 → 58, Windows schema 72 → 76,
ESP8266 18 → 22; удалённых ключей нет. В каждом наборе добавлены только четыре общие метрики.
Негативная компиляция с PHY VDD33=254 отклонена предусмотренным #error.
OSAL suite: 69 проверок; SDK seams: 122 с VDD и 115 без VDD; единый CLI: 41 командный случай
для протестированной native сборки GUI=OFF/TUI=ON (число зависит от compiled modes).
В minimal несколько platform suites выполняют только проверки включённых групп.

Дополнительно: product-only конфигурация без тестов и UI не ищет Python/C++ compiler;
установка проверена, отдельный C-клиент линкует установленный core/query без sensors/UI.
14 public headers отдельно скомпилированы в C11 и C++17 (28 запусков),
единственный project include path — `include`. Для portable executable проверен список
неразрешённых символов: нет прямых зависимостей от getpid/sigaction/uname/sysconf/socket
или native metric API. ABI-symbol aliases libc зависят от toolchain: это не обещание запуска
полученного бинарника на старой glibc. XCB window backend отдельно компилируется с -Werror,
но эта компиляция не считается тестом всего ImGui frontend.

## Не проверено / не заявляется

**Windows нативно не запускался.** CI и инструкции подготовлены; pure-data/schema tests
не заменяют MSVC Debug/Release, Windows TUI и processor-group affinity.

**Нет реальной SDK/Xtensa-сборки и прошивки ESP8266.** Нужны build/link/size/map, плата,
serial, heap/stack и длительный прогон; подробности в [ESP8266](ESP8266.md).

**Весь ImGui GUI не собирался и окно программы не проверялось.** Исходники зависимости
в среде отсутствовали; попытка явного получения не удалась из-за недоступности DNS/сети.
Проверены AUTO-отключение и явная ошибка при GUI=ON без зависимости. Linux GUI smoke исправлен:
он создаёт собственный Xvfb, а не вызывает отсутствующий вспомогательный script. Его полный
GUI-сценарий ещё требует прогона с настоящим ImGui. Это не screenshot/визуальная приёмка.

Аппаратные memory/MSR/PCI/GPIO операции проверяются fixtures, не реальными опасными записями.
Ни тесты, ни сборка не требуют root и не загружают драйверы.

## Проверка архива

При выпуске создаются MANIFEST.sha256 (файлы дерева, кроме самого манифеста) и внешний SHA256
ZIP. ZIP распаковывается в новый путь с пробелами, манифест сверяется, выполняется чистая
configure/build с автоматическими тестами и повторный CTest. Логи проверки архива и матрицы
прилагаются отдельно, чтобы не держать в исходном дереве историю старых прогонов и build outputs.

Документация embedded-метрик проверяется `python3 tools/embed_docs.py --check`.
Исходный `docs/Quanta_Design.md` в этой поставке побайтно совпадает с документом пользователя;
текущая архитектура описана отдельно, а не незаметно переписана в исходном документе.
