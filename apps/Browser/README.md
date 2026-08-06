# Embedded Linux Multimedia Browser

基于 Linux Framebuffer、Input、FreeType、libjpeg、libpng、giflib、ALSA 和
mpg123 的用户态多媒体文件浏览器。

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
| `page_manager.c/.h` | 页面 operation 注册、查找、渲染、输入分发、周期任务和事件等待时间调整 |
| `page_file.c/.h` | 文件列表渲染、目录进入/返回、文件页键盘和触摸处理 |
| `page_image.c/.h` | 图片/GIF 打开关闭、图片渲染、相邻图片选择、自动播放、静态图预解码、旋转和图片页输入处理 |
| `page_text.c/.h` | UTF-8 文本分页渲染、文本翻页键盘和触摸处理 |
| `page_audio.c/.h` | 音频页渲染、播放暂停、seek、音量条和音频页输入处理 |
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

## 构建与安装

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make clean
make CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
make DESTDIR=/home/cube/WorkSpace/Linux/ARM_Linux/target install

cd /home/cube/WorkSpace/Linux/ARM_Linux
make
```

## QEMU 启动

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
| 媒体页面 | 左上角 `<` | 返回文件列表 |

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
  WAV/MP3、键盘和触摸路径均可进入对应页面。
- 图片页 `Space` 或 `AUTO ON/OFF` 可切换自动播放，约 3 秒自动切换下一张。
- BMP/JPEG/PNG 相邻切换可复用后台预解码结果；GIF 继续由动画 decoder 实时播放。
- `BROWSER_LOG_LEVEL=debug` 启动时可看到 input、decoder、audio 和文件列表等模块日志。
