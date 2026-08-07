#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-render-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../platform/display" \
    -I"$SCRIPT_DIR/../media/image" \
    "$SCRIPT_DIR/image_render_smoke.c" \
    "$SCRIPT_DIR/../media/image/image_render.c" \
    "$SCRIPT_DIR/../media/image/image_data.c" \
    -o "$WORK_DIR/image-render-smoke"
"$WORK_DIR/image-render-smoke"
