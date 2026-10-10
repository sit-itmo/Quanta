# C tests are mandatory when BUILD_TESTING=ON. Python adds integration/doc checks.
function(quanta_test target)
    add_executable(${target} ${ARGN})
    quanta_target(${target})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/frontends/private ${PROJECT_SOURCE_DIR}/frontends/include
        ${PROJECT_SOURCE_DIR}/frontends/gui ${PROJECT_SOURCE_DIR}/sensors/private
        ${PROJECT_SOURCE_DIR}/sensors/include ${PROJECT_SOURCE_DIR}/hal/include
        ${PROJECT_SOURCE_DIR}/hal/common ${PROJECT_SOURCE_DIR}/metrics)
    target_link_libraries(${target} PRIVATE quanta_output quanta_sensors)
    add_test(NAME ${target} COMMAND ${target})
    set_property(GLOBAL APPEND PROPERTY QUANTA_TEST_TARGETS ${target})
endfunction()
quanta_test(quanta_unit_tests test_core.c ${PROJECT_SOURCE_DIR}/frontends/gui/raster.c)
quanta_test(quanta_windows_data_tests test_windows_data.c
    ${PROJECT_SOURCE_DIR}/hal/common/windows_math.c ${PROJECT_SOURCE_DIR}/hal/common/smbios.c)
if(WIN32)
    quanta_test(quanta_windows_tests test_windows.c)
else()
    quanta_test(quanta_device_tests test_devices.c)
    quanta_test(quanta_linux_hal_tests test_linux_hal.c)
    # A separate Windows registry compiled on Linux. This is NOT a WinAPI emulator.
    file(READ ${PROJECT_BINARY_DIR}/generated/quanta_config.h schema_config)
    string(REPLACE "#define QUANTA_OS_LINUX 1" "#define QUANTA_OS_LINUX 0" schema_config "${schema_config}")
    string(REPLACE "#define QUANTA_OS_WINDOWS 0" "#define QUANTA_OS_WINDOWS 1" schema_config "${schema_config}")
    file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema/quanta_config.h CONTENT "${schema_config}" @ONLY)
    add_executable(quanta_windows_schema_tests test_windows_schema.c windows_schema_stubs.c
        ${PROJECT_SOURCE_DIR}/sensors/registry.c ${PROJECT_SOURCE_DIR}/sensors/metric_docs.c)
    quanta_target(quanta_windows_schema_tests)
    target_include_directories(quanta_windows_schema_tests BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/schema)
    target_include_directories(quanta_windows_schema_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/frontends/private ${PROJECT_SOURCE_DIR}/frontends/include
        ${PROJECT_SOURCE_DIR}/sensors/private ${PROJECT_SOURCE_DIR}/sensors/include
        ${PROJECT_SOURCE_DIR}/hal/include ${PROJECT_SOURCE_DIR}/metrics)
    target_link_libraries(quanta_windows_schema_tests PRIVATE quanta_core)
    if(QUANTA_WITH_MEMORY AND QUANTA_WITH_CPU AND QUANTA_WITH_DMI AND QUANTA_WITH_NETWORK
       AND QUANTA_WITH_PROCESS AND QUANTA_WITH_FILESYSTEM)
        file(GLOB windows_queries CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/queries/windows/*.q)
    endif()
    add_test(NAME windows_schema COMMAND quanta_windows_schema_tests ${windows_queries})
    set_property(GLOBAL APPEND PROPERTY QUANTA_TEST_TARGETS quanta_windows_schema_tests)
endif()
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME frontend_modes COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/frontend_modes.py
        $<TARGET_FILE:quanta> $<TARGET_FILE:quanta_cli> $<TARGET_FILE:quanta_csv> $<TARGET_FILE:quanta_txt>)
    add_test(NAME documentation COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/embed_docs.py --check)
    if(WIN32)
        add_test(NAME integration COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/windows_integration.py $<TARGET_FILE:quanta>)
        if(QUANTA_WITH_GUI)
            add_test(NAME windows_gui COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/windows_gui.py $<TARGET_FILE:quanta>)
        endif()
    else()
        add_test(NAME integration COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/integration.py $<TARGET_FILE:quanta>)
        if(QUANTA_WITH_TUI)
            add_test(NAME tui_pty COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tui_pty.py $<TARGET_FILE:quanta>)
        endif()
        if(QUANTA_WITH_GUI)
            find_program(QUANTA_XVFB Xvfb)
            if(QUANTA_XVFB)
                add_test(NAME gui_x11 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gui_smoke.py $<TARGET_FILE:quanta>)
            endif()
        endif()
    endif()
else()
    message(STATUS "Python 3 not found: C tests enabled, Python integration/doc tests skipped")
endif()
# Public API tests must link to core alone: a hidden sensor dependency fails at link time.
foreach(suite catalog history osal)
    add_executable(quanta_${suite}_tests test_${suite}.c)
    quanta_target(quanta_${suite}_tests)
    target_link_libraries(quanta_${suite}_tests PRIVATE quanta_core)
    add_test(NAME quanta_${suite}_tests COMMAND quanta_${suite}_tests)
    set_property(GLOBAL APPEND PROPERTY QUANTA_TEST_TARGETS quanta_${suite}_tests)
endforeach()
get_property(all_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${all_tests} PROPERTIES TIMEOUT 120)
get_property(test_targets GLOBAL PROPERTY QUANTA_TEST_TARGETS)
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${PROJECT_BINARY_DIR} -C $<CONFIG> --output-on-failure --no-tests=error
    DEPENDS ${test_targets} quanta quanta_cli quanta_csv quanta_txt USES_TERMINAL)
if(QUANTA_TESTS_ON_BUILD AND NOT CMAKE_CROSSCOMPILING)
    add_custom_target(quanta_verify ALL DEPENDS check)
endif()
