#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pedantic -I../../prototype \
  ../../prototype/esp_education_core.c tests/test_education_core_host.c \
  -o "$tmp/brick_edu_host_test"
"$tmp/brick_edu_host_test"
