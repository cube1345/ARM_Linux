# 媒体全流程链路：图片 / 视频 / 音频 —— 从原始数据到屏幕显示与扬声器

> 目的：梳理 i.MX6ULL 智能桌面上图片、视频、音频三条链路"从源到出"的完整旅程，
> 含每条链路的处理线程、内存类型、同步/丢失策略。
> 依据：仓库 `apps/PhotoAlbum/qt/` 真实代码（main_window / photo_view / video_player / music_player）。
> 适用：简历项目讲述 / 面试问答（"一帧从 NVR 到屏幕经历了什么"）。

---

## 总览：三条链路的源头与宿处

| 链路 | 源                                         | 途径                                   | 宿                            |
| ---- | ------------------------------------------ | -------------------------------------- | ----------------------------- |
| 图片 | 本地相册文件（JPEG/PNG）或`demo://` 生成 | 解码线程池 → QImage → 缓存           | PhotoView 绘制到屏幕          |
| 视频 | 本地文件 / NVR RTSP / HLS / RTMP           | VideoDecodeThread → 软解 → QImage    | VideoPlayerWidget 按 fps 重绘 |
| 音频 | 本地 MP3 / WAV（音乐）或视频伴音           | AudioDecodeThread / 视频线程内 → ALSA | 扬声器                        |

共同原则：**源尽量在后台线程处理，UI 只收"结果对象"，且每类都走"源 → 内存 → 显示/播放器"三步。**

---

## 一、图片链路：文件 → QImage → 三级缓存 → 屏幕

### 场景 A：缩略图网格（多图并发小图）

```
相册目录
   │  QDir 扫描，得到 photoPaths 列表（多线程之外，主线程）
   ▼
thumbnailForPath(path, cellSize)        // main_window.cpp:2952
   │  ① 查 thumbnailCache（key = path@WxH）命中直接返回 → 免解码
   ▼
QImageReader reader(path)                // ② 解码源图（仅读头部拿尺寸）
   │  reader.size() 原图尺寸
   │  reader.setScaledSize(size*2)      // ③ 解码就直接缩到 2x 格大小
   ▼
reader.read() → QImage(source)          // ④ 一次解码即得缩略图（不载全图）
   │  source.scaled(size, KeepAspectRatioByExpanding)  // ⑤ 二次缩放到目标格
   │  .copy(居中裁剪)                    // ⑥ 切出正好 cell 大小的中心块
   ▼
thumbnailCache.insert(key, thumb)       // ⑦ 写入缩略图缓存（上限 96 张）
   ▼
主线程 → ThumbnailButton 绘制 → 屏幕
```

**关键点**：

- 缩略图是**同步**做的（网格打开时逐张生成），但用了 `setScaledSize`（imglib 解码即缩放），**从不把整张原图载入内存**。
- 第三步用二级缓存 `thumbnailCache`，同尺寸再打开秒出。

### 场景 B：详情页大图（异步 + 三级缓存）

```
用户点击缩略图 → showDetail(currentIndex)
   ▼
loadingPath = photoPaths.at(currentIndex)
   │  loadWatcher->setFuture( QtConcurrent::run(&MainWindow::decodeImage, path) )
   │        ↑ 后台线程池线程：QImageReader.read() 解出完整 QImage（main_window.cpp:2829）
   ▼
主线程 onImageLoaded()（loadWatcher finished 信号触发，main_window.cpp:2837）
   │  image = loadWatcher->result()
   │  cacheImage(path, image)      // 一级缓存 imageCache（大图缓存）
   ▼
photoView->setImage(image)          // 交给 PhotoView
   ▼
PhotoView::paintEvent → imageRectForScale 按缩放矩阵 drawImage → 屏幕
```

**关键点**：

- 大图解码放 **QtConcurrent 后台线程**，不卡 UI；`QFutureWatcher<QImage>` 完成回主线程。
- `imageForPath()` 三者优先：**editedImages（编辑结果）> imageCache（大图缓存）> demo:// 内置**。
- 缩放/平移纯在主线程（PhotoView 存 offset/scale，paint 时变换），只对已在内存的 QImage 操作。

### 图片链路内存流小结

```
磁盘文件 ──QImageReader──► QImage(RGB) ──scaled/copy──► 缩略图QImage
                                          └──► 大图QImage ──Cache──► 复用
                                          └──► PhotoView offset+scale ──paint──► framebuffer
```

隐式共享贯穿始终（QImage 拷贝近乎零成本，直到 paint 才真正各取所需）。

---

## 二、视频链路：源 → VideoDecodeThread 软解 → 最新帧 → 屏幕

### 完整旅程（以 NVR RTSP 为例）

