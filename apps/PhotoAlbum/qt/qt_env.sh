#!/bin/sh
# 强制 UTF-8 locale：否则 Qt 对 UTF-8 文件名按 local8bit 解码 → 中文歌名乱码
export LANG="${LANG:-C.UTF-8}"
export LC_ALL="${LC_ALL:-C.UTF-8}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-linuxfb:fb=/dev/fb0}"
export QT_QPA_GENERIC_PLUGINS="${QT_QPA_GENERIC_PLUGINS:-evdevtouch}"
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="${QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS:-/dev/input/event1}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts/ttf}"
# 板卡 Qt 插件目录（qsvgicon/imageformats），缺省不搜这里 → SVG QIcon 需指定
export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-/usr/lib/plugins}"
