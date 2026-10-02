： 

# Qt 触摸相册与监控

一个面向 i.MX6ULL 4.3 英寸 `480x272` 触摸屏部署的 Qt Widgets 多媒体应用，兼容 Qt 5.12.9。项目提供照片浏览与裁剪、监控画面转播、九路监控画面同时展示、录像/直播播放和音频输出能力。应用默认扫描 `/root/photos`，目录为空时自动生成 15 张演示图。

## 功能

- iOS 桌面首页：相册、监控、视频三个应用图标（仿手机桌面 UI，顶部实时时钟）
- 点「相册」进入照片网格，点「监控」进入九宫格监控页，点「视频」进入视频列表；各自页面可返回桌面
- 两级相册界面：缩略图网格主页 + 图片详情页
- 缩略图主页：网格式展示所有图片，点击进入详情页
- 详情页顶部栏：左侧返回按钮，中间显示图片尺寸和拍摄时间（无 EXIF 时用文件修改时间）
- 详情页底部固定操作栏：裁剪、删除、另存为、编辑（暂禁用）
- 裁剪模式下操作栏切换为应用 / 取消 / 复位
- 单指水平滑动切换图片
- 单指拖动平移放大后的图片
- 双指捏合缩放，范围 `1x` 到 `8x`
- 双击复位
- 从底部向上滑动：展开底部横向缩略图列表，点击可切换图片；下滑收起（动画约 220ms）
- 删除当前图片（`demo://` 演示图或磁盘文件），全部删空后自动重新生成 15 张演示图
- 监控九宫格：同时展示 9 路监控缩略画面，点击任一路可进入该通道详情画面
- 视频播放：缩略图主页点「视频」进入视频列表，点击播放 ffmpeg 解码（软解，建议 ≤640x360），支持暂停/继续、返回；可播本地文件、录像回放与**网络直播流**（RTSP/RTMP/HLS）
- 音频输出：视频或监控流存在音频轨时，通过 ALSA 输出到板卡声卡，支持界面静音/出声切换
- 深色 iOS 风格界面

## 视频

- 基于 ffmpeg 的软解播放器 `video_player.cpp`：解码线程输出 RGB 帧到 `VideoPlayerWidget`（"最新帧"模式，解码跟不上自动丢帧，保证实时性），音频轨解码后写入 ALSA。
- 视频目录：与图片同目录（`/root/photos` 里的 `.mp4/.mkv/.avi/.mov/.flv/.ts/.m4v/.webm`）；无视频时用 `demo_media/demo.mp4` 演示视频（放在程序可执行文件旁的 `demo_media/` 或 `media/` 下）。
- **网络直播流**：在 `streams.txt`（程序可执行文件旁）每行写一个流地址，支持 `rtsp://`、`rtmp://`、`rtmps://` 与 `.m3u8`(HLS)。RTSP 强制走 TCP（`rtsp_transport=tcp`，便于穿越 NAT），并设 5s socket 超时、1s 缓冲上限；断流自动每 3 秒重连。直播流按"最新帧"实时显示，解码跟不上自动丢帧。
- 帧率节流：文件模式按源 fps 的剩余时间睡眠（真实速度播放）；直播流数据到达即显示，天然实时。
- 运行依赖：板卡需有 `libavformat.so.57 / libavcodec.so.57 / libavutil.so.55 / libswscale.so.4`（NXP BSP 自带，验证：`ls /usr/lib/libav*`）。

### 直播流自测（推荐）

在 Windows（或局域网内机器，注意板卡只能访问 `192.168.137.x` 与公网）推一条 RTSP 直播流，板卡拉取：

```sh
# Windows 推流（板卡屏幕 480x270）：
ffmpeg -re -f lavfi -i testsrc=size=480x270:rate=25 -c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p -f rtsp -rtsp_transport tcp rtsp://192.168.137.1:8554/live
# 板卡 streams.txt 写入：
echo "rtsp://192.168.137.1:8554/live" > /root/streams.txt
```

公网 http 测试流（板卡 ffmpeg 无 TLS，勿用 https）：

