# Embedded Linux Multimedia Browser

基于 Linux Framebuffer、Input、FreeType、libjpeg、libpng、giflib、ALSA 和
mpg123 和 FFmpeg 的用户态多媒体文件浏览器桌面。

## 功能

- 浏览目录，仅显示子目录和受支持的媒体文件。
- 显示 BMP、JPEG、PNG 和 GIF。
- BMP 支持未压缩 4/8/24/32-bit，以及 BI_RLE4、BI_RLE8。
- PNG 支持 palette、gray、16-bit、interlace 和 alpha；透明像素合成到黑色。
- GIF 支持动画循环、透明、局部帧、帧延时以及 disposal 0/1/2/3。
- 图片等比例缩放、居中显示，支持顺时针 90 度旋转、自动播放和
  BMP/JPEG/PNG 下一张图片后台预解码。
- 使用 FreeType 分页显示 UTF-8 文本，支持中文。
- 后台播放 PCM WAV 和 MP3，支持暂停、软件音量、进度显示和 seek。
- Player 支持 MP4、MOV、MKV、AVI、WebM、M4V 视频，以及 AAC、M4A、FLAC、OGG、
  Opus 音频；FFmpeg 负责解复用、视频转 RGB 和音频重采样。
- Player 触摸页提供 `PLAY/PAUSE`、进度条和音量条；文件页提供 `UP` 父目录和
  `HOME` 桌面入口，适合不接键盘的设备。
- 视频帧按容器 PTS 和单调时钟控制显示节奏，带音轨和纯视频文件均按正常速度播放。
- 启动后进入简约软件桌面，Gallery、Player、Files、Reader、Diagnostics、Tools、
  Settings 按功能提供独立入口。
- Tools 应用以白名单方式运行现有 Linux ARM 命令，可查看 ALSA、mpg123、strace、
  framebuffer 截图和 input 查询等工具输出。
- 同时支持 Linux Input 键盘与绝对坐标触摸设备，并以 input operation
  链表统一分发。
- 使用 framebuffer 离屏缓冲区完成整帧刷新。
- 提供统一日志模块，支持通过 `BROWSER_LOG_LEVEL` 控制 ERROR/WARN/INFO/DEBUG。
- 采用深色简约 UI：顶栏、文件卡片、彩色类型标签、底部操作提示、按钮和进度条。

## 架构

主程序已经按“应用上下文、页面 operation、解码/播放 backend、设备适配和调试”
拆分：

| 模块 | 职责 |
| --- | --- |
| `image_browser.c` | CLI 参数解析、初始化/释放、周期刷新、页面 manager 调度和主事件循环 |
| `browser_app.c/.h` | `browser_app` 共享上下文、页面枚举、文件类型 helper 和跨页面资源收尾 |
| `desktop_app.c/.h` | Gallery、Player、Files、Reader、Diagnostics、Tools、Settings 应用注册和启动 |
| `page_desktop.c/.h` | 软件桌面卡片、应用选择、键盘和触摸入口 |
| `page_manager.c/.h` | 页面 operation 注册、查找、渲染、输入分发、周期任务和事件等待时间调整 |
| `page_file.c/.h` | 文件列表渲染、目录进入/返回、文件页键盘和触摸处理 |
| `page_image.c/.h` | 图片/GIF 打开关闭、图片渲染、相邻图片选择、自动播放、静态图预解码、旋转和图片页输入处理 |
| `page_text.c/.h` | UTF-8 文本分页渲染、文本翻页键盘和触摸处理 |
| `page_audio.c/.h` | 音频页渲染、播放暂停、seek、音量条和音频页输入处理 |
| `media_player.c/.h` | FFmpeg 容器、视频和音频解码线程、RGB 帧快照、ALSA 输出 |
| `page_video.c/.h` | 通用媒体页面、视频帧显示、进度/音量控制 |
| `page_tools.c/.h` | 外部 Linux 命令白名单、命令执行、输出采集和工具页输入处理 |
| `browser_ui.c/.h` / `ui_draw.c/.h` | 公共 UI 常量、按钮/进度条 helper、矩形与文字绘制 |
| `browser_log.c/.h` | 统一日志等级、环境变量初始化和 errno 日志输出 |
| `image_decoder.c/.h` | 静态图片 decoder manager，当前注册 BMP、JPEG、PNG |
| `animation_decoder.c/.h` / `gif_animation.c/.h` | 动画 decoder manager 与 GIF 帧合成/延时/disposal 状态 |
| `audio_player.c/.h` | WAV/MP3 后台播放线程、backend 表、暂停、音量、seek 与状态快照 |
| `input_keyboard.c/.h` | input operation 注册、poll 遍历、Linux Input 键盘与绝对坐标触摸设备归一化 |

