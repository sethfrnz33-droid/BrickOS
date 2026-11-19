#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c17 -Wall -Wextra -Werror -Imain main/web_assets.c tests/test_web_assets_host.c -o /tmp/brickos_assets_host
/tmp/brickos_assets_host
