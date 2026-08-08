#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-screen-power.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../core" "$SCRIPT_DIR/screen_power_smoke.c" \
    "$SCRIPT_DIR/../core/screen_power.c" -o "$WORK_DIR/screen-power-smoke"
"$WORK_DIR/screen-power-smoke"
