# Main application component

file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_http_client
        esp_https_ota
        esp_wifi
        mqtt
)

