#!/bin/sh
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-linuxfb:fb=/dev/fb0}"
export QT_QPA_GENERIC_PLUGINS="${QT_QPA_GENERIC_PLUGINS:-evdevtouch}"
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="${QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS:-/dev/input/event1}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts/ttf}"
# 板卡 Qt 插件目录（qsvgicon/imageformats），缺省不搜这里 → SVG QIcon 需指定
export QT_PLUGIN_PATH="${QT_PLUGIN_PATH:-/usr/lib/plugins}"