```
相机子码流 HEVC 640×360@25fps RTSP（192.168.114.48 / Streaming/Channels/NN02）
   │  ← Windows 侧 FFmpeg 转码（scale=480:270 → RTMP → MediaMTX → HLS/RTSP）
   ▼
板端 URL 判为直播 → VideoDecodeThread::open(path) → start()（video_player.cpp:121）
   ▼
VideoDecodeThread::run() → decodeNew()/decodeOld()   // 视频线程（②）
   │  avformat_open_input + find_stream_info      // 打开源，找视频/音频流
   │  avcodec_open2 打开解码器
   ▼
解码主循环（video_player.cpp:350 起）：
   │  处理等待中的 seek 请求（QAtomicInt pendingSeek）
   │  av_read_frame(format, packet)         // ⚠阻塞：等网络/文件数据
   │     ├─ 视频包 → avcodec_send_packet → avcodec_receive_frame → frame
   │     │     → sws_scale 转 RGB888 → 封装成 QImage(RGB888, 借用rgbBuffer内存在)
   │     │     → emit frameReady(image.copy())   // ⚠关键：copy() 隐式共享+深拷贝
   │     └─ 音频包 → avcodec_send_packet(audioCtx) → receive_frame(aframe)
   │           → writeAudioFrames(aframe)  // 转 S16 → snd_pcm_writei(ALSA)
   ▼（主线程，queued 信号槽）
VideoPlayerWidget::onFrame(frame)（video_player.cpp:622）
   │  QMutexLocker(frameMutex); currentFrame = frame;   // 只留最新一帧
   │  ++fpsFrames（统计）
   ▼
refreshTimer(30ms, PreciseTimer) → update() → paintEvent
   │  QMutexLocker(frameMutex); 拷贝 currentFrame → drawImage  → 屏幕
   ▼
（文件模式）按源 fps QThread::msleep 节流 │ （直播）latest-frame 丢帧不节流
```

### 视频链路的关键设计点

| 点                          | 说明                                                                                                               |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **latest-frame 丢帧** | `onFrame` 直接覆盖 `currentFrame`，不排队。解码跟不上就丢，保证缓冲只有一帧深 → 直播低延迟、不积压            |
| **跨线程发帧**        | `frameReady(QImage)` 用 queued 信号槽；QImage 隐式共享，发出时深拷贝，解码线程/UI 各持一份，天然免锁读           |
| **绘制时再锁一次**    | `paintEvent` 用 `frameMutex` 取帧，防止 UI 画到一半线程换帧 → 半张图                                          |
| **seek 零阻塞**       | UI`seek()` 只写 `QAtomicInt pendingSeek+pendingSeekMs`，解码线程循环里顺路 `av_seek_frame`，不重启线程       |
| **停止原子化**        | `stop()` set `stopped/running` 原子位 → 下一次循环退出 → emit `ended("__stopped__")`；析构 `wait()` 回收 |
| **音视频同线程**      | 视频伴音写 ALSA 也在 VideoDecodeThread 里（writeAudioFrames），与视频同源同步，天然对齐                            |

### 视频内存流小结

```
RTSP/文件 ──av_read_frame──► AVPacket ──send/receive──► AVFrame(解码后YUV)
   ──sws_scale──► RGB888(QImage) ──copy──► frameReady 信号
   ──► currentFrame(主线程, QMutex) ──paint取走──► framebuffer
   └─音: AVFrame ──S16 转换──► snd_pcm_writei(ALSA) ──► 扬声器
```

---

## 三、音频链路：文件/WAV → AudioDecodeThread → ALSA → 扬声器

### 场景 A：音乐（WAV 直放，主推路径）

```
本地 WAV（已离线预解码成 PCM，44.1kHz/16bit/双声道）
   ▼
AudioDecodeThread::play(path) → start()（music_player.cpp:61）
   ▼
run() 判断 .wav → runWav()（music_player.cpp:89 / 196）
   │  解析 RIFF/WAVE 头，取 fmt(rate/channels/bits) + data 段大小
   │  循环：读一缓冲块 → snd_pcm_writei(pcm, buf, frames)   // 零实时解码
   ▼ 音量：每帧重读 m_volume(QAtomicInt)，平滑渐变(±0.02)防爆音
   ▼ 播放完 → emit playbackFinished()（主线程收到 → 自动切歌/停止）
   └─ 切歌异步排队：旧曲播完才启动新曲，不阻塞 UI
```

**关键点**：音乐 WAV 是**零实时解码**——离线已转 PCM，板端只解析头部 + 写声卡，单核 CPU 占用极低。这是"预解码换 CPU 空闲"的典型。

### 场景 B：音乐（MP3/其他，遗留解码路径）

