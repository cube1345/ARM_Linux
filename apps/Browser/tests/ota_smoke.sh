#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-ota.XXXXXX")
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM

sh -n "$SCRIPT_DIR/../deploy/media-browser-ota"
printf 'fixture\n' > "$WORK_DIR/image.bin"
printf 'a\n' > "$WORK_DIR/active"

OUTPUT=$(OTA_SLOT_A="$WORK_DIR/a" OTA_SLOT_B="$WORK_DIR/b" \
    OTA_ACTIVE_FILE="$WORK_DIR/active" OTA_DRY_RUN=1 \
    "$SCRIPT_DIR/../deploy/media-browser-ota" "$WORK_DIR/image.bin")
case "$OUTPUT" in
    *"target=$WORK_DIR/b"*"next=b"*) ;;
    *) echo "FAIL OTA dry-run" >&2; exit 1 ;;
esac
echo "PASS OTA updater"
