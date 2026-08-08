#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
PKG_CONFIG=${PKG_CONFIG:-pkg-config}
CC_PATH=$(command -v "$CC")
TOOLCHAIN_DIR=$(dirname "$CC_PATH")
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/browser-plugin.XXXXXX")
PLUGIN_DIR="$WORK_DIR/plugins"
FAILING_PLUGIN_DIR="$WORK_DIR/failing-plugins"
MARKER="$WORK_DIR/shutdown.marker"
trap 'rm -rf "$WORK_DIR"' EXIT HUP INT TERM
mkdir -p "$PLUGIN_DIR" "$FAILING_PLUGIN_DIR"

COMMON_FLAGS="-D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -O2"
PACKAGE_FLAGS=$($PKG_CONFIG --cflags freetype2)
INCLUDES="-I$SCRIPT_DIR/../app -I$SCRIPT_DIR/../core \
-I$SCRIPT_DIR/../platform/display -I$SCRIPT_DIR/../platform/input \
-I$SCRIPT_DIR/../platform/font -I$SCRIPT_DIR/../media/image \
-I$SCRIPT_DIR/../media/animation -I$SCRIPT_DIR/../media/audio \
-I$SCRIPT_DIR/../media/video -I$SCRIPT_DIR/../pages/reader"

# shellcheck disable=SC2086
"$CC" -B"$TOOLCHAIN_DIR/" $COMMON_FLAGS $PACKAGE_FLAGS \
    -fPIC -shared $INCLUDES \
    "$SCRIPT_DIR/plugin_invalid_fixture.c" \
    -o "$PLUGIN_DIR/00-invalid.so"
# shellcheck disable=SC2086
"$CC" -B"$TOOLCHAIN_DIR/" $COMMON_FLAGS $PACKAGE_FLAGS \
    -fPIC -shared $INCLUDES \
    "$SCRIPT_DIR/plugin_fixture.c" \
    -o "$PLUGIN_DIR/10-fixture.so"
# shellcheck disable=SC2086
"$CC" -B"$TOOLCHAIN_DIR/" $COMMON_FLAGS $PACKAGE_FLAGS \
    -fPIC -shared $INCLUDES \
    "$SCRIPT_DIR/plugin_failing_fixture.c" \
    -o "$FAILING_PLUGIN_DIR/10-failing.so"
# shellcheck disable=SC2086
"$CC" -B"$TOOLCHAIN_DIR/" $COMMON_FLAGS $PACKAGE_FLAGS $INCLUDES \
    "$SCRIPT_DIR/plugin_manager_smoke.c" \
    "$SCRIPT_DIR/../core/plugin_manager.c" \
    "$SCRIPT_DIR/../core/file_list.c" \
    "$SCRIPT_DIR/../core/browser_log.c" -ldl \
    -o "$WORK_DIR/plugin-manager-smoke"
"$WORK_DIR/plugin-manager-smoke" "$PLUGIN_DIR" "$MARKER" \
    "$FAILING_PLUGIN_DIR"
