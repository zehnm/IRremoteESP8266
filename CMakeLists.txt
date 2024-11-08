idf_component_register(
    SRC_DIRS
    "src"
    "src/hal"
    INCLUDE_DIRS
    "src"
    "src/locale"
    PRIV_INCLUDE_DIRS
    "src/hal"
    REQUIRES esp_driver_gpio esp_timer hal driver
)

target_compile_definitions(${COMPONENT_TARGET} PUBLIC "-D ESP32")

project(IRemoteESP8266)
