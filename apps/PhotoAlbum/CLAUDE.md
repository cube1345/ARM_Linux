# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 概述

i.MX6ULL 板端 Qt Widgets 触摸相册 + 监控客户端，配套一套**在 Windows 侧运行的**流媒体服务链。板卡只做拉流/软解/显示，摄像头接入、转码、切片、录像、快照全部在 Windows 完成。

系统是两半，改任何一半都要先确认另一半：

| 半边 | 位置 | 角色 |
|---|---|---|
| 板卡客户端 | `qt/` | Qt 5.12.9 C++ 程序，拉流显示、浏览照片 |
| Windows 服务链 | `qt/windows/`、`stream_server.py`、`mediamtx_v1.21.0_windows_amd64/` | 拉摄像头 → 转码 → 分发 |

## 目标环境

- 板卡：ALIENTEK i.MX6ULL，7 寸 **1024x600** 触摸屏（当前 dtb `imx6ull-14x14-emmc-7-1024x600-c.dtb`；旧文档写的 4.3 寸 480x272 是错的，触摸坐标 0~1024/0~600）
- Qt：NXP fsl-imx-x11 SDK `4.1.15-2.1.0` 提供的 **Qt 5.12.9**（`cortexa7hf-neon-poky-linux-gnueabi`）
- 运行后端：`linuxfb:/dev/fb0` + `evdevtouch:/dev/input/event1`，**无 X11/Wayland**
- 板端 ffmpeg：NXP BSP 自带 **2.8**，`libavformat.so.57 / libavcodec.so.57 / libavutil.so.55 / libswscale.so.4`；音频走 ALSA `plughw:0,0`

两个硬限制决定了所有设计取舍：

1. **板端 ffmpeg 无 TLS** → `https://` 流打不开，只能用 `http://` / `rtsp://` / `rtmp://`。板卡代码里没有报错提示，只会静默黑屏。
2. **板端软解不了 HEVC** → 当前摄像头（`192.168.114.48`，子码流 HEVC）必须经 ffmpeg 转成 H.264 才能上板（见「形态 B」）。这是转码链存在的根本原因，不是可选项。

## 构建

构建系统：**CMake**（`qt/CMakeLists.txt`）。`.pro` 文件保留作参考但不再使用。

### 主机仿真（x86，快速验证逻辑）

```sh
mkdir -p /tmp/pa-host && cd /tmp/pa-host
cmake <repo>/apps/PhotoAlbum/qt -DCMAKE_BUILD_TYPE=Release
make -j2
QT_QPA_PLATFORM=offscreen ./photo-album    # 无 GUI 环境的冒烟测试
```

主机产物**不能上板**。`main.cpp` 用 `#ifdef __arm__` 区分：板端 `showFullScreen()`，主机 `resize(...)`。

> 本机 linuxbrew 的 `ld` 会干扰系统 Qt 链接（ICU/pcre2 符号找不到），主机构建需干净 PATH：`export PATH=/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin`。

### 单元测试

```sh
cd /tmp/pa-host && ./tests/tests      # 或 ctest
```

测试为纯逻辑（2048/俄罗斯方块，`game2048_logic` / `tetris_logic`），**主机运行，不依赖板卡**。

### ARM 交叉编译

```sh
bash <repo>/apps/PhotoAlbum/qt/build.sh
```

`build.sh` 用 `cmake/toolchain-arm.cmake` → shadow build → `file` 断言产物含 "ARM" → 复制到 `qt/.dist/photo-album` → 打印 md5。

> `cmake/toolchain-arm.cmake` 里 `SDK_ROOT` 硬编码本机 SDK 路径（`<repo>/sdk/fsl-imx-x11/4.1.15-2.1.0/`），换机器需改。工具链文件必须提供 `OE_QMAKE_PATH_EXTERNAL_HOST_BINS`（Yocto SDK 的 `Qt5Config.cmake` 靠它定位宿主 moc/uic/rcc）；架构 flag 需同时 `add_compile_options` **和** `add_link_options`，否则产物变软浮点（`ld-linux.so.3` 而非 `ld-linux-armhf.so.3`）。

**新增 `.cpp/.h` 必须加进 `CMakeLists.txt` 的 `add_executable(photo-album ...)` 列表**（头文件也要列，AUTOMOC 才能扫到 `Q_OBJECT`）。

### 部署与运行

```sh
scp qt/.dist/photo-album qt/qt_env.sh root@<板卡IP>:/root/
scp <图片> root@<板卡IP>:/root/photos/
```

板端：

