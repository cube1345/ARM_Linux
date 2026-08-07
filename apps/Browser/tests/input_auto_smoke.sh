#!/bin/sh
set -eu

BROWSER_BIN=${BROWSER_BIN:-/usr/bin/media-browser}
FRAMEBUFFER=${FRAMEBUFFER:-/dev/fb0}
MEDIA_ROOT=${MEDIA_ROOT:-/root/media}
FONT_PATH=${FONT_PATH:-/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc}
ALSA_DEVICE=${ALSA_DEVICE:-default}
LOG_FILE=$(mktemp "${TMPDIR:-/tmp}/browser-input-smoke.XXXXXX")
trap 'rm -f "$LOG_FILE"' EXIT HUP INT TERM

if [ ! -x "$BROWSER_BIN" ] || [ ! -e "$FRAMEBUFFER" ] ||
   [ ! -e "$FONT_PATH" ]; then
    echo "SKIP input auto smoke: target framebuffer/browser/font unavailable"
    exit 0
fi
if ! find /dev/input -maxdepth 1 -name 'event*' -print -quit | grep -q .; then
    echo "FAIL input auto smoke: no evdev event node" >&2
    exit 1
fi

"$BROWSER_BIN" "$FRAMEBUFFER" auto "$MEDIA_ROOT" "$FONT_PATH" \
    "$ALSA_DEVICE" auto >"$LOG_FILE" 2>&1 &
browser_pid=$!
sleep 2
kill -TERM "$browser_pid" 2>/dev/null || true
wait "$browser_pid" || status=$?
status=${status:-0}
if [ "$status" -ne 0 ] ||
   ! grep -q 'auto keyboard input:' "$LOG_FILE" ||
   ! grep -q 'auto pointer input:' "$LOG_FILE"; then
    cat "$LOG_FILE" >&2
    echo "FAIL input auto smoke" >&2
    exit 1
fi
echo "PASS input auto smoke"
