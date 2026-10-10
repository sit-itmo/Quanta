# One explicit source/ABI manifest for the SDK component AND its host contract tests.
# Caller sets QUANTA_ROOT. No filesystem search, downloading, or desktop build recursion.
set(QUANTA_ESP_SOURCES
    core/src/status.c core/src/text.c core/src/value.c core/src/registry.c core/src/metric.c
    core/src/engine.c core/src/discovery.c core/src/scheduler.c core/src/history.c core/src/live.c
    query/src/query.c osal/common.c osal/freertos/freertos.c osal/posix/thread.c
    hal/common/hal.c hal/esp8266/hal_esp8266.c
    sensors/registry.c sensors/metric_docs.c sensors/os/metrics.c sensors/os/esp8266.c
    sensors/mcu/metrics.c sensors/mcu/esp8266.c)
set(QUANTA_ESP_INCLUDES
    firmware/esp8266/components/quanta/config
    core/include osal/include hal/include sensors/include
    core/private osal/private hal/private sensors/private metrics)
list(TRANSFORM QUANTA_ESP_SOURCES PREPEND "${QUANTA_ROOT}/")
list(TRANSFORM QUANTA_ESP_INCLUDES PREPEND "${QUANTA_ROOT}/")
set(QUANTA_ESP_DEFINITIONS
    QUANTA_PLATFORM_FREERTOS=1 QUANTA_MAX_SELECT=4 QUANTA_LABEL_CAP=1
    QUANTA_TEXT_CAP=128 QUANTA_ID_CAP=128 QUANTA_PATH_CAP=128
    QUANTA_MAX_CHANNELS=32 QUANTA_MAX_SERIES=16 QUANTA_MAX_ACQUISITIONS=4
    QUANTA_QUERY_CAP=2048 QUANTA_INITIAL_CAPACITY=4 QUANTA_LIVE_QUEUE_CAP=8)
