# Browser 移植到 RK3506 交接文档

本文面向万象奥科 RK3506 Linux SDK，目标板以
`HD-RK3506-IOT-EMMC`/`HD-RK3506B-IOT-EMMC` 为例。该方案使用 SDK 的
Qt 5.15.11、Linux framebuffer 和 evdev touch，不直接复用当前 QEMU 的
AArch64 rootfs。

## 1. 当前代码状态

Browser 有两个 UI 后端：

```text
apps/Browser/app + core + media + platform  # 现有 C 业务和媒体后端
apps/Browser/pages + ui                     # 现有 framebuffer UI
apps/Browser/qt                              # Qt Widgets UI（Qt 5.15）
```

Qt 前端当前已经支持：

- 明亮、紧凑的 Qt Widgets 主窗口和导航栏。
- Gallery 图片缩略图网格。
- Files/Audio/Video/Text 栏目分类过滤。
- 图片预览和文本阅读。
- Settings 页面入口。

Qt 前端目前仍是独立的 `media-browser-qt` 程序。Audio/Video 栏目已经能够
分类显示文件，但双击播放尚未接入现有 C 的 ALSA/FFmpeg 播放线程；真机第一阶段
建议先保留 `/usr/bin/media-browser` 作为功能 fallback。

## 2. SDK 和硬件前提

建议使用 SDK 自带板级配置，不要直接修改当前 QEMU Buildroot：

```text
HD-RK3506-IOT-EMMC
HD-RK3506B-IOT-EMMC
```

至少确认：

```sh
uname -m
getconf LONG_BIT
ls -l /dev/fb0 /dev/input /dev/snd 2>/dev/null
```

RK3506 的实际架构和工具链以前 BSP 输出为准。当前 QEMU 工程使用 AArch64，
不能把 QEMU 的 `image_browser` 直接复制到 ARMv7 RK3506 系统。

## 3. 内核配置

在 SDK 的 RK3506 defconfig 中启用 LinuxFB 所需选项。文件名以 SDK 实际版本为准，
典型路径为：

```text
kernel-6.1/arch/arm/configs/vanxoak_hd_rk3506b_iot_emmc_defconfig
```

关键配置：

```text
CONFIG_APERTURE_HELPERS=y
CONFIG_DRM_FBDEV_EMULATION=y
CONFIG_DRM_FBDEV_OVERALLOC=100
CONFIG_DRM_GEM_SHMEM_HELPER=y
CONFIG_DRM_SIMPLEDRM=y
CONFIG_FB_NOTIFY=y
CONFIG_FB=y
CONFIG_FB_CFB_FILLRECT=y
CONFIG_FB_CFB_COPYAREA=y
CONFIG_FB_CFB_IMAGEBLIT=y
CONFIG_FB_SYS_FILLRECT=y
CONFIG_FB_SYS_COPYAREA=y
CONFIG_FB_SYS_IMAGEBLIT=y
CONFIG_FB_SYS_FOPS=y
CONFIG_FB_DEFERRED_IO=y
```

启动后必须出现 `/dev/fb0`，并且：

```sh
cat /sys/class/graphics/fb0/name
cat /sys/class/graphics/fb0/virtual_size
```

触摸屏还需要对应的 I2C/sensor 驱动和 evdev 输入设备。验证：

```sh
cat /proc/bus/input/devices
ls -l /dev/input/event*
```

## 4. Buildroot 文件系统配置

关闭 SDK 默认的 LVGL demo（如果该板级配置默认启用），启用 Qt 5：

```text
BR2_PACKAGE_QT5=y
BR2_PACKAGE_QT5BASE=y
BR2_PACKAGE_QT5BASE_GUI=y
BR2_PACKAGE_QT5BASE_WIDGETS=y
BR2_PACKAGE_QT5BASE_LINUXFB=y
BR2_PACKAGE_QT5BASE_NETWORK=y
BR2_PACKAGE_QT5BASE_SQL=y
BR2_PACKAGE_QT5BASE_SQLITE_NONE=y
BR2_PACKAGE_QT5BASE_TEST=y
BR2_PACKAGE_QT5BASE_XML=y
BR2_PACKAGE_QT5SCRIPT=y
BR2_PACKAGE_QT5SERIALBUS=y
BR2_PACKAGE_QT5SERIALPORT=y
```

Browser 现有后端还需要：

```text
freetype
alsa-lib
libjpeg
libpng
giflib
mpg123
ffmpeg（libavformat/libavcodec/libavutil/libswscale/libswresample）
```

如果要启用 RKMPP 视频硬解，BSP 的 FFmpeg 必须实际包含
`h264_rkmpp`/`hevc_rkmpp`，否则程序会自动回退软件 decoder。

修改 menuconfig 后保存板级 defconfig：

```sh
cd /work/rk3506_linux6.1_sdk_v1.2.0/buildroot
cp output/rockchip_hd_rk3506b_iot_emmc/.config \
   configs/rockchip_hd_rk3506b_iot_emmc_defconfig
```

