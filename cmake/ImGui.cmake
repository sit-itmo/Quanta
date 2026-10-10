# cmake/ImGui.cmake
# Source list and settings for this module/platform; no runtime logic.
# Sources are detected before targets are created. CMake never downloads code.
add_library(imgui_vendor STATIC
  ${QUANTA_IMGUI_DIR}/imgui.cpp ${QUANTA_IMGUI_DIR}/imgui_draw.cpp
  ${QUANTA_IMGUI_DIR}/imgui_tables.cpp ${QUANTA_IMGUI_DIR}/imgui_widgets.cpp)
target_include_directories(imgui_vendor PUBLIC ${QUANTA_IMGUI_DIR})
target_compile_definitions(imgui_vendor PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_compile_features(imgui_vendor PUBLIC cxx_std_11)
if(QUANTA_SANITIZE)
  target_compile_options(imgui_vendor PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(WIN32)
    # Keep native ImGui helpers' dependencies on their own target, including
    # non-MSVC linkers which do not process #pragma comment(lib, ...).
    target_link_libraries(imgui_vendor PRIVATE user32 imm32 shell32)
endif()
if(MSVC)
    target_compile_options(imgui_vendor PRIVATE /utf-8)
endif()
