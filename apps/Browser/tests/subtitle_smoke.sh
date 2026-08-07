#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-subtitle-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

printf '%s\n' \
    '1' \
    '00:00:01,000 --> 00:00:02,500' \
    'Hello' \
    'world' \
    '' \
    '2' \
    '00:00:03.000 --> 00:00:04.000' \
    'Second cue' > "$WORK_DIR/sample.srt"

"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../media/video" \
    "$SCRIPT_DIR/subtitle_smoke.c" \
    "$SCRIPT_DIR/../media/video/subtitle.c" \
    -o "$WORK_DIR/subtitle-smoke"
"$WORK_DIR/subtitle-smoke" "$WORK_DIR/sample.mp4" \
    "$WORK_DIR/missing.mp4"
