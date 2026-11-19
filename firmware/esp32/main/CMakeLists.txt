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

# Keep embedded web assets synchronized even when developers run idf.py build
# directly without invoking the standalone generator first.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/../web/index.html"
  "${CMAKE_CURRENT_LIST_DIR}/../web/app.css"
  "${CMAKE_CURRENT_LIST_DIR}/../web/app.js"
  "${CMAKE_CURRENT_LIST_DIR}/../web/core.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guided.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guided-ui.mjs")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/generate_web_assets.py"
  RESULT_VARIABLE guide_asset_result
)
if(NOT guide_asset_result EQUAL 0)
  message(FATAL_ERROR "BrickOS embedded browser asset generation failed")
endif()
