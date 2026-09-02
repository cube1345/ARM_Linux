#!/bin/sh
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-linuxfb:fb=/dev/fb0}"
export QT_QPA_GENERIC_PLUGINS="${QT_QPA_GENERIC_PLUGINS:-evdevtouch}"
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="${QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS:-/dev/input/event0}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