```sh
cd /root && chmod +x photo-album qt_env.sh
. ./qt_env.sh
./photo-album /root/photos
```

**LinuxFB 没有窗口合成器**：`psplash` / `systemui` 等任何在写 `/dev/fb0` 的进程会和本程序互相覆盖画面并同时抢触摸。启动前必须查杀：

```sh
for p in /proc/[0-9]*; do ls -l "$p/fd" 2>/dev/null | grep -q '/dev/fb0' && echo "${p##*/} $(tr '\0' ' ' < "$p/cmdline")"; done
```

## Windows 侧服务链

`qt/windows/run_all.ps1` 是标准启动器（会先 `Stop-Process` 掉残留的 ffmpeg/mediamtx/python，避免旧进程占着 8888/1935 端口跑老配置），一次拉起五件事：

| 进程 | 端口 | 作用 |
|---|---|---|
| `mediamtx.exe` | 8554 RTSP / 1935 RTMP / **8888 HLS** | 转封装、出 HLS/RTSP/RTMP。**不转码** |
| `ffmpeg` × N 路 | — | 拉摄像头 HEVC → H.264 `scale=480:270` → 推 `rtmp://127.0.0.1:1935/live/cam<N>` |
| `stream_server.py` | 8000 | 静态文件 + Range、`GET /list` 录像列表、`DELETE` 删 `record_*.mp4` |
| `windows/snapshot.py` | — | 每 3s 写 `snap/cam<N>.jpg`，供板卡九宫格静态快照 |
| `windows/grid_mjpeg.py` | 8010 | 从本机 mediamtx RTSP 拉流压 JPEG，以 MJPEG 流提供 `/cam<N>` |

摄像头的 HEVC 是硬约束：`mediamtx` 不转码，所以只有两条路——改摄像头子码流为 H.264（形态 A，单进程最省事），或 ffmpeg 先转再推（形态 B，不动摄像头配置）。详见 `qt/MEDIAMTX_DEPLOY.md`。

摄像头凭据不写死在仓库里：`run_all.ps1` 读 `$env:CAMPASS`，`.bat` 里是 `CHANGE_ME` 占位符。摄像头 RTSP 路径形如 `/Streaming/Channels/{通道}02`（`02` = 子码流）。

`stream_server.py` 的 `ROOT = 自己所在目录`，而快照/录像都写在 PhotoAlbum 根目录 → **它必须从 `apps/PhotoAlbum/` 根启动**，不能从 `qt/` 或 `qt/windows/` 启动。

## 架构

### 板端：页面栈

`MainWindow` 用 `QStackedWidget` 承载六个页面，`buildUi()` 统一构建：

```
homePage ──┬─→ thumbnailPage ─→ detailPage（裁剪/删除/另存为 + 上滑 filmstrip）
           ├─→ monitorPage（3x3 九宫格）─→ videoPage（单路实时，返回 monitorPage）
           └─→ videoListPage ─→ videoPage（本地文件/录像回放/网络流，返回 videoListPage）
```

`videoReturnPage` 记录 videoPage 的来路，`videoBack()` 据此返回——监控单路和视频列表共用同一个 `VideoPlayerWidget`，改返回逻辑必须兼顾两条路径。

### 板端：配置解析（改地址先看这里）

三个配置都在 `applicationDirPath()` → `QDir::currentPath()` 两个候选位置依次查找，找不到就回退到默认值或推算值：

| 文件 | 读取者 | 用途 / 回退 |
|---|---|---|
| `streams.txt` | `findStreams()` | 「视频」页的 `网络流:` 条目；`#` 与空行跳过 |
| `gridServer.txt` | `gridServerBase()` | 九宫格 MJPEG 基址，默认 `http://192.168.137.1:8010` |
| `recordServer.txt` | `streamServerBase()` | 快照 `/snap/cam<N>.jpg`、`/list`、`DELETE` 的基址 |

**隐式耦合（容易踩）**：`streams.txt` 里第一条 `http://` 的 origin 会被 `monitorLiveBase()` 和 `streamServerBase()` 当作基址复用。所以板卡 MEDIAMTX 部署时写入 `http://192.168.137.1:8888/live/index.m3u8` 后，点九宫格单路会自动请求 `http://192.168.137.1:8888/live/cam<N>/index.m3u8`——**要求 mediamtx 侧的流路径就叫 `live/cam<N>`**（`run_all.ps1` 就是这么推的）。改了推送路径名，单路点播就会 404。

板卡只能访问 `192.168.137.x`（Windows ICS）与公网，`192.168.114.x` 摄像头段是 Windows 侧直达的。

