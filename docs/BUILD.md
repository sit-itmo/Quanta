# Сборка и зависимости

Windows: [основной README](../README.md), [WINDOWS.md](WINDOWS.md).
CMake 3.21+, C99 (MSVC /std:c11). Следующий раздел относится к Linux.

Все библиотеки собственного проекта — STATIC. Исполняемый файл получает
`-static` при `INV_STRICT_STATIC=ON`, включая GUI. Ни OpenGL, ни dlopen, ни
xcb_connect (с DNS/NSS) не используются. XCB-адаптер открывает локальный Unix
socket, читает MIT-MAGIC-COOKIE-1 и вызывает xcb_connect_to_fd. Для GUI ищется
именно libxcb.a, не shared library.

| Опция CMake | Default |
|---|---|
| INV_WITH_GUI | OFF |
| INV_WITH_TUI | ON |
| INV_WITH_MEMORY / CPU / DMI / NETWORK / HWMON | ON |
| INV_WITH_CPUID / FILESYSTEM / PROCESS | ON |
| INV_STRICT_STATIC | ON |
| INV_EMBED_DOCS | ON |
| INV_BUILD_TESTS | ON |
| INV_WARNINGS_AS_ERRORS | OFF |
| INV_SANITIZE | OFF |
| INV_FETCH_IMGUI | ON при GUI |

CPUID компилируется только на x86. OS-провайдер обязателен. Выключенные
провайдеры исключаются из списка источников и реестра. Встроенные документы
также условные, в соответствии с включёнными группами.

Документы скомпилированы заранее в `src/core/docs.c`; Python при обычной сборке
не нужен. После редактирования Markdown выполните `python3 tools/embed_docs.py`.
Список метрик авторитетен в `.def`; `metrics/catalog.json` — исторический Linux-каталог для документации,
он не загружается исполняемой программой. CI/проверки должны держать их согласованными.

Для системного install:

```bash
cmake --install build --prefix "$HOME/.local"
```

Программа не нуждается в install: достаточно `build/inv`. `--output` никогда
не перезаписывает существующие файлы. Тесты fixtures создают временные каталоги
в /tmp и удаляют их. Тесты не пишут в настоящие sysfs/procfs.

## GUI dependency

По умолчанию CMake загружает tarball точного Git commit Dear ImGui по HTTPS.
Он не использует ветку master/latest. Дополнительный integrity hash tarball не
задан: immutable revision — pin источников, но не замена независимой проверки
поставщика. Для строгого supply-chain/offline процесса предоставьте проверенный
`INV_IMGUI_DIR`, отключите fetch и зафиксируйте собственные контрольные суммы.
`tools/fetch_imgui.py` загружает только нужные исходники/лицензию и пишет их SHA256
в provenance.json; эти суммы вычисляются после загрузки, не являются внешним
доверенным эталоном.

## ASan/UBSan

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
 -DINV_SANITIZE=ON -DINV_STRICT_STATIC=OFF -DINV_WARNINGS_AS_ERRORS=ON
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
```

Санитайзерный бинарник не является статическим production-профилем. При проблемах
ASLR/PIE конкретного контейнера используйте clang либо отдельный non-PIE test build;
не отключайте реальные обнаруженные проверки памяти ради прохождения тестов.

## Отдельный тест XCB без загрузки ImGui

```bash
./tools/test_xcb.sh
```

Нужны gcc, libxcb1-dev, Xvfb, Python3. Тест создаёт отдельный Xvfb с cookie,
проверяет успешный вход, вывод пикселей и отказ без cookie. Он не проверяет
интеграцию с ImGui. `tests/gui_smoke.py` — отдельный тест уже собранного GUI в Xvfb.

## Ограничения бинарной переносимости

Результат для x86-64 не запускается на ARM. musl и glibc — разные toolchain/ABI;
при musl нужно пересобрать все внешние статические зависимости, включая XCB и
C++ runtime. Просто добавлять glibc `.a` к musl-сборке нельзя. Пресета musl SDK
в первой поставке нет. Исходники x86-CPUID отключаются на других архитектурах,
но ARM-сборка в этой поставке не тестировалась.
