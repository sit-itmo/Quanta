# core/CMakeLists.txt
# Source list and settings for this module/platform; no runtime logic.
# One portable C library: query, scheduler, HAL and the small OSAL contract.
# No built-in metrics, frontend implementation, C++ or GUI library is linked here.
if(WIN32)
    set(thread ../osal/windows/thread.c)
else()
    set(thread ../osal/posix/thread.c)
endif()
add_library(quanta_core STATIC
    src/status.c src/text.c src/value.c src/registry.c src/metric.c src/query.c
    src/engine.c src/discovery.c src/scheduler.c src/history.c src/live.c
    ../osal/common.c ../osal/${QUANTA_PLATFORM}/${QUANTA_PLATFORM}.c ${thread}
    ../hal/common/hal.c ../hal/${QUANTA_PLATFORM}/hal_${QUANTA_PLATFORM}.c)
quanta_target(quanta_core)
target_include_directories(quanta_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_include_directories(quanta_core PRIVATE private ../osal/private ../hal/private)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(quanta_core PUBLIC Threads::Threads m)
endif()
