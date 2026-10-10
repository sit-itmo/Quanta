if(WIN32)
    set(QUANTA_THREAD_SOURCE ${PROJECT_SOURCE_DIR}/osal/windows/thread.c)
else()
    set(QUANTA_THREAD_SOURCE ${PROJECT_SOURCE_DIR}/osal/posix/thread.c)
endif()
# Reusable C core: depends on low-level HAL/OSAL, never on built-in metrics or UI.
add_library(quanta_core STATIC
    src/status.c src/text.c src/value.c src/registry.c src/metric.c
    src/engine.c src/discovery.c src/scheduler.c src/history.c src/live.c
    ${PROJECT_SOURCE_DIR}/query/src/query.c
    ${PROJECT_SOURCE_DIR}/osal/common.c
    ${PROJECT_SOURCE_DIR}/osal/${QUANTA_PLATFORM}/${QUANTA_PLATFORM}.c
    ${QUANTA_THREAD_SOURCE})
quanta_target(quanta_core)
target_include_directories(quanta_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/osal/include)
target_link_libraries(quanta_core PUBLIC quanta_hal)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(quanta_core PUBLIC Threads::Threads m)
endif()
