#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-crash-log.XXXXXX")
LOG_PATH="$WORK_DIR/crash.log"
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM
ulimit -c 0

"$CC" -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
    -I"$SCRIPT_DIR/../core" "$SCRIPT_DIR/crash_log_smoke.c" \
    "$SCRIPT_DIR/../core/browser_log.c" -o "$WORK_DIR/crash-log-smoke"
set +e
"$WORK_DIR/crash-log-smoke" "$LOG_PATH" 2>/dev/null
STATUS=$?
set -e
if [ "$STATUS" -ne 134 ]; then
    echo "FAIL crash exit status: $STATUS" >&2
    exit 1
fi
if ! grep -q '^media-browser fatal signal=6$' "$LOG_PATH"; then
    echo "FAIL crash log content" >&2
    exit 1
fi
echo "PASS crash log"
