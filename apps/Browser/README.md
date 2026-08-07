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
- 使用 FreeType 分页显示 UTF-8 文本，支持中文；Settings 可调整全局字体大小和播放模式。
- 后台播放 PCM WAV 和 MP3，支持暂停、软件音量、进度显示和 seek；MP3 页面读取
  ID3v2.3、ID3v2.4 和 ID3v1 的标题、艺术家与专辑标签，并在正常退出媒体页时
  保存断点位置。
- Player 支持 MP4、MOV、MKV、AVI、WebM、M4V 视频，以及 AAC、M4A、FLAC、OGG、
  Opus 音频；FFmpeg 负责解复用、视频转 RGB 和音频重采样。
- Player 触摸页提供 `PLAY/PAUSE`、进度条和音量条；文件页提供 `UP` 父目录和
  `HOME` 桌面入口，适合不接键盘的设备。
- 视频帧按容器 PTS 和单调时钟控制显示节奏，带音轨和纯视频文件均按正常速度播放。
- 视频画面支持 FIT 留边、FILL 裁切铺满和 1:1 原始大小，并可隐藏控件全屏播放。
- 启动后进入简约软件桌面，Gallery、Player、Files、Reader、Diagnostics、Tools、
  Settings 按功能提供独立入口。
- Tools 应用以白名单方式运行现有 Linux ARM 命令，可查看 ALSA、mpg123、strace、
  framebuffer 截图和 input 查询等工具输出。
- 同时支持 Linux Input 键盘、标准输入、绝对坐标触摸与相对坐标鼠标设备，
  可用 `auto` 自动发现 evdev 节点，并以 input operation 链表统一分发。
- 文件列表显示文件大小与修改时间，支持名称、类型、时间和大小排序。
- 文件列表支持递归搜索：按 `/` 或顶部 `SEARCH` 进入，输入文件名片段，按退格删除。
- 使用 framebuffer 离屏缓冲区完成整帧刷新。
- 提供统一日志模块，支持通过 `BROWSER_LOG_LEVEL` 控制 ERROR/WARN/INFO/DEBUG。
- Settings 会持久化字体大小、音量、文件排序、播放模式和最近一次媒体断点；默认路径为
  `/etc/media-browser.conf`，可用 `BROWSER_CONFIG_PATH` 指定可写路径。
- 采用深色简约 UI：顶栏、文件卡片、彩色类型标签、底部操作提示、按钮和进度条。

## 架构

主程序已经按“应用上下文、页面 operation、解码/播放 backend、设备适配和调试”
拆分：

| 模块 | 职责 |
| --- | --- |
| `app/main.c` | CLI 参数解析、初始化/释放、周期刷新和主事件循环 |
| `app/browser_app.c/.h` | 共享上下文、页面枚举、文件类型 helper 和跨页面资源收尾 |
| `core/debug_manager.c/.h` | Diagnostics 状态 operation 注册、设备/FFmpeg/工具状态采集 |
| `core/desktop_app.c/.h` | Gallery、Player、Files、Reader、Diagnostics、Tools、Settings 应用注册 |
| `pages/desktop/page_desktop.c/.h` | 软件桌面卡片、应用选择、键盘和触摸入口 |
| `core/page_manager.c/.h` | 页面 operation 注册、查找、渲染、输入分发、周期任务和事件等待时间调整 |
| `pages/files/page_file.c/.h` | 文件列表渲染、目录进入/返回、文件页键盘和触摸处理 |
| `pages/gallery/page_image.c/.h` | 图片/GIF 打开关闭、相邻图片选择、自动播放、预解码和旋转 |
| `pages/reader/page_text.c/.h` | UTF-8 文本分页渲染、键盘和触摸翻页处理 |
| `pages/player/page_audio.c/.h` | 音频页渲染、播放暂停、seek 和音量控制 |
| `media/audio/audio_metadata.c/.h` | MP3 ID3v2/ID3v1 标题、艺术家和专辑标签解析 |
| `media/video/media_player.c/.h` | FFmpeg 容器、视频和音频解码线程、RGB 帧快照和 ALSA 输出 |
| `pages/player/page_video.c/.h` | 通用媒体页面、视频帧显示、进度和音量控制 |
| `pages/tools/page_tools.c/.h` | 外部 Linux 命令白名单、命令执行、输出采集和工具页输入处理 |
| `ui/browser_ui.c/.h` / `ui/ui_draw.c/.h` | 公共 UI 常量、按钮/进度条 helper、矩形与文字绘制 |
| `core/browser_log.c/.h` | 统一日志等级、环境变量初始化和 errno 日志输出 |
| `media/image/image_decoder.c/.h` | 静态图片 decoder manager，当前注册 BMP、JPEG、PNG |
| `media/animation/*` | 动画 decoder manager 与 GIF 帧合成、延时和 disposal 状态 |
| `media/audio/audio_player.c/.h` | WAV/MP3 后台播放线程、backend 表、暂停、音量、seek 与状态快照 |
| `platform/display/*` | Framebuffer 设备、mmap 显存和 video buffer 离屏刷新 |
| `platform/display/display_manager.c/.h` | framebuffer 显示 operation 注册、打开/关闭和当前设备状态 |
| `platform/input/*` | Linux Input 键盘、stdin、绝对触摸、相对鼠标和 evdev 自动发现 |
| `platform/font/*` | FreeType 字体加载、UTF-8 解码和文字绘制 |
| `platform/font/font_manager.c/.h` | FreeType/UTF-8 字体 operation 注册、打开和像素大小切换 |
| `media/image/*` | BMP/JPEG/PNG 图片数据、decoder manager 和缩放渲染 |

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

