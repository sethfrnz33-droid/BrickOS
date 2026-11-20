#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
INC='-Itests/host_stubs -Imain -include tests/host_stubs/main_extra.h'
cc -std=gnu17 -fsyntax-only -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare \
  -DCONFIG_BRICK_ALLOW_CAL_WRITES=0 \
  $INC main/main.c
cc -std=gnu17 -fsyntax-only -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare \
  -DCONFIG_BRICK_SD_SPI=0 $INC main/storage.c
printf 'PASS: main.c build syntax with legacy CAL_WRITES=0 and storage.c host C syntax checks\n'
# v2.7: optional SD MSC module and signature-checked SPIFFS importer.
cc -std=gnu17 -fsyntax-only -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare \
  $INC main/usb_sd_ini.c
cc -std=gnu17 -fsyntax-only -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare \
  -DCONFIG_BRICK_SD_SPI=0 $INC main/ini_cache.c
printf 'PASS: SD MSC reader and signature-checked INI importer C syntax\n'

# v3.7: fast read-only telemetry USB sampler.
cc -std=gnu17 -fsyntax-only -Wall -Wextra -Werror \
  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -Wno-sign-compare \
  $INC main/telemetry.c
printf 'PASS: fast read-only telemetry source C syntax\n'
