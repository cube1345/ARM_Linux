# Qt 触摸相册

一个面向 i.MX6ULL 4.3 英寸 `480x272` 触摸屏的 Qt Widgets 相册应用，兼容 Qt 5.12.9。应用默认扫描 `/root/photos`，目录为空时自动生成 3 张演示图。

## 功能

- 单指水平滑动切换图片
- 单指拖动平移放大后的图片
- 双指捏合缩放，范围 `1x` 到 `8x`
- 双击复位
- 拖拽选择区域并裁剪
- 应用、取消、复位和保存当前图片
- 深色 iOS 风格界面

## 主机仿真

```sh
cd /tmp
mkdir -p photo-album-build
cd photo-album-build
qmake /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/photo_album.pro
make
./photo-album
```

## 交叉编译

使用 NXP i.MX Qt 5.12.9 SDK 提供的 ARM `qmake`：

```sh
. /opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
qmake -v
mkdir -p /tmp/photoalbum-arm-build
cd /tmp/photoalbum-arm-build
qmake /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/photo_album.pro
make -j2
file photo-album
```

不要使用主机 `/usr/bin/qmake` 生成的二进制上板。当前已验证产物位于 `.dist/photo-album`，应为 `ELF 32-bit ARM EABI5`。

## 板端运行

```sh
mkdir -p /root/photos
# 将 jpg/png/bmp/gif 图片放入 /root/photos
cd /root
. ./qt_env.sh
./photo-album /root/photos
```

如果触摸无响应，先在板卡执行：

```sh
cat /proc/bus/input/devices
for event in /dev/input/event*; do
    evtest "$event" 2>/dev/null | grep -q 'ABS_MT_POSITION_X' && echo "$event"
done
```

把 `qt_env.sh` 中的 `/dev/input/event0` 改成实际触摸设备。若板卡没有 `evtest`，以 `/proc/bus/input/devices` 中列出的 `Handlers` 为准。
