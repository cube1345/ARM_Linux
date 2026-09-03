# Qt 触摸相册开发、编译与上板流程

本文以当前 `PhotoAlbum` 工程为例，总结 i.MX6ULL 板端 Qt Widgets 程序的编写方式、交叉编译、传输、运行和常见问题处理。目标环境如下：

- 主机：Ubuntu 24.04 x86_64
- SDK：NXP i.MX Qt 5.12.9 ARM SDK
- 板卡：ALIENTEK i.MX6ULL，4.3 英寸 480x272 触摸屏
- 运行后端：`linuxfb + evdevtouch`，不依赖 X11/Wayland

## 1. 程序结构

```text
PhotoAlbum/qt
├── photo_album.pro     # qmake 工程定义
├── main.cpp             # QApplication 入口和图片目录解析
├── main_window.h/.cpp   # 主窗口、按钮、图片列表、保存流程
├── photo_view.h/.cpp    # 图片绘制、触摸、手势、裁剪选择
├── qt_env.sh            # 板端 LinuxFB 和触摸设备环境
└── README.md            # 快速使用说明
```

### qmake 工程文件

`photo_album.pro` 是 qmake 的入口：

```pro
QT += widgets
CONFIG += c++11
TEMPLATE = app
TARGET = photo-album
SOURCES += main.cpp main_window.cpp photo_view.cpp
HEADERS += main_window.h photo_view.h
```

含义：

- `QT += widgets`：使用 QtWidgets 模块。
- `TEMPLATE = app`：生成可执行程序，而不是库。
- `TARGET = photo-album`：输出文件名。
- `SOURCES` 和 `HEADERS`：源码与头文件列表。新增 `.cpp/.h` 后要同步更新。

### Qt 程序入口

`main.cpp` 负责：

1. 创建 `QApplication`；
2. 解析命令行中的图片目录；
3. 没有传目录时依次尝试 `./photos` 和 `/root/photos`；
4. 创建并显示主窗口；
5. 进入 Qt 事件循环。

核心流程：

```cpp
QApplication application(argc, argv);
MainWindow window(photoDirectory);
#ifdef __arm__
window.showFullScreen();
#else
window.resize(480, 272);
window.show();
#endif
return application.exec();
```

`__arm__` 分支让程序在板端全屏，在主机调试时使用 480x272 窗口。

## 2. 界面与交互编写方式

### 主窗口

`MainWindow` 继承 `QMainWindow`，负责业务层：

- 扫描和加载图片；
- 维护当前图片索引；
- 切换上一张/下一张；
- 创建底部按钮；
- 执行裁剪、复位、保存；
- 显示标题和状态信息。

Qt 的信号槽负责解耦 UI 事件和业务函数。例如：

```cpp
connect(previousButton, SIGNAL(clicked()),
        this, SLOT(showPrevious()));
connect(nextButton, SIGNAL(clicked()),
        this, SLOT(showNext()));
```

自定义控件也可以发信号：

```cpp
connect(photoView, SIGNAL(previousRequested()),
        this, SLOT(showPrevious()));
connect(photoView, SIGNAL(nextRequested()),
        this, SLOT(showNext()));
```

这样 `PhotoView` 只负责识别滑动，不需要知道主窗口如何切换图片。

### 图片视图与触摸手势

`PhotoView` 继承 `QWidget`，负责显示和交互：

- `paintEvent()`：绘制当前图片和裁剪遮罩；
- `mousePressEvent()`：记录按下位置，区分拖动、裁剪和滑动；
- `mouseMoveEvent()`：拖动平移或更新裁剪框；
- `mouseReleaseEvent()`：判断是否触发上一张/下一张；
- `mouseDoubleClickEvent()`：双击复位；
- `event()`：处理 `QGestureEvent`；
- `applyPinch()`：根据 `QPinchGesture` 更新缩放倍数。

当前手势行为：

| 操作 | 行为 |
|---|---|
| 单指水平快速滑动 | 上一张/下一张 |
| 放大后单指拖动 | 平移图片 |
| 双指捏合 | 缩放，范围 1x 到 8x |
| 双击 | 复位视图 |
| 裁剪模式下拖动 | 选择裁剪区域 |

裁剪结果保存在 `MainWindow::editedImages` 中，只修改当前会话内存数据；点击“保存”后导出为 PNG 文件。

## 3. 主机调试编译

主机调试用于快速检查代码逻辑和窗口布局，但主机生成的 x86 二进制不能上板。

```sh
mkdir -p /tmp/photoalbum-host-build
cd /tmp/photoalbum-host-build
qmake /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/photo_album.pro
make -j2
./photo-album
```

如果当前 shell 没有 GUI，可使用 offscreen 后端做冒烟测试：

```sh
QT_QPA_PLATFORM=offscreen ./photo-album
```

offscreen 模式不会真正显示窗口，适合检查程序是否能启动、是否立刻崩溃。

## 4. ARM 交叉编译

先加载 NXP SDK 环境：

```sh
. /opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi
qmake -v
```

期望看到：

```text
QMake version 3.1
Using Qt version 5.12.9
```

然后 shadow build：

```sh
mkdir -p /tmp/photoalbum-arm-build
cd /tmp/photoalbum-arm-build
qmake /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/photo_album.pro
make -j2
file photo-album
```

