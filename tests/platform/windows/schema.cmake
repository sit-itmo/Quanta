# Compile only the Windows catalog on another OS; this is NOT a WinAPI emulator.
file(READ ${PROJECT_BINARY_DIR}/generated/quanta_config.h schema_config)
string(REPLACE "#define QUANTA_OS_LINUX 1" "#define QUANTA_OS_LINUX 0" schema_config "${schema_config}")
string(REPLACE "#define QUANTA_OS_WINDOWS 0" "#define QUANTA_OS_WINDOWS 1" schema_config "${schema_config}")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schema/quanta_config.h CONTENT "${schema_config}" @ONLY)
add_executable(quanta_test_windows_schema platform/windows/test_windows_schema.c platform/windows/schema_stubs.c
    ${PROJECT_SOURCE_DIR}/sensors/registry.c ${PROJECT_SOURCE_DIR}/sensors/metric_docs.c
    ${PROJECT_SOURCE_DIR}/sensors/common/runtime.c)
quanta_target(quanta_test_windows_schema)
target_include_directories(quanta_test_windows_schema BEFORE PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/schema)
target_include_directories(quanta_test_windows_schema PRIVATE
    ${PROJECT_SOURCE_DIR}/sensors/private ${PROJECT_SOURCE_DIR}/metrics)
target_link_libraries(quanta_test_windows_schema PRIVATE quanta_core)
if(QUANTA_WITH_MEMORY AND QUANTA_WITH_CPU AND QUANTA_WITH_DMI AND QUANTA_WITH_NETWORK
   AND QUANTA_WITH_PROCESS AND QUANTA_WITH_FILESYSTEM)
    file(GLOB windows_queries CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/examples/queries/windows/*.q)
endif()
add_test(NAME windows_schema COMMAND quanta_test_windows_schema ${windows_queries})
set_tests_properties(windows_schema PROPERTIES LABELS schema)
set_property(GLOBAL APPEND PROPERTY QUANTA_TEST_TARGETS quanta_test_windows_schema)