新增格式时优先新增 decoder/backend 并注册到 manager；新增交互时优先放在对应
`page_*.c` 页面模块，避免重新膨胀主循环。

## Buildroot 配置

```sh
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_LIBPNG
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_GIFLIB
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_MPG123
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_FFMPEG
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_EVTEST
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_TSLIB
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_FBGRAB
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_FB_TEST_APP
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_ALSA_UTILS_AMIXER
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_ALSA_UTILS_SPEAKER_TEST
/home/cube/Edisk/buildroot/utils/config --file .config \
    --enable BR2_PACKAGE_STRACE
make olddefconfig
make
```

FFmpeg 最小配置需要包含 `libavformat`、`libavcodec`、`libavutil`、`libswscale`
和 `libswresample`，以及 MP4/MOV/MKV/AVI/WebM 容器、H.264、MPEG-4、VP8/VP9、
AAC、MP3、Opus、PCM 等解码器。当前 ARM64 配置已将这些库安装到 target。

## 构建与安装

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make clean
make CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
make DESTDIR=/home/cube/WorkSpace/Linux/ARM_Linux/target install

cd /home/cube/WorkSpace/Linux/ARM_Linux
make
```

## RK3506 移植准备

代码只依赖标准 Linux 用户态接口，没有 AArch64 汇编或 QEMU 专用调用。切换到
RK3506 BSP 时，先在 BSP 的 Buildroot output 中确认实际 toolchain triple：

```sh
ls <rk3506-output>/host/bin/*-gcc
```

然后使用对应 output 和 triple 构建，例如：

```sh
make clean
make BUILDROOT_OUTPUT=<rk3506-output> \
     TARGET_TRIPLE=<实际-toolchain-triple> \
     CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
make BUILDROOT_OUTPUT=<rk3506-output> \
     TARGET_TRIPLE=<实际-toolchain-triple> \
     DESTDIR=<rk3506-output>/target install
```

RK3506 rootfs 必须提供 framebuffer、Linux Input、ALSA、FreeType、JPEG、PNG、giflib、
mpg123 和 FFmpeg 动态库。上板前逐项确认：

```sh
ls -l /dev/fb0 /dev/input/event* /dev/snd/*
file /usr/bin/media-browser
ldd /usr/bin/media-browser
```

当前视频使用 FFmpeg 软件解码和 `libswscale` 转 RGB，不依赖 QEMU virtio-gpu。
RK3506 首次上板先以 320x240、480p 视频验收 CPU 占用和帧率，再决定是否接入
Rockchip 硬件解码接口；硬件加速不影响桌面和页面层接口。

已在独立 Buildroot ARMv7 output 中完成 Cortex-A7 hard-float 门禁：

```sh
make O=/tmp/browser-rk3506-qemu-armv7 \
     BR2_DL_DIR=/home/cube/Edisk/buildroot/dl -j16
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make BUILDROOT_OUTPUT=/tmp/browser-rk3506-qemu-armv7 \
     TARGET_TRIPLE=arm-buildroot-linux-gnueabihf \
     CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
```

产物为 `ARM EABI5`、`VFPv3-D16`、hard-float ELF。使用 `virt + cortex-a7`
QEMU 已验证 rootfs、动态库、FreeType 字体、测试 MP4 和 Browser usage 可运行。
`vexpress-a9` 只能用于 Cortex-A9 用户态门禁，不能直接运行开启 ARMv7 分频指令
的 Cortex-A7 构建；RK3506 上板应使用实际 BSP toolchain 和硬件设备节点验收。

## 嵌入式设备 QEMU 虚拟机启动

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux/images
./start-qemu-scp.sh --fb
```

目标机检查设备节点：

```sh
ls -l /dev/fb0 /dev/input/event* /dev/snd/pcmC0D0p
cat /proc/bus/input/devices
```

启动程序。键盘参数和触摸参数都可使用 `-` 禁用，但不能同时禁用：

```sh
/usr/bin/media-browser \
    /dev/fb0 \
    /dev/input/event0 \
    /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc \
    default \
    /dev/input/event1
```

兼容原来的五参数或六参数命令行；不传最后一个参数时只使用键盘。

## 设备调试工具

```sh
# 查看键盘、鼠标或触摸事件。
evtest /dev/input/event1

# 校准真实触摸屏。
TSLIB_TSDEVICE=/dev/input/event1 \
TSLIB_FBDEVICE=/dev/fb0 \
ts_calibrate

# 保存 framebuffer 截图并运行像素测试。
fbgrab /tmp/screen.png
fb-test

# 查看 ALSA 控件并测试立体声音频输出。
amixer scontrols
speaker-test -D default -c 2

# 记录 media-browser 的 syscall。
strace -f -o /tmp/browser.strace /usr/bin/media-browser \
    /dev/fb0 /dev/input/event0 /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc default \
    /dev/input/event1
```

## 键盘操作

| 页面 | 按键 | 动作 |
| --- | --- | --- |
| 文件列表 | `Up` / `Down` | 选择条目 |
| 文件列表 | `Enter` | 打开目录或媒体 |
| 文件列表 | `Esc` / `Backspace` | 返回父目录；根目录退出 |
| 图片 | `Left` / `Right` | 上一张 / 下一张 |
| 图片 | `Space` | 开启 / 关闭自动播放 |
| 图片 | `R` | 顺时针旋转 90 度 |
| 文本 | `Left` / `Right` | 上一页 / 下一页 |
| 音频 | `Space` | 暂停 / 继续 |
| 音频 | `Left` / `Right` | 后退 / 前进 5% |
| 音频 | `-` / `+` | 音量降低 / 增加 5% |
| Player 媒体 | `Space` | 暂停 / 继续 |
| Player 媒体 | `Left` / `Right` | 后退 / 前进 5% |
| Player 媒体 | `-` / `+` | 音量降低 / 增加 5% |
| Tools | `Up` / `Down` | 选择外部命令 |
| Tools | `Enter` | 运行选中命令并显示输出 |
| Tools | `Esc` / `Backspace` | 返回桌面 |
| 媒体页面 | `Esc` / `Backspace` | 返回文件列表 |
| 任意页面 | `Q` | 退出程序 |

## 触摸操作

| 页面 | 手势或控件 | 动作 |
| --- | --- | --- |
| 文件列表 | 点击条目 | 打开条目 |
| 文件列表 | 上下滑动 | 分页浏览 |
| 图片 | 左右滑动 | 下一张 / 上一张 |
| 图片 | `AUTO ON/OFF` | 开启 / 关闭自动播放 |
| 图片 | `ROTATE` | 顺时针旋转 90 度 |
| 文本 | 左右滑动 | 下一页 / 上一页 |
| 音频 | `PLAY / PAUSE` | 暂停 / 继续 |
| 音频 | 点击或拖动进度条 | seek |
| 音频 | 点击或拖动音量条 | 设置软件音量 |
| Player 媒体 | 点击或拖动进度条 | seek 视频或 FFmpeg 音频 |
| Player 媒体 | 点击或拖动音量条 | 设置软件音量 |
| Tools | 点击命令条目 | 运行选中命令并显示输出 |
| 媒体页面 | 左上角 `<` | 返回文件列表 |
| Player 媒体 | 顶部 `PLAY/PAUSE` | 暂停 / 继续 |
| 文件列表 | 顶部 `UP` | 返回父目录 |

触摸坐标通过 `EVIOCGABS` 从设备量程映射到 framebuffer 分辨率。点击允许
15 像素移动；滑动阈值为 32 像素与屏幕对应轴 5% 中的较大值。

## 验收清单

- `make clean` 后可重新完整构建。
- `make CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'` 无 warning/error。
- `git diff --check -- apps/Browser` 无空白错误。
- `make DESTDIR=/home/cube/WorkSpace/Linux/ARM_Linux/target install` 生成
  `target/usr/bin/media-browser`。
- 在 `/home/cube/WorkSpace/Linux/ARM_Linux` 执行 `make` 后生成
  `images/rootfs.ext4`。
- QEMU 中使用 `default` ALSA device 启动，文件列表、图片/GIF、文本、
  WAV/MP3、FFmpeg 媒体页面、键盘和触摸路径均可进入对应页面。
- 图片页 `Space` 或 `AUTO ON/OFF` 可切换自动播放，约 3 秒自动切换下一张。
- BMP/JPEG/PNG 相邻切换可复用后台预解码结果；GIF 继续由动画 decoder 实时播放。
- `BROWSER_LOG_LEVEL=debug` 启动时可看到 input、decoder、audio 和文件列表等模块日志。