```sh
echo "http://maitv-vod.lab.eyevinn.technology/VINN.mp4/600/600-.m3u8" > /root/streams.txt
```

### 真实直播源接入（已验证协议齐全）

板卡 ffmpeg 2.8 已确认编译入 `rtsp` / `rtmp` / `rtmpt` / `hls` 协议，真实直播流可直接拉取（模板见 `streams.txt`）：

- **局域网 IP 摄像头**：`rtsp://<摄像头IP>:554/...`（不同品牌路径不同，走 TCP，见代码 `rtsp_transport=tcp`）
- **RTMP 推流**：`rtmp://<服务器IP>:1935/live/stream`（OBS/ffmpeg 推到 nginx-rtmp/mediamtx 等）
- **HLS 直播**：`http://<IP>:<端口>/out.m3u8`（本项目 Windows 自测方案已验证：实时 ~22fps 与源同步）

> 说明：播放器显示视频画面，并在存在可解码音频轨时输出声音；板卡无 TLS，`https://` 打不开；真实直播延迟取决于源（RTSP 最低，HLS 约 2s）。

### 历史录像回放

- Windows 用 `stream_server.py`（本项目自带）替代 `python -m http.server`，额外提供 `GET /list`（录像列表）和 `DELETE`（删除录像）：

  ```powershell
  python stream_server.py 8000
  ```

  注意：脚本要与 `out.m3u8` / `record_*.mp4` 放在同一目录。
- Windows 录像（摄像头按 10 分钟切段存盘，文件名含拍摄开始时间）：

  ```powershell
  ffmpeg -f dshow -video_size 352x288 -framerate 15 -i 'video=USB2.0 HD UVC WebCam' -c:v libx264 -preset veryfast -pix_fmt yuv420p -f segment -segment_time 600 -strftime 1 record_%Y%m%d_%H%M%S.mp4
  ```
- 板卡「视频」页会列出 `录像: record_xxxx.mp4`，点击回放：

  - **进度条**（仅回放显示，可拖动跳转）
  - **画面左下角实时叠加拍摄时间**（文件名解析的拍摄开始时间 + 播放进度）
  - **删除**按钮直接删除该录像（走 HTTP DELETE）
- 直播流（`网络流:` 条目）不显示进度条/删除，行为不变。

## 演示图片资源

`demo_photos/` 目录内置 15 张 810x540 演示 PNG（demo01.png ~ demo15.png）。可作为项目资源，
或直接拷贝到板端 `/root/photos` 使用。图片目录为空时，程序仍会自动生成相同样式的 15 张演示图。

## 主机仿真

```sh
cd /tmp
mkdir -p photo-album-build
cd photo-album-build
qmake /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/photo_album.pro
make
./photo-album /home/cube/WorkSpace/iMX6Ull/ARM_Linux/apps/PhotoAlbum/qt/demo_photos
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
cat /proc/bus/input/devices在·
for event in /dev/input/event*; do
    evtest "$event" 2>/dev/null | grep -q 'ABS_MT_POSITION_X' && echo "$event"
done
```

当前板卡触摸屏为 `/dev/input/event1`。更换硬件或系统后，若板卡没有 `evtest`，以 `/proc/bus/input/devices` 中列出的 `Handlers` 为准。

# ToDo

* [X] 三个窗口跑通 → 写成 bat 一键启动 + 计划任务自启
* [X] 板卡「监控」页九宫格展示 9 路画面，点击任一路进入单路详情画面
* [X] 板卡「视频」页列出 1~N 路网络流，点击切换播放
* [ ] 板卡直接拉 mediamtx 的 RTSP（0.5~2s），直播默认走 RTSP，继续降低延迟
* [ ] 按时间/通道检索、缩略图预览、批量删除、保留策略（自动删 N 天前）
* [ ] 优化UI：桌面扩展：底部 dock、更多应用位、壁纸渐变；视频播放器：倍速播放、帧步进
* [ ] 增加看门狗逻辑以及自启动
* [ ] 降低延迟时间