期望结果：

```text
ELF 32-bit LSB executable, ARM, EABI5
```

检查动态依赖：

```sh
readelf -d photo-album | grep NEEDED
```

主要依赖应为板端 Qt 库：

```text
libQt5Widgets.so.5
libQt5Gui.so.5
libQt5Core.so.5
libstdc++.so.6
libc.so.6
```

当前工程的已验证 ARM 产物保存在：

```text
ARM_Linux/apps/PhotoAlbum/qt/.dist/photo-album
```

## 5. 传输到板卡

### 串口/ZMODEM 方式

板卡执行：

```sh
mkdir -p /root/photos
cd /root
rz -y
```

从主机发送：

```text
ARM_Linux/apps/PhotoAlbum/qt/.dist/photo-album
ARM_Linux/apps/PhotoAlbum/qt/qt_env.sh
```

再传图片：

```sh
cd /root/photos
rz -y
```

支持格式取决于板端 Qt imageformats 插件，常见为 JPEG、PNG、BMP、GIF。

### 网络/scp 方式

如果板卡网络已配置：

```sh
scp ARM_Linux/apps/PhotoAlbum/qt/.dist/photo-album root@<板卡IP>:/root/
scp ARM_Linux/apps/PhotoAlbum/qt/qt_env.sh root@<板卡IP>:/root/
scp <图片文件> root@<板卡IP>:/root/photos/
```

## 6. 板端运行

### 停掉已有界面程序

LinuxFB 没有窗口合成器。如果已有 Qt 程序或 `psplash` 正在写 `/dev/fb0`，两个程序会互相覆盖画面，并可能同时响应触摸。

先查找占用 framebuffer 的进程：

```sh
for p in /proc/[0-9]*; do
    if ls -l "$p/fd" 2>/dev/null | grep -q '/dev/fb0'; then
        echo "${p##*/} $(cat "$p/cmdline" | tr '\0' ' ')"
    fi
done
```

停止对应 PID：

```sh
kill <PID>
```

如果进程是前台程序，也可以在其终端按 `Ctrl+C`。

### 配置 Qt 运行环境

`qt_env.sh` 内容：

```sh
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-linuxfb:fb=/dev/fb0}"
export QT_QPA_GENERIC_PLUGINS="${QT_QPA_GENERIC_PLUGINS:-evdevtouch}"
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS="${QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS:-/dev/input/event1}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
```

含义：

- `linuxfb:fb=/dev/fb0`：直接使用 Linux framebuffer；
- `evdevtouch`：使用 Qt 的 evdev 触摸输入插件；
- `/dev/input/event1`：触摸设备路径，需按实际板卡修改；
- `QT_QPA_FONTDIR`：指定字体目录。

### 启动

```sh
cd /root
chmod +x photo-album qt_env.sh
. ./qt_env.sh
./photo-album /root/photos
```

如果 `/root/photos` 为空，程序会自动生成 3 张演示图，用于先验证界面和触摸。

## 7. 常见问题

### 程序无法显示

检查：

```sh
ls -l /dev/fb0
echo $QT_QPA_PLATFORM
```

如果提示找不到 `linuxfb` 插件，检查板端：

```sh
find /usr/lib -name 'libqlinuxfb*'
```

并设置：

```sh
export QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins
```

### 触摸无响应

检查输入设备：

```sh
cat /proc/bus/input/devices
ls -l /dev/input/event*
```

根据 `Handlers` 找到触摸对应的 `/dev/input/eventX`，修改 `/root/qt_env.sh` 后重新加载：

```sh
. ./qt_env.sh
```

### 画面被其他程序覆盖

说明还有 `systemui`、`psplash` 或其他 GUI 程序在写 `/dev/fb0`。再次执行占用检查并停止对应进程。

如果重启后自动恢复，再查启动脚本：

```sh
grep -R "systemui\|psplash" /etc/init.d /etc/rc.local 2>/dev/null
```

确认启动位置后再处理，不要盲目修改系统脚本。

### 中文不显示

检查字体目录：

```sh
ls -l /usr/share/fonts
```

没有中文字体时，需要向板端安装字体并保持 `QT_QPA_FONTDIR` 指向对应目录。

### JPEG 无法打开

检查板端 Qt JPEG 插件：

```sh
find /usr/lib -name 'libqjpeg.so'
```

如果没有该插件，可用 SDK 中的 `libqjpeg.so` 部署到板端 Qt 插件目录，或将图片转换为 PNG 后测试。

## 8. 上板验收清单

- [ ] ARM 二进制可执行；
- [ ] `/dev/fb0` 存在；
- [ ] 已停止 `psplash/systemui` 等旧界面进程；
- [ ] 主窗口全屏显示为 480x272；
- [ ] 空目录时能看到 3 张演示图；
- [ ] `/root/photos` 中图片可切换；
- [ ] 单指快速滑动能切换上一张/下一张；
- [ ] 双指捏合能缩放；
- [ ] 放大后单指拖动能平移；
- [ ] 双击能复位；
- [ ] 裁剪、应用、取消、复位功能可用；
- [ ] 保存后在 `cropped/` 目录生成 PNG；
- [ ] 触摸和显示无旧程序抢占；
- [ ] `Ctrl+C` 或界面关闭后程序能正常退出。
