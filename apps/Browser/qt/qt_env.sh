#!/bin/sh

# Qt 5.15 runtime defaults for RK3506 LinuxFB images.
export QTDIR=/usr/lib
export QT_ROOT=/usr/lib
export QT_FONT_DIR=/usr/share
export LD_LIBRARY_PATH=/usr/lib:${LD_LIBRARY_PATH:-}
export QT_QPA_PLATFORM='linuxfb:fb=/dev/fb0'
export QT_QPA_GENERIC_PLUGINS=evdevtouch
export QT_QPA_FONTDIR=/usr/share/fonts

touch_name=${QT_TOUCH_DEVICE_NAME:-Goodix Capacitive TouchScreen}
for input_dir in /sys/class/input/input*; do
    [ -r "$input_dir/name" ] || continue
    device_name=$(cat "$input_dir/name")
    if [ "$device_name" = "$touch_name" ]; then
        input_number=${input_dir##*input}
        export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/event${input_number}
        break
    fi
done
