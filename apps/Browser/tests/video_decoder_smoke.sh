#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
PKG_CONFIG=${PKG_CONFIG:-pkg-config}
CC_PATH=$(command -v "$CC")
TOOLCHAIN_DIR=$(dirname "$CC_PATH")
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-video-decoder.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

"$CC" -B"$TOOLCHAIN_DIR/" \
    -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    $($PKG_CONFIG --cflags libavcodec libavutil) \
    -I"$SCRIPT_DIR/../media/video" \
    "$SCRIPT_DIR/video_decoder_smoke.c" \
    "$SCRIPT_DIR/../media/video/video_decoder.c" \
    $($PKG_CONFIG --libs libavcodec libavutil) \
    -o "$WORK_DIR/video-decoder-smoke"
"$WORK_DIR/video-decoder-smoke"
