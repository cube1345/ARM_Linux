#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-config-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../core" "$SCRIPT_DIR/config_smoke.c" \
    "$SCRIPT_DIR/../core/browser_config.c" -o "$WORK_DIR/config-smoke"
"$WORK_DIR/config-smoke" "$WORK_DIR/media-browser.conf"
