#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_DIR=${1:-/home/cube/WorkSpace/Linux/ARM_Linux/target/root/media}
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-media-smoke.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

if [ ! -d "$SOURCE_DIR" ]; then
    echo "media-smoke: source media directory not found: $SOURCE_DIR" >&2
    exit 1
fi
python3 "$SCRIPT_DIR/generate_fixtures.py" "$WORK_DIR/valid"
mv "$WORK_DIR/valid/corrupt.png" "$WORK_DIR/corrupt.png"
if command -v convert >/dev/null 2>&1; then
    convert "$WORK_DIR/valid/sample.png" "$WORK_DIR/valid/sample.jpg"
else
    echo "media-smoke: ImageMagick convert is required for JPEG fixture" >&2
    exit 1
fi

mp3=$(find "$SOURCE_DIR" -maxdepth 1 -type f -iname '*.mp3' | head -1)
mp4=$(find "$SOURCE_DIR" -maxdepth 1 -type f -iname '*.mp4' | head -1)
if [ -z "$mp3" ] || [ -z "$mp4" ]; then
    echo "media-smoke: supply MP3 and MP4 fixtures in $SOURCE_DIR" >&2
    exit 1
fi
cp "$mp3" "$WORK_DIR/valid/sample.mp3"
cp "$mp4" "$WORK_DIR/valid/sample.mp4"
mkdir "$WORK_DIR/valid/nested"
cp "$WORK_DIR/valid/sample.png" "$WORK_DIR/valid/nested/nested.png"
mkdir "$WORK_DIR/empty"

pkg_flags=$(pkg-config --cflags --libs libpng libjpeg)
"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../app" -I"$SCRIPT_DIR/../core" \
    -I"$SCRIPT_DIR/../media/image" -I"$SCRIPT_DIR/../media/animation" \
    "$SCRIPT_DIR/media_smoke.c" "$SCRIPT_DIR/../core/browser_log.c" \
    "$SCRIPT_DIR/../core/file_list.c" "$SCRIPT_DIR/../media/image/image_data.c" \
    "$SCRIPT_DIR/../media/image/bmp_decoder.c" \
    "$SCRIPT_DIR/../media/image/jpeg_decoder.c" \
    "$SCRIPT_DIR/../media/image/png_decoder.c" \
    "$SCRIPT_DIR/../media/image/image_decoder.c" \
    "$SCRIPT_DIR/../media/animation/gif_animation.c" \
    "$SCRIPT_DIR/../media/animation/animation_decoder.c" \
    -o "$WORK_DIR/media-smoke" $pkg_flags -lgif

"$WORK_DIR/media-smoke" "$WORK_DIR/valid" "$WORK_DIR/empty" \
    "$WORK_DIR/corrupt.png"
echo "PASS media decoder smoke tests"
