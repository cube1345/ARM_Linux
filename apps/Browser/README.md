# Embedded Linux Multimedia Browser

基于 Linux Framebuffer、Input、FreeType、libjpeg 和 ALSA 的用户态多媒体
文件浏览器。

## 功能

- 浏览目录，仅显示子目录和受支持的媒体文件。
- 显示未压缩 24/32-bit BMP 与 JPEG 图片。
- 等比例缩放并居中显示图片。
- 使用 FreeType 分页显示 UTF-8 文本，支持中文。
- 后台播放 PCM WAV，支持播放和暂停。
- 使用 framebuffer 离屏缓冲区完成整帧刷新。

当前不支持压缩 BMP、MP3、富文本和非 PCM WAV。

## 构建与安装

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make clean
make CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
make DESTDIR=/home/cube/WorkSpace/Linux/ARM_Linux/target install
```

重新生成 Buildroot 根文件系统：

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux
make
```

## QEMU 启动

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux/images
./start-qemu-scp.sh --fb
```

在目标机查找键盘事件节点：

```sh
cat /proc/bus/input/devices
ls -l /dev/input/event*
```

启动浏览器，其中最后一个可选参数是 ALSA PCM 设备名：

```sh
/usr/bin/media-browser \
    /dev/fb0 \
    /dev/input/event0 \
    /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc \
    default
```

## 按键

| 页面 | 按键 | 动作 |
| --- | --- | --- |
| 文件列表 | `Up` / `Down` | 选择条目 |
| 文件列表 | `Enter` | 打开目录或媒体 |
| 文件列表 | `Esc` / `Backspace` | 返回父目录；根目录退出 |
| 图片 | `Left` / `Right` | 上一张 / 下一张 |
| 文本 | `Left` / `Right` | 上一页 / 下一页 |
| 音频 | `Space` | 暂停 / 继续 |
| 媒体页面 | `Esc` / `Backspace` | 返回文件列表 |
| 任意页面 | `Q` | 退出程序 |

## QEMU 音频验证

目标机启动后应存在 ALSA 设备：

```sh
cat /proc/asound/cards
aplay -l
```

若没有 `/dev/snd`，先检查 QEMU 是否使用 `--fb` 或 `--vnc` 模式，以及启动
日志中是否识别 `USB Audio`。串口专用模式没有添加虚拟显示和音频设备。
