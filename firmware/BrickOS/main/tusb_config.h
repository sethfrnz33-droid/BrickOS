#pragma once

#include "sdkconfig.h"
#include "tusb_option.h"

#define CFG_TUSB_MCU                    OPT_MCU_ESP32S3
#define CFG_TUSB_OS                     OPT_OS_FREERTOS

#define CFG_TUH_ENABLED                 1
#define CFG_TUD_ENABLED                 0

#define CFG_TUH_RHPORT                  0
#define CFG_TUH_MAX_SPEED               OPT_MODE_FULL_SPEED

#define CFG_TUH_HUB                     1
#define CFG_TUH_CDC                     1

#define CFG_TUH_CDC_LINE_CODING         1
#define CFG_TUH_CDC_RX_BUFSIZE          256
#define CFG_TUH_CDC_TX_BUFSIZE          256

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN              __attribute__((aligned(4)))

#define CFG_TUSB_RHPORT0_MODE           (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)

#define CFG_TUH_ENUMERATION_BUFSIZE     256