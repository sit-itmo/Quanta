# Hardware access is intentionally independent of the metric/core implementation.
add_library(quanta_hal STATIC common/hal.c ${QUANTA_PLATFORM}/hal_${QUANTA_PLATFORM}.c)
quanta_target(quanta_hal)
target_include_directories(quanta_hal PUBLIC include ${PROJECT_SOURCE_DIR}/core/include)
target_include_directories(quanta_hal PRIVATE private)
if(NOT WIN32)
    target_compile_definitions(quanta_hal PRIVATE _FILE_OFFSET_BITS=64)
endif()
