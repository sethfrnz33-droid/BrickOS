# Compile the SAME bounded pure-C education agent used by host tests.
# Generate its original lesson text at configure time; never hand-maintain C copies.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(SETH_PROTOTYPE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../prototype")
set(SETH_AGENT_ROOT "${SETH_PROTOTYPE_ROOT}/seth-agent")
set(SETH_CATALOG_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/seth_catalog.c")
execute_process(
 COMMAND "${Python3_EXECUTABLE}" "${SETH_AGENT_ROOT}/generate_catalog.py" --output "${SETH_CATALOG_SOURCE}"
 RESULT_VARIABLE seth_catalog_result)
if(NOT seth_catalog_result EQUAL 0)
 message(FATAL_ERROR "Seth lesson catalog generation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
 "${CMAKE_CURRENT_LIST_DIR}/../../../design/education/lesson-catalog.v1.json"
 "${SETH_AGENT_ROOT}/generate_catalog.py")
idf_component_register(
 SRCS "main.c" "protocol.c" "telemetry.c" "storage.c" "ini_cache.c" "web_assets.c" "seth_http.c" "vcp_tcp_gateway.cpp" "tsdiag_parser.cpp" "usb_sd_ini.c"
  "${SETH_AGENT_ROOT}/seth_agent.c"
  "${SETH_AGENT_ROOT}/seth_packet_json.c"
  "${SETH_AGENT_ROOT}/seth_coordinator.c"
  "${SETH_AGENT_ROOT}/seth_actions.c"
  "${SETH_AGENT_ROOT}/seth_checkpoint.c"
  "${SETH_AGENT_ROOT}/seth_learning_path.c"
  "${SETH_AGENT_ROOT}/seth_evidence.c"
  "${SETH_PROTOTYPE_ROOT}/esp_education_core.c"
  "${SETH_CATALOG_SOURCE}"
 INCLUDE_DIRS "." "${SETH_AGENT_ROOT}" "${SETH_PROTOTYPE_ROOT}"
 REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_http_client esp-tls mbedtls driver esp_driver_gpio esp_driver_sdspi esp_driver_spi esp_timer lwip json spiffs fatfs sdmmc esp_partition espressif__usb_host_msc
)

# Same TinyUSB host include exposure as the user's known-good working3wayhub project.
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
if(tusb_lib)
    target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
endif()

# Keep embedded web assets synchronized even when developers run idf.py build
# directly without invoking the standalone generator first.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/../web/index.html"
  "${CMAKE_CURRENT_LIST_DIR}/../web/app.css"
  "${CMAKE_CURRENT_LIST_DIR}/../web/app.js"
  "${CMAKE_CURRENT_LIST_DIR}/../web/core.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/ini-fast-cache.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guided.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guided-ui.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-story.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-setting-help.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/ini-compat.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-reasoning.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-story-ui.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-voice.mjs"
  "${CMAKE_CURRENT_LIST_DIR}/../web/seth.html"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-catalog.json"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-assets/seth-head.png"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-assets/seth-jaw.png"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-assets/seth-mouth.png"
  "${CMAKE_CURRENT_LIST_DIR}/../web/guide-assets/voice.mp3")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/generate_web_assets.py"
  RESULT_VARIABLE guide_asset_result
)
if(NOT guide_asset_result EQUAL 0)
  message(FATAL_ERROR "BrickOS embedded browser asset generation failed")
endif()
