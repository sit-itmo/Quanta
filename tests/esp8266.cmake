if(NOT WIN32)
    find_package(Threads REQUIRED)
    set(QUANTA_ROOT ${PROJECT_SOURCE_DIR})
    include(${QUANTA_ROOT}/cmake/esp8266_sources.cmake)
    foreach(mode enabled disabled)
        set(target quanta_esp8266_${mode}_tests)
        add_executable(${target} test_esp8266.c ${QUANTA_ESP_SOURCES}
            ${QUANTA_ROOT}/firmware/esp8266/main/serial_format.c
            ${QUANTA_ROOT}/firmware/esp8266/main/app_main.c)
        quanta_target(${target})
        target_include_directories(${target} BEFORE PRIVATE ${QUANTA_ESP_INCLUDES}
            ${CMAKE_CURRENT_SOURCE_DIR}/esp8266_stubs
            ${QUANTA_ROOT}/firmware/esp8266/main)
        target_compile_definitions(${target} PRIVATE ${QUANTA_ESP_DEFINITIONS})
        if(mode STREQUAL "disabled")
            target_compile_definitions(${target} PRIVATE CONFIG_QUANTA_MEASURE_VDD33=0)
        endif()
        target_link_libraries(${target} PRIVATE Threads::Threads m)
        add_test(NAME ${target} COMMAND ${target})
        set_property(GLOBAL APPEND PROPERTY QUANTA_TEST_TARGETS ${target})
    endforeach()
endif()
