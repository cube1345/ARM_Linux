#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-watchdog.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../core" "$SCRIPT_DIR/watchdog_smoke.c" \
    "$SCRIPT_DIR/../core/watchdog.c" -o "$WORK_DIR/watchdog-smoke"
"$WORK_DIR/watchdog-smoke"
