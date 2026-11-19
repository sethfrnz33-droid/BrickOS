#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c17 -Wall -Wextra -Werror -DCONFIG_BRICK_CAN_TX_ID=0x100 -DCONFIG_BRICK_CAN_RX_ID=0x102 -DCONFIG_BRICK_CAN_DISCOVERY=1 -DCONFIG_BRICK_CAN_TRACE=1 -DCONFIG_BRICK_CAN_500K=1 -DBRICK_HOST_TEST=1 -Itests/host_stubs -Imain main/protocol.c tests/test_protocol_host.c -o /tmp/brickos_protocol_host
/tmp/brickos_protocol_host

cc -std=c17 -Wall -Wextra -Werror -DCONFIG_BRICK_CAN_TX_ID=0x100 -DCONFIG_BRICK_CAN_RX_ID=0x102 -DCONFIG_BRICK_CAN_DISCOVERY=1 -DCONFIG_BRICK_CAN_TRACE=1 -DCONFIG_BRICK_CAN_500K=1 -DBRICK_HOST_TEST=1 -Itests/host_stubs -Imain main/protocol.c tests/test_handshake_plain_host.c -o /tmp/brickos_handshake_plain_host
/tmp/brickos_handshake_plain_host
