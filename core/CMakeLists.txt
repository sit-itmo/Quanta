# Reusable C core: no collectors, HAL, UI, or native headers in the public API.
add_library(quanta_core STATIC
    src/status.c src/text.c src/value.c src/registry.c src/provider.c
    src/engine.c src/discovery.c src/scheduler.c src/history.c src/live.c
    ${PROJECT_SOURCE_DIR}/query/src/query.c
    ${PROJECT_SOURCE_DIR}/osal/common.c
    ${PROJECT_SOURCE_DIR}/osal/${QUANTA_PLATFORM}/${QUANTA_PLATFORM}.c
    ${PROJECT_SOURCE_DIR}/osal/${QUANTA_PLATFORM}/thread.c
    ${PROJECT_SOURCE_DIR}/osal/${QUANTA_PLATFORM}/affinity.c)
quanta_target(quanta_core)
target_include_directories(quanta_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/osal/include)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(quanta_core PUBLIC Threads::Threads m)
endif()
