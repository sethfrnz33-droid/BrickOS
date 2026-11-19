cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(brickos_s3_full)

# The upstream espressif USB VCP C++ components use throw/catch. Existing
# sdkconfig files may retain CONFIG_COMPILER_CXX_EXCEPTIONS=n, overriding
# sdkconfig.defaults and generating -fno-exceptions. Apply -fexceptions to
# the actual affected targets in addition to the sdkconfig default.
# This preserves the user's other existing sdkconfig and pin/flash settings.
# The USB VCP headers also #error unless CONFIG_COMPILER_CXX_EXCEPTIONS
# is defined, even if the C++ compiler is already given -fexceptions.
foreach(usb_cpp_component IN ITEMS
        main
        espressif__usb_host_vcp
        espressif__usb_host_ch34x_vcp
        espressif__usb_host_cp210x_vcp
        espressif__usb_host_ftdi_vcp)
    idf_component_get_property(usb_cpp_lib ${usb_cpp_component} COMPONENT_LIB)
    if(usb_cpp_lib)
        # Espressif VCP headers require CONFIG_COMPILER_CXX_EXCEPTIONS
        # to be defined; -fexceptions alone does not satisfy their guard.
        # A pre-existing sdkconfig may disable the Kconfig option even when
        # sdkconfig.defaults enables it, so set BOTH for these C++ targets.
        target_compile_options(${usb_cpp_lib} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>")
        target_compile_definitions(${usb_cpp_lib} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:CONFIG_COMPILER_CXX_EXCEPTIONS=1>")
    endif()
endforeach()

# rusEFI USB MSD exposes the SD card on LUN1, while LUN0 is the
# firmware's embedded INI drive. Espressif MSC v1.1.4 hardcodes LUN0.
# This isolated copy of the project overrides the BOT CBW LUN only;
# all USB CDC endpoints remain untouched.
set(BRICK_MSC_BOT_SOURCE "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__usb_host_msc/src/msc_scsi_bot.c")
if(NOT EXISTS "${BRICK_MSC_BOT_SOURCE}")
    message(FATAL_ERROR "USB MSC driver source missing; check dependency download: ${BRICK_MSC_BOT_SOURCE}")
endif()
file(READ "${BRICK_MSC_BOT_SOURCE}" BRICK_MSC_BOT_TEXT)
if(BRICK_MSC_BOT_TEXT MATCHES "\\.lun[ ]*=[ ]*0,")
    string(REGEX REPLACE "\\.lun[ ]*=[ ]*0," ".lun = 1," BRICK_MSC_BOT_TEXT "${BRICK_MSC_BOT_TEXT}")
    file(WRITE "${BRICK_MSC_BOT_SOURCE}" "${BRICK_MSC_BOT_TEXT}")
    message(STATUS "BrickOS: rusEFI SD card LUN1 selected in USB MSC BOT driver")
elseif(BRICK_MSC_BOT_TEXT MATCHES "\\.lun[ ]*=[ ]*1,")
    message(STATUS "BrickOS: MSC driver already configured for SD card LUN1")
else()
    message(FATAL_ERROR "USB MSC driver changed; refusing to use implicit LUN0 instead of ECU SD LUN1")
endif()
