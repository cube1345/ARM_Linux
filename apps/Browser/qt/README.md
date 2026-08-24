# Qt 5.15 frontend

This directory contains the first Qt Widgets frontend for the media browser.
It is intentionally built as a separate `media-browser-qt` target while the
existing framebuffer C application remains the fallback.

The Vanxoak RK3506 SDK uses Qt 5.15.11 with the `linuxfb` QPA plugin and
`evdevtouch` input. Enable `BR2_PACKAGE_QT5`, `BR2_PACKAGE_QT5BASE`,
`BR2_PACKAGE_QT5BASE_WIDGETS`, and the required input/graphics options in the
SDK Buildroot configuration before cross-compiling this project.

Host build (Qt 5):

```sh
cd /tmp
rm -rf browser-qt-build
mkdir browser-qt-build
cd browser-qt-build
qmake /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser/qt/media-browser-qt.pro
make
```

RK3506 runtime environment:

```sh
export QT_QPA_PLATFORM='linuxfb:fb=/dev/fb0'
export QT_QPA_GENERIC_PLUGINS=evdevtouch
export QT_QPA_FONTDIR=/usr/share/fonts
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/eventX
export BROWSER_VIDEO_DECODER=auto
./media-browser-qt /root/media
```

The frontend currently provides the desktop navigation, file browser,
The Files, Audio, Video, and Text entries apply separate filename filters;
directories remain visible for navigation.
Audio playback is connected to the existing C audio backend for WAV/MP3.
Video playback is connected to the existing FFmpeg/RKMPP backend and displays
RGB24 frames through Qt; decoder selection follows the BROWSER_VIDEO_DECODER
environment variable (auto, software, or rkmpp).