### 板端：照片浏览

- 图片目录来自命令行参数，缺省依次尝试 `./photos`、`/root/photos`；**目录为空时自动生成 15 张演示图**，所以"看不到照片"未必是 bug
- 大图走 `QFutureWatcher<QImage>` 异步解码；`onImageLoaded()` 用 `loadingPath == photoPaths.at(currentIndex)` 判断结果是否仍对应当前图——`loadGeneration` 字段只自增、从未被读取，别误以为它在防旧结果覆盖
- `thumbnailCache` / `imageCache`(`imageCacheOrder` 做淘汰) 两级缓存
- 编辑结果只留在内存 `editedImages`，`savePhoto()` 才落盘为 PNG
- `PhotoView` 同时处理 `QTouchEvent` 和鼠标事件——**因为 evdevtouch 可能把触摸翻译成单指指针事件**，只实现一套会漏手势
- 触摸调试：设环境变量 `PHOTO_ALBUM_TOUCH_DEBUG`（任意值）后，详情页顶部会多出「接触点/滑动」两个绿色标签，`PhotoView` 也会在 stderr 打印每次触摸事件与捏合缩放（`main_window.cpp` 与 `photo_view.cpp` 各自独立判断该变量）

### 板端：视频播放

`VideoDecodeThread`（QThread）解码 → `frameReady(QImage)` → `VideoPlayerWidget` 只保留**最新一帧**（解码跟不上就丢帧，保证直播实时性），音频解码后写 ALSA。

- 直播判定：路径含 `.m3u8` 或以 `rtsp://`/`rtmp://`/`rtmps://` 开头。直播不显示进度条/删除按钮
- RTSP 强制 `rtsp_transport=tcp`（穿 NAT）+ `stimeout=5000000`（5 秒）+ `max_delay=1000000`（1 秒）；断流每 3s 重连
- 文件模式按源 fps 剩余时间 sleep 以真实速度播放
- 新旧 ffmpeg API 用 `LIBAVCODEC_VERSION_MAJOR >= 58` 编译期切换（`decodeNew()` / `decodeOld()`）。板端 2.8 走旧 API，主机可能走新 API——**两条路径都要改**，别只改一条

## 关键陷阱

1. **`qt/` 是唯一权威源码。** `git ls-files apps/PhotoAlbum` 只跟踪 `qt/**`；`apps/PhotoAlbum/` 根下的 `stream_server.py`、`windows/`、`qt_env.sh`、`photo-album`、`record/`、`snap/`、`out.m3u8` 全是**未跟踪的运行/构建产物**，并且已经和 `qt/` 版本分叉了（根目录 `qt_env.sh` 还是过时的 `event0`，`qt/` 是正确的 `event1`；根目录 `stream_server.py` 缺 `os.walk` 递归和 `PermissionError` 重试）。**改代码只改 `qt/` 下的文件。**
2. **通道号散落三处，必须同步**：`run_all.ps1` 的 `$channels = @(1,2,3,4,5,6,8)`、`grid_mjpeg.py` 的 `CHANNELS = [1,2,3,4,5,6,8]`、`snapshot.py` 的 `cam1..cam9`。当前 snapshot 比另两个多试两路（NVR 并发连接数有限，`run_all.ps1` 每路之间还 `Start-Sleep 3` 错开）。改通道列表时三处一起改。
3. **`main_window.h` 里 `stackedWidget` 声明为 `QWidget *`**，实现里到处 `static_cast<QStackedWidget *>`。新增页面切换沿用这个既有写法，别单独改类型。
4. **改动公共接口前先 Grep 调用点**。`PhotoView` 的 signal、`VideoPlayerWidget` 的公开方法在 `main_window.cpp` 里被大量 lambda 捕获。
5. **软件解码上限 ≈ 480x270**。推流侧分辨率调高会直接卡死板卡，`run_all.ps1` 的 `scale=480:270` 和 `-g 15`（低延迟关键帧）不要随手改大。

## 相关文档

- `qt/README.md` — 功能清单、自测方法、板端运行、ToDo（当前进度以此为准）
- `qt/QT_WORKFLOW.md` — 交叉编译/传输/上板全流程与常见问题排障
- `qt/MEDIAMTX_DEPLOY.md` — mediamtx 两种部署形态、yml 配置详解、防火墙端口、排障表
- `qt/STREAMING_PROTOCOLS.md` — HLS/RTSP/RTMP/DASH/SRT/WebRTC 原理与选型（理论参考）
