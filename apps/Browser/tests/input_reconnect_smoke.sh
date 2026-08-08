#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-input-reconnect.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../core" -I"$SCRIPT_DIR/../platform/input" \
    "$SCRIPT_DIR/input_reconnect_smoke.c" \
    "$SCRIPT_DIR/../platform/input/input_keyboard.c" \
    "$SCRIPT_DIR/../core/browser_log.c" -o "$WORK_DIR/input-reconnect-smoke"
"$WORK_DIR/input-reconnect-smoke" "$WORK_DIR/reconnect-input"
