idf_component_register(
 SRCS "main.c" "protocol.c" "telemetry.c" "storage.c" "ini_cache.c" "web_assets.c" "vcp_tcp_gateway.cpp" "tsdiag_parser.cpp" "usb_sd_ini.c"
 INCLUDE_DIRS "."
 REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_http_client esp-tls mbedtls driver esp_driver_gpio esp_driver_sdspi esp_driver_spi esp_timer lwip json spiffs fatfs sdmmc esp_partition espressif__usb_host_msc
)

# Same TinyUSB host include exposure as the user's known-good working3wayhub project.
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
if(tusb_lib)
    target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
endif()
