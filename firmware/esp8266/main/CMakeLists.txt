# firmware/esp8266/main/CMakeLists.txt
# Source list and settings for this module/platform; no runtime logic.
idf_component_register(SRCS app_main.c serial_format.c INCLUDE_DIRS . REQUIRES quanta)