```
MP3 → run() 走 avformat 解码（music_player.cpp:94 起）
   avformat_open_input → avformat_find_stream_info → av_find_best_stream(AUDIO)
   → avcodec_open2 → av_read_frame → 解码 → S16 转换 → snd_pcm_writei
```

（CPU 占用明显更高，故默认音频文件已预转 WAV 走场景 A。）

### 场景 C：视频伴音

视频链路里已经讲过：**复用 VideoDecodeThread**，音频包在解码循环里被 `writeAudioFrames()` 直接消费写 ALSA——不另起线程，靠同一线程的 PTS 对齐保证 A/V 同步。ALSA underrun / Broken pipe 时 `snd_pcm_prepare` 恢复重写。

### 音频链路小结

```
WAV ──解析头──► 原样缓冲 ──snd_pcm_writei──► ALSA ──► 扬声器
MP3 ──av_read_frame──► 解码 ──S16──► snd_pcm_writei──► ALSA
音量: m_volume(QAtomicInt) ──每帧平滑渐变──► 写声卡前应用
```

---

## 四、三条链路的共性骨架（一句话可讲）

> **源在后台线程 → 解码成 QImage(图片/视频)或 S16(音频) → 通过队列信号/原子量交回主线程 → 主线程只做"拿最新结果绘制/写卡"。**

| 共性      | 图片                    | 视频                       | 音频                                |
| --------- | ----------------------- | -------------------------- | ----------------------------------- |
| 后台线程  | QtConcurrent 线程池     | VideoDecodeThread          | AudioDecodeThread（或并入视频线程） |
| 内存对象  | QImage                  | QImage(currentFrame)       | S16 缓冲                            |
| 交回方式  | QFutureWatcher finished | frameReady 信号            | playbackFinished 信号               |
| 跨线程锁  | 无（不共享像素）        | frameMutex 保护绘制读      | 无（ALSA 线程内写）                 |
| 丢/节流   | 缓存命中免解码          | latest-frame 丢帧          | 平滑渐变、坏曲自动停                |
| 显示/发声 | paintEvent 缩放矩阵     | 30ms refreshTimer update() | snd_pcm_writei 阻塞写卡             |

---

## 五、面试问答准备

**Q1：一帧 NVR 视频从源到屏幕经历了什么？**
相机 HEVC → （Windows FFmpeg 转 H.264 480×270 → MediaMTX 分发）→ 板端 `av_read_frame` → `avcodec_send/receive` 解码 → `sws_scale` 转 RGB→ QImage → `frameReady` 信号 → 主线程 `onFrame` 覆盖 `currentFrame`（QMutex）→ 30ms `refreshTimer update()` → paintEvent 绘制到 fb。途中 `latest-frame` 丢帧保证直播实时性。

**Q2：怎么处理解码太快/太慢？**
太快（文件模式）：按源 fps `QThread::msleep` 节流；太慢（直播）：latest-frame 丢帧，缓冲只留最新一帧，绝不积压——这是"宁可丢帧、不可延迟"的取舍。

**Q3：为什么大图解码不卡 UI？**
大图走 QtConcurrent 后台线程 + QFutureWatcher 回主线程；缩略图用 `setScaledSize` 解码即缩放不载全图；加 imageCache/thumbnailCache 两级缓存复用。

**Q4：音视频同步怎么做？**
声音和画面在同一解码线程里处理（VideoDecodeThread），包序天然一致；音频播放进度作 Master Clock，视频按需丢帧匹配，解决 ALSA underrun/Broken pipe。

**Q5：图片/视频的内存管理？**
QImage 隐式共享 + 深拷贝（跨线程发帧时拷贝一份），绘制前端再拷贝；解码线程和 UI 线程各持独立副本，避免共享内存互踩；帧缓冲只有一帧深，内存占用恒定。

---

## 备注（与真实实现的对应关系）

- 图片：`main_window.cpp:2818/2829/2837/2874`（QtConcurrent 大图）、`2925 imageForPath`、`2945 decodeImage`、`2952 thumbnailForPath`；`photo_view.cpp:41 setImage / 108 paintEvent`
- 视频：`video_player.cpp:214 run`、`235 decodeNew / decodeOld`、`350 主循环`、`622 onFrame`、`642/670/750/788 frameMutex`
- 音频：`music_player.cpp:82 run`、`89 runWav`、`136 av_read_frame`、`197 runWav 解析 WAV`；伴音 `video_player.cpp:153 writeAudioFrames`
- Windows 转码源：`qt/windows/run_all.ps1`（scale=480:270 → RTMP → Mediamtx → HLS/RTSP）
- 媒体取流/流媒体协议选型：`qt/STREAMING_PROTOCOLS.md`