## 自动化验证

Host smoke test 会生成 4/8/24/32-bit、RLE4/RLE8 BMP、PNG、JPEG、GIF、WAV
和 ID3 标签，并使用 Buildroot target 中已有的 MP3/MP4，检查正常解码、配置
读写、文件排序/搜索、音频元数据、三种画面缩放模式、空目录和损坏文件拒绝逻辑：

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make test
```

QEMU 或开发板中可以运行输入自动发现和 SIGTERM 资源清理测试：

```sh
./tests/input_auto_smoke.sh
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
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
./scripts/start-qemu.sh --fb
```

启动脚本会检查旧 QEMU 进程和 SSH 端口。如果需要并行启动或避免修改
`rootfs.ext4`，可以使用备用端口和临时快照：

```sh
./scripts/start-qemu.sh --vnc --ssh-port 2223 --vnc-display 1 --readonly
```

目标机检查设备节点：

```sh
ls -l /dev/fb0 /dev/input/event* /dev/snd/pcmC0D0p
cat /proc/bus/input/devices
```

启动程序。第 2 个参数可传键盘 event、`stdin`、`auto` 或 `-`；第 7 个参数可传
绝对坐标触摸/相对坐标鼠标 event、`auto`，也可省略或使用 `-` 禁用。
键盘/stdin 与指针设备不能同时禁用：

```sh
/usr/bin/media-browser \
    /dev/fb0 \
    /dev/input/event0 \
    /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc \
    default \
    /dev/input/event1
```

QEMU 或上板调试时可以先使用自动发现，程序会扫描 `/dev/input/event0` 到
`/dev/input/event31`：

```sh
/usr/bin/media-browser \
    /dev/fb0 \
    auto \
    /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc \
    default \
    auto
```

兼容原来的五参数或六参数命令行；不传最后一个参数时只使用键盘或 stdin。
串口/终端调试时可用标准输入替代 evdev 键盘：

```sh
/usr/bin/media-browser /dev/fb0 stdin /root/media /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc default -
```

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
| 文件列表 | `Tab` 或 stdin `O` | 循环切换名称/类型/时间/大小排序 |
| 文件列表 | `/` | 递归搜索文件名；搜索模式下直接输入字符 |
| 文件列表 | `Esc` / `Backspace` | 返回父目录；根目录退出 |
| stdin | `W/A/S/D`、`H/J/K/L`、`Enter`、`B`、`Q` | 上下左右、打开、返回、退出 |
| 图片 | `Left` / `Right` | 上一张 / 下一张 |
| 图片 | `Space` | 开启 / 关闭自动播放 |
| 图片 | `R` | 顺时针旋转 90 度 |
| 文本 | `Left` / `Right` | 上一页 / 下一页 |
| 音频 | `Space` | 暂停 / 继续 |
| 音频 | `Left` / `Right` | 后退 / 前进 5% |
| 音频 | `Up` / `Down` | 上一首 / 下一首 |
| 音频 | `-` / `+` | 音量降低 / 增加 5% |
| Player 媒体 | `Space` | 暂停 / 继续 |
| Player 媒体 | `Left` / `Right` | 后退 / 前进 10 秒 |
| Player 媒体 | `Up` / `Down` | 上一个 / 下一个媒体 |
| Player 媒体 | `-` / `+` | 音量降低 / 增加 5% |
| Player 视频 | `R` | 循环切换 FIT、FILL、1:1 缩放模式 |
| Player 视频 | `Enter` | 进入 / 退出隐藏控件的全屏模式 |
| Settings | `Left` / `Right` 或 `-` / `+` | 音量降低 / 增加 5% |
| Settings | `Up` / `Down` | 字体增大 / 缩小 |
| Settings | `R` | 循环切换单次、单曲循环、列表循环和随机播放 |
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
| Player 视频 | 顶部 `FIT/FILL/1:1` | 循环切换画面缩放模式 |
| Player 视频 | 顶部 `FULL` | 进入全屏；全屏中点击任意位置退出 |
| Settings | 点击音量条或 `- 5` / `+ 5` | 设置软件音量 |
| Settings | 点击 `A -` / `A +` | 缩小 / 增大全局字体 |
| Settings | 点击 `CHANGE` | 循环切换播放模式 |
| Tools | 点击命令条目 | 运行选中命令并显示输出 |
| 媒体页面 | 左上角 `<` | 返回文件列表 |
| Player 媒体 | 顶部 `PLAY/PAUSE` | 暂停 / 继续 |
| 文件列表 | 顶部 `UP` | 返回父目录 |
| 文件列表 | 顶部排序按钮 | 循环切换名称/类型/时间/大小排序 |

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
- 带 ID3 标签的 MP3 可显示标题、艺术家和专辑；无标签文件回退显示文件名。
- 音频或视频播放超过 3 秒后退出，在距离结尾 3 秒以上时重新打开同一文件可从
  上次位置继续播放；接近开头或结尾不保存断点。
- 视频页可通过 `R`/缩放按钮检查 FIT、FILL、1:1，通过 `Enter`/`FULL` 检查
  隐藏控件的全屏显示；全屏时 `Esc` 或点击画面只退出全屏，不关闭媒体。
- 图片页 `Space` 或 `AUTO ON/OFF` 可切换自动播放，约 3 秒自动切换下一张。
- BMP/JPEG/PNG 相邻切换可复用后台预解码结果；GIF 继续由动画 decoder 实时播放。
- `BROWSER_LOG_LEVEL=debug` 启动时可看到 input、decoder、audio 和文件列表等模块日志。