## 5. 编译 SDK

使用万象奥科 SDK 的标准流程：

```sh
cd /work/rk3506_linux6.1_sdk_v1.2.0
./build.sh
```

确认 Qt 构建结果：

```sh
ls output/rockchip_hd_rk3506b_iot_emmc/host/usr/bin/qmake
ls output/rockchip_hd_rk3506b_iot_emmc/target/usr/lib/libQt5*
```

## 6. 编译 Qt 前端

使用 SDK 生成的 target qmake，不要使用主机 `/usr/bin/qmake`：

```sh
SDK=/work/rk3506_linux6.1_sdk_v1.2.0
OUT=$SDK/buildroot/output/rockchip_hd_rk3506b_iot_emmc
BUILD=/tmp/media-browser-qt-rk3506

rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

"$OUT/host/usr/bin/qmake" \
    /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser/qt/media-browser-qt.pro
make -j"$(nproc)"
```

如果 SDK 的 qmake 不在 `host/usr/bin`，从 `find "$OUT/host" -name qmake` 的结果中
选择 target qmake。编译产物必须用 `file` 检查：

```sh
file media-browser-qt
```

## 7. 安装到 rootfs

```sh
TARGET="$OUT/target"
install -D -m 0755 media-browser-qt \
    "$TARGET/usr/bin/media-browser-qt"
install -D -m 0644 \
    /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser/qt/qt_env.sh \
    "$TARGET/etc/profile.d/qt_env.sh"
```

`qt_env.sh` 内容建议：

```sh
export QTDIR=/usr/lib
export QT_ROOT=/usr/lib
export QT_FONT_DIR=/usr/share
export LD_LIBRARY_PATH=/usr/lib:${LD_LIBRARY_PATH:-}
export QT_QPA_PLATFORM='linuxfb:fb=/dev/fb0'
export QT_QPA_GENERIC_PLUGINS=evdevtouch
export QT_QPA_FONTDIR=/usr/share/fonts
```

触摸屏设备名和 event 编号应由启动脚本动态探测，不要永久写死
`/dev/input/eventX`。字体使用 SDK 中已安装的开源字体，例如
`/usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc`。

## 8. 真机启动

串口登录 RK3506 后：

```sh
source /etc/profile.d/qt_env.sh
/usr/bin/media-browser-qt /root/media
```

也可以先临时覆盖平台参数：

```sh
QT_QPA_PLATFORM='linuxfb:fb=/dev/fb0' \
QT_QPA_GENERIC_PLUGINS=evdevtouch \
QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/eventX \
/usr/bin/media-browser-qt /root/media
```

常见错误：

```text
Could not find the Qt platform plugin "linuxfb"
```

表示 Qt LinuxFB plugin 未安装或 `QT_QPA_PLATFORM_PLUGIN_PATH` 不正确。

```text
Permission denied: /dev/fb0
```

表示运行用户没有 framebuffer 权限，需要调整设备权限或用 root 验证。

```text
No suitable input device
```

表示触摸设备路径不对，应根据 `/proc/bus/input/devices` 动态查找 event 节点。

## 9. 现有 C 后端需要携带的代码

Qt UI 迁移不需要删除现有模块，以下代码应继续编译进 RK3506 版本：

```text
core/browser_config.*       配置持久化
core/file_list.*            文件扫描、排序、搜索
core/file_watcher.*         inotify 热更新
core/plugin_manager.*       动态插件
core/watchdog.*             watchdog
media/image/*               BMP/JPEG/PNG
media/animation/*           GIF
media/audio/*               WAV/MP3/元数据
media/video/*               FFmpeg/RKMPP/字幕
platform/input/*            evdev 键盘和触摸
deploy/*                    启动、自动挂载、OTA
```

后续 Qt 接入建议增加 C/C++ bridge，而不是让 Qt 页面直接操作 ALSA 或 FFmpeg：

```text
Qt page
  ↓ signal/slot
browser_media_bridge
  ↓ C API
audio_player / media_player / video_decoder
```

## 10. 验收清单

```text
[ ] file 显示 ARM/RK3506 目标架构
[ ] /dev/fb0 存在且分辨率正确
[ ] Qt linuxfb plugin 可加载
[ ] 触摸 event 节点可识别
[ ] 中文字体正常显示
[ ] Gallery 图片预览正常
[ ] Files/Audio/Video/Text 分类正常
[ ] WAV/MP3 播放正常
[ ] MP4 播放正常
[ ] h264_rkmpp/hevc_rkmpp 可用或 software fallback 正常
[ ] watchdog、自动挂载和开机启动正常
```

## 11. 当前未完成事项

- Qt Audio 页面尚未连接现有 ALSA 播放线程。
- Qt Video 页面尚未连接现有 FFmpeg/RKMPP 播放线程。
- Qt 程序还未加入当前 QEMU rootfs；QEMU 继续运行 C framebuffer 版本。
- RK3506 OTA 分区路径必须由实际产品分区表填写，不能沿用 QEMU 分区。
