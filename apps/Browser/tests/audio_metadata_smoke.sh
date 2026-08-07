#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-metadata-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../media/audio" "$SCRIPT_DIR/audio_metadata_smoke.c" \
    "$SCRIPT_DIR/../media/audio/audio_metadata.c" \
    -o "$WORK_DIR/audio-metadata-smoke"
"$WORK_DIR/audio-metadata-smoke" "$WORK_DIR/tagged.mp3"
