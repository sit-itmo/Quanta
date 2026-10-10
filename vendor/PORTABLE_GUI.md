# Адаптация переданного portable_gui

Источник: пользовательский `portable_gui.zip`. Два относящихся к задаче оригинала
сохранены побайтно в `portable_gui_reference/` для сравнения; **не компилируются**.
В архиве отсутствуют требуемые ими `std.h`, `gui_framebuffer.h`, `gui_os.h`,
`imgui_backend_framebuffer.h` и отдельная лицензия. Поэтому ZIP не является
самостоятельно собираемой библиотекой, которую достаточно добавить в CMake.

## Что перенесено в активный код

`frontends/gui/backend/portable_quad.cpp` использует адаптированные функции
`fb_try_make_quad_direct`, `fb_try_make_quad`, `fb_assign_quad_corner`, сравнение
координат и `fb_fill_u32_row` из `imgui_backend_framebuffer.cpp`.
Это распознавание шести индексов как прямоугольника и быстрые quad/row пути:
однотонная заливка, копирование непрозрачных строк, текстуры и alpha blending.
Функция действительно вызывается CPU-рендерером и проверяется pixel-тестами.

При переносе добавлены проверки переполнения индексов, конечности координат,
правильной диагонали пары треугольников и UV. Градиенты/неподходящие пары идут
в существующий проверяемый triangle fallback; clipping и полупрозрачность сохраняются.
Пороговая бинаризация шрифта из MiniOS не переносилась: Alpha8 остаётся полутоновым.

Старый фиксированный font texture ID и obsolete atlas API заменены сохранённым
в Quanta адаптером динамического протокола ImGui 1.92: create/update/destroy,
копии текстур, размеры/pitch, `VtxOffset`. Контекстом ImGui владеет frontend,
а не второй параллельный init внутри MiniOS-backend.

## Граница этой интеграции

Desktop `FRAMEBUFFER` означает CPU-рисование в RGB32 RAM-буфер и показ в X11/Win32.
Это **не драйвер `/dev/fb0`, DRM/KMS, GOP/UEFI и не готовый noOS-порт**.
Из `gui_framebuffer.c` учтено разделение memory surface / presentation, но его
MiniOS-вызовы получения экрана и 16/24-bit packing не подключены к desktop-сборке.
Другие панели, workspace, disassembler и console из ZIP не нужны Quanta и не включены.
Слой presentation можно заменить отдельно, не меняя metric engine.

## Происхождение и лицензия

В предоставленном ZIP не указана лицензия. Перенос не присваивает этому коду MIT:
в `portable_quad.cpp` прямо отмечено происхождение. Перед публикацией отдельно
уточните право распространения этой части у автора исходного `portable_gui`.
Root MIT и лицензия ImGui не заменяют отсутствующее разрешение автора этой части.

```text
fdf46d613b339bfb82d6a043262f3e6d427e1cd68a0a3a819e02777b281954cf  gui_framebuffer.c
a62c7b12b93329b0183ea0eb744219b8450b9f0c4dcf8a0c47f7c71d9525a301  imgui_backend_framebuffer.cpp
```
