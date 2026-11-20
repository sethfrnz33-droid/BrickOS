# BrickOS v1.8 — USB VCP exception compile guard fix

The v1.7 ESP-IDF build log confirmed `-fexceptions` was appended after
`-fno-exceptions`, but three Espressif USB VCP headers still issued:
`#error This component requires C++ exceptions`.

**Root cause:** those headers test `#ifndef CONFIG_COMPILER_CXX_EXCEPTIONS`,
which is generated from `sdkconfig`. A CMake compile flag `-fexceptions`
does **not** automatically define that Kconfig macro.

**Correction:** root `CMakeLists.txt` now applies *both* `-fexceptions` and
`-DCONFIG_COMPILER_CXX_EXCEPTIONS=1` to C++ compilation of `main` and the
four Espressif USB VCP components, without editing third-party sources.
`sdkconfig.defaults` still enables the Kconfig option for fresh projects.

**Validation:** project CMake check, browser tests, host syntax tests and ZIP
CRC inspection. Full ESP-IDF 5.3.1 cross compilation unavailable here.

Rebuild using `idf.py reconfigure build`. With this archive applied, the
`vcp_tcp_gateway.cpp.obj` command must include both `-fexceptions` and
`-DCONFIG_COMPILER_CXX_EXCEPTIONS=1`. If your build cache retains the old
command, run `idf.py fullclean build`.
