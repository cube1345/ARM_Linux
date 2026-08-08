#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-deploy.XXXXXX")
FAKE_BROWSER="$WORK_DIR/fake-browser"
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

sh -n "$SCRIPT_DIR/../deploy/media-browser-launcher"
sh -n "$SCRIPT_DIR/../deploy/S95media-browser"
printf '#!/bin/sh\nprintf "%%s\\n" "$@"\n' > "$FAKE_BROWSER"
chmod 0755 "$FAKE_BROWSER"

OUTPUT=$(MEDIA_BROWSER_ENV=- MEDIA_BROWSER_BIN="$FAKE_BROWSER" \
    MEDIA_BROWSER_FB=/dev/fb-test MEDIA_BROWSER_KEYBOARD=stdin \
    MEDIA_BROWSER_ROOT=/media/test MEDIA_BROWSER_FONT=/font/test.ttf \
    MEDIA_BROWSER_ALSA=plughw:1 MEDIA_BROWSER_POINTER=/dev/input/test \
    "$SCRIPT_DIR/../deploy/media-browser-launcher")
EXPECTED=$(printf '%s\n' /dev/fb-test stdin /media/test /font/test.ttf \
    plughw:1 /dev/input/test)
if [ "$OUTPUT" != "$EXPECTED" ]; then
    echo "FAIL launcher arguments" >&2
    exit 1
fi
if ! grep -q '^ExecStart=/usr/bin/media-browser-launcher$' \
    "$SCRIPT_DIR/../deploy/media-browser.service"; then
    echo "FAIL systemd service" >&2
    exit 1
fi
echo "PASS deployment launchers"
