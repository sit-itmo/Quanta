get_filename_component(QUANTA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
include("${QUANTA_ROOT}/cmake/esp8266_sources.cmake")
idf_component_register(SRCS ${QUANTA_ESP_SOURCES}
    INCLUDE_DIRS "${QUANTA_ROOT}/core/include" "${QUANTA_ROOT}/osal/include"
                 "${QUANTA_ROOT}/hal/include" "${QUANTA_ROOT}/sensors/include" config
    PRIV_INCLUDE_DIRS ${QUANTA_ESP_INCLUDES}
    REQUIRES freertos esp8266 pthread)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ${QUANTA_ESP_DEFINITIONS})
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu11 -Wall -Wextra)
