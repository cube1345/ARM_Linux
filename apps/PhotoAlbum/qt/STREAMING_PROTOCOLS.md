# 流媒体协议完全手册（详版·含原理图解·工程实践）

> 面向 i.MX6ULL 板卡相册/视频应用（PhotoAlbum）的流媒体协议深度手册。
> 目标：把每一个协议讲透——历史、原理、字节级格式、时序、状态机、延迟构成、错误处理、
> 安全、与 ffmpeg 的对接、排障方法，最后落到本项目的选型与实现。

---

cd E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum
Remove-Item out.m3u8, out*.ts -Force -ErrorAction SilentlyContinue
ffmpeg -rtsp_transport tcp -i "rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/102" -vf scale=480:270 -c:v libx264 -preset veryfast -tune zerolatency -g 15 -keyint_min 15 -pix_fmt yuv420p -f hls -hls_time 1 -hls_list_size 4 -hls_flags delete_segments out.m3u8 -f segment -segment_time 60 -strftime 1 record_%Y%m%d_%H%M%S.mp4



ffmpeg -rtsp_transport tcp -i "rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/102" -t 5 -f null -


## 目录

- **第一部分 基础概念**
  - [1. 网络分层与媒体&#34;加壳&#34;](#1-网络分层与媒体加壳)
  - [2. 传输层：TCP 与 UDP 的流媒体行为](#2-传输层tcp-与-udp-的流媒体行为)
  - [3. 编码与容器基础](#3-编码与容器基础)
- **第二部分 传输环境工程**
  - [4. NAT、防火墙与 CDN](#4-nat防火墙与-cdn)
  - [5. 延迟工程：端到端延迟是怎么来的](#5-延迟工程端到端延迟是怎么来的)
- **第三部分 各协议完全原理**
  - [6. HLS 完全原理](#6-hls-完全原理)
  - [7. RTSP 完全原理](#7-rtsp-完全原理)
  - [8. RTMP 完全原理](#8-rtmp-完全原理)
  - [9. HTTP-FLV 完全原理](#9-http-flv-完全原理)
  - [10. DASH 完全原理](#10-dash-完全原理)
  - [11. SRT 完全原理](#11-srt-完全原理)
  - [12. WebRTC 完全原理](#12-webrtc-完全原理)
- **第四部分 工程实践**
  - [13. 抓包排障手册](#13-抓包排障手册)
  - [14. 每协议对应的 ffmpeg 命令](#14-每协议对应的-ffmpeg-命令)
- **第五部分 选型与实现**
  - [15. 综合对比与选型矩阵](#15-综合对比与选型矩阵)
  - [16. 本项目深度评估](#16-本项目深度评估)
  - [17. 本项目实现与演进](#17-本项目实现与演进)
  - [18. 术语表](#18-术语表)

---

# 第一部分 基础概念

## 1. 网络分层与媒体"加壳"

### 1.1 五层模型中的流媒体

```
┌────────────────────────────────────────────────────────────────┐
│ 应用层   HTTP / RTSP / RTMP / HLS / DASH / SRT / WebRTC 等      │
├────────────────────────────────────────────────────────────────┤
│ 表示层   H.264/H.265/AAC 编码 → MPEG-TS/MP4/FLV/RTP 封装         │
├────────────────────────────────────────────────────────────────┤
│ 传输层   TCP（可靠、有序、流控） │ UDP（不可靠、低延迟）            │
├────────────────────────────────────────────────────────────────┤
│ 网络层   IP、路由、NAT                                          │
├────────────────────────────────────────────────────────────────┤
│ 链路/物理  以太网、Wi-Fi、4G/5G                                  │
└────────────────────────────────────────────────────────────────┘
```

### 1.2 "加壳"过程（同一码流，多种壳）

```
摄像头帧 ──H.264编码──► 裸码流(NAL) ──封装──► 容器 ──传输协议──► 网络字节流
 (YUV)      (压缩成SPS/PPS+帧)      (TS包|FLVtag|RTP包|MP4)  (TCP段|UDP包)

同一个 H.264 码流可以：
  装进 MPEG-TS  → 用 HLS 或 TS over UDP 传输
  装进 FLV     → 用 RTMP 或 HTTP-FLV 传输
  装进 RTP     → 用 RTSP/SIP/WebRTC 传输
  装进 MP4     → 用 DASH/渐进式下载传输
```

**含义**：协议选型与"编码"解耦。本项目编码固定 H.264，换协议只是换"壳"和"搬运方式"。

### 1.3 容器格式对照

| 容器     | 适用协议                | 关键结构            | 特点             |
| -------- | ----------------------- | ------------------- | ---------------- |
| MPEG-TS  | HLS、UDP 组播           | 188B 固定包         | 抗丢包，可拼接   |
| fMP4/MP4 | HLS(fMP4)、DASH、渐进式 | ftyp/moov/moof/mdat | 现代、可随机访问 |
| FLV      | RTMP、HTTP-FLV          | tag 流              | 简单，国内直播   |
| RTP 载荷 | RTSP、WebRTC、SIP       | 12B 头+载荷         | 实时、带时间戳   |
| MKV/WebM | 文件、Web               | EBML                | 开源             |

---

## 2. 传输层：TCP 与 UDP 的流媒体行为

### 2.1 TCP 如何影响流媒体

```
发送方                      接收方
 │ 应用写数据 ──► 发送缓冲区 ──► 网络
 │                              │
 │  ◄── ACK（确认已收）          │
 │  发送窗口 = f(往返时间, 丢包)  │
 │                              │
 │  拥塞控制：慢启动/拥塞避免       │
 └──────────────────────────────┘
```

TCP 给流媒体的"服务"：

- **可靠**：丢包重传 → 视频不花屏，但可能"卡住等数据"
- **有序**：乱序重组 → 播放器不需要处理乱序
- **流控/拥塞**：按网络状况调速 → 但会导致**吞吐抖动**

TCP 给流媒体的"麻烦"：

- **队头阻塞（Head-of-Line）**：一个包丢了，后面的包全被挡住 → 延迟抖动的元凶
- **重传延迟**：丢包时重传的包到达晚 → 实时性变差
- **慢启动**：连接建立初期吞吐低（影响起播）

### 2.2 UDP 如何影响流媒体

- **不可靠**：丢包直接丢 → 需上层处理（FEC/重传/掩盖）
- **无队头阻塞**：包独立到达 → 延迟稳定
- **无拥塞控制**：自己实现（SRT 做了）

### 2.3 结论：选 TCP 还是 UDP

| 需求                      | 选择                     |
| ------------------------- | ------------------------ |
| 延迟敏感 + 可接受轻微丢包 | UDP（WebRTC/SRT/RTP）    |
| 强可靠 + 延迟容忍         | TCP（HLS/RTSP-TCP/RTMP） |
| 广穿透 + 简单             | HTTP/TCP（HLS/HTTP-FLV） |

本项目：**全部走 TCP**（HLS/HTTP/RTSP-TCP），可靠优先，延迟通过协议结构优化。

---

## 3. 编码与容器基础

### 3.1 H.264 码流结构（板卡解码对象）

```
Annex-B 字节流格式：
  00 00 01 [NAL头] [NAL载荷]    （StartCode 分隔每个 NAL）

NAL 头（1 字节）：
┌────────┬─────────┬──────────────┐
│ F(1bit)│ NRI(2)  │  nal_type(5)  │
└────────┴─────────┴──────────────┘

nal_type 关键取值：
  7  = SPS（序列参数集：分辨率/帧率/Profile）
  8  = PPS（图像参数集）
  5  = IDR（关键帧，解码器可从此开始）
  1  = 非关键帧（P/B 帧）
  9  = AUD（访问单元分隔符）
  6  = SEI（补充增强信息，可放时间戳等）
```

**解码器前置条件**：必须先拿到 SPS+PPS 才能解码。容器（TS/FLV/MP4）会在关键帧前带上，或通过 `extradata` 提供。

### 3.2 GOP 与关键帧

```
I  B P B P B P B ... I  B P B ...
└─────── GOP ───────┘ └──下一GOP──┘
```

| GOP 属性               | 影响                     |
| ---------------------- | ------------------------ |
| GOP 长 → 压缩率高     | 码率省                   |
| GOP 长 → 关键帧间隔大 | seek 慢、分片粗、延迟高  |
| GOP 短 → 关键帧密     | 分片细、延迟低、码率略高 |

**本项目**：`-g 15`（15fps 下 = 1 秒一个 I 帧）→ HLS 分片可到 1s。

### 3.3 YUV 与 RGB（板卡显示相关）

```
编码/传输用 YUV420（4:2:0）：亮度 4 + 色度 1/4 采样 → 省带宽
显示用 RGB24：3 字节/像素
转换：sws_scale
本项目板卡 SDK swscale 无 NEON → yuv420p→rgb24 走 C 慢路径
优化：自写 NEON（toolchain 带 -mfpu=neon）
```

### 3.4 音视频同步（时间戳体系）

```
容器/传输里每个媒体单元都带时间戳：
  TS：PES 里的 PTS/DTS（90kHz）
  RTP：RTP timestamp（如 H.264 用 90000Hz）
  FLV：tag 时间戳（毫秒）
  MP4：mvhd/moof 的 timescale
播放器按时间戳调度音视频渲染（不是按到达顺序）
```

---

# 第二部分 传输环境工程

## 4. NAT、防火墙与 CDN

### 4.1 NAT 对拉流/推流的影响

```
公网 ◄──────── NAT 设备（路由器/Windows ICS）────────► 内网
                                                     │
 出站连接（内网发起）→ NAT 记录映射 → 数据能回来 ✅
 入站连接（外网发起）→ NAT 不知道发给谁 → 被拒绝 ❌
```

| 场景                                    | NAT 影响        | 解决                   |
| --------------------------------------- | --------------- | ---------------------- |
| 板卡 → 公网服务器拉流（HTTP/RTSP-TCP） | 出站，无影响 ✅ | 直接可用               |
| 外网 → 板卡推流/控制                   | 入站，被拒 ❌   | 端口映射/反向代理/TURN |
| 板卡 → Windows（同一 NAT 内）          | 局域网直连 ✅   | 直接可用               |

本项目：板卡经 Windows ICS 出网，**所有拉流都是出站**，NAT 无影响；RTSP 用 TCP 交织避免 UDP 端口映射。

### 4.2 防火墙

- HTTP(S) 80/443 通常放行 → HLS/HTTP-FLV 最易穿透
- 自定义端口（554/1935）常被拦 → RTSP/RTMP 需放行
- 本项目 Windows 侧放行：8000（HTTP 服务器）、8554（如启用 RTSP）

### 4.3 CDN 如何缓存（为什么 HLS 最适合 CDN）

```
用户 ──► 边缘节点(缓存) ──► 源站
        URL 唯一 = 缓存键
        HLS: 每个分片 URL 带 MEDIA-SEQUENCE 序号 → 天然唯一 → 可缓存
        RTSP: 有状态会话，无法在边缘缓存（回源直连）
```

- HLS/DASH 分段文件 = CDN 的天然缓存单元
- RTSP/RTMP 是"流"，CDN 只能做转封装或代理，成本高

---

## 5. 延迟工程：端到端延迟是怎么来的

### 5.1 延迟分解模型

```
端到端(Glass-to-Glass) =
  采集延迟(摄像头/屏幕) +
  编码缓冲(编码器攒帧, zerolatency可压到~50ms) +
  打包/分段(容器等待完整单元) +
  网络传输(RTT×往返次数 + 排队/拥塞) +
  接收缓冲(播放器攒缓冲防抖动) +
  解码(CPU软解) +
  显示刷新
```

### 5.2 各协议延迟预算

| 协议     | 打包       | 接收缓冲       | 网络      | 典型端到端 |
| -------- | ---------- | -------------- | --------- | ---------- |
| HLS      | 1~3 分片   | 1~3 分片       | RTT×多次 | 2~30s      |
| RTSP/RTP | 无         | ~100~300ms    | RTT×1    | 0.5~2s     |
| RTMP     | 服务器缓冲 | ~200ms         | RTT×1    | 1~3s       |
| HTTP-FLV | 无         | ~200ms         | RTT×1    | 1~3s       |
| SRT      | 无         | 目标延迟(去抖) | 重传      | 0.2~1s     |
| WebRTC   | 无         | jitter buffer  | RTT×1    | <0.5s      |

### 5.3 延迟的三个敌人

1. **缓冲（buffer）**：所有协议都靠缓冲换流畅，缓冲越大延迟越高
2. **分片/打包粒度**：HLS 的分片时长直接决定延迟下限
3. **往返次数**：列表刷新、控制消息、重传都增加 RTT 次数

### 5.4 本项目压延迟实践（已验证）

```
关键：-g <帧率> -keyint_min <帧率>（否则 GOP 16.7s，分片被拉长到~17s，延迟 20s+）
实测：-g 15 + -hls_time 1 + -hls_list_size 4 → 端到端 ~2s（源 25fps，GOP=0.6s）
再压：-g 7 + -hls_time 0.5 + list 3 → 可到 ~1.5s
再低：换 RTSP → 0.5~1.5s
```

---

# 第三部分 各协议完全原理

## 6. HLS 完全原理

### 6.1 历史与标准

- 2009 由 Apple 提出，随 iOS 3.0 发布
- 2017 年成为 RFC 8216（标准）
- 2019 起 Apple 主推 fMP4 分片（CMAF 兼容）
- 现状：几乎所有点播/直播平台（YouTube/Netflix/抖音 H5）都在用

### 6.2 体系图

```
          编码器 + 切片器 (ffmpeg)
               │
               │ 切成分片 + 生成播放列表
               ▼
      ┌────────┼─────────┐
      ▼        ▼         ▼
   seg0.ts  seg1.ts  seg2.ts      (分片文件)
      └────────┼─────────┘
               ▼
           index.m3u8              (播放列表)
               │
      ┌────────┼─────────┐
      ▼        ▼         ▼
    CDN      HTTP服务器   CDN
      └────────┼─────────┘
               ▼
           播放器/板卡
       1) GET index.m3u8
       2) GET segN.ts
       3) 定期重新 GET index.m3u8
```

### 6.3 播放列表（m3u8）逐行详解

**Master（自适应多码率）**：

```
#EXTM3U                                    ← 固定头
#EXT-X-VERSION:3
#EXT-X-STREAM-INF:BANDWIDTH=246440,AVERAGE-BANDWIDTH=246440,
    CODECS="mp4a.40.5,avc1.42000d",RESOLUTION=320x184,FRAME-RATE=25
sub_320.m3u8                               ← 子列表URL
#EXT-X-STREAM-INF:BANDWIDTH=2149280,CODECS="mp4a.40.2,avc1.64001f",RESOLUTION=1280x720
sub_720.m3u8
```

**Media（单码率）**：

```
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-TARGETDURATION:1                     ← 分片最大时长（播放器据此设缓冲窗口）
#EXT-X-MEDIA-SEQUENCE:42                    ← 当前窗口第一片序号
#EXT-X-PROGRAM-DATE-TIME:2026-09-06T13:00:00.000Z   ← 第一片节目时间
#EXT-X-DISCONTINUITY                        ← (可选)时间戳不连续
#EXTINF:1.0,                                ← 分片时长
seg42.ts
#EXTINF:1.0,
seg43.ts
                                          ← 无 #EXT-X-ENDLIST = LIVE
```

### 6.4 EXT-X 标签速查表（RFC 8216 核心）

| 标签                                  | 作用                   | 归属      |
| ------------------------------------- | ---------------------- | --------- |
| `#EXTM3U`                           | 文件头                 | 必        |
| `#EXT-X-VERSION`                    | 版本号                 | 必        |
| `#EXT-X-TARGETDURATION`             | 最大分片时长秒数       | 必        |
| `#EXT-X-MEDIA-SEQUENCE`             | 窗口起始序号           | 直播关键  |
| `#EXT-X-ENDLIST`                    | 有=点播；无=直播       | 关键      |
| `#EXTINF`                           | 分片时长               | 必        |
| `#EXT-X-STREAM-INF`                 | master 子流描述        | master    |
| `#EXT-X-KEY`                        | 加密方式/URI           | 加密      |
| `#EXT-X-PROGRAM-DATE-TIME`          | 节目时间               | 时钟同步  |
| `#EXT-X-DISCONTINUITY`              | 时间不连续             | 广告等    |
| `#EXT-X-BYTERANGE`                  | 字节范围（单文件分片） | 优化      |
| `#EXT-X-PLAYLIST-TYPE`              | VOD/EVENT 标记         | 点播      |
| `#EXT-X-I-FRAMES-ONLY`              | 仅关键帧列表           | seek/预览 |
| `#EXT-X-MAP`                        | fMP4 init segment      | fMP4      |
| `#EXT-X-PART / PRELOAD-HINT / SKIP` | LL-HLS                 | LL-HLS    |

### 6.5 分片容器：MPEG-TS 字节级

```
TS 流：连续 188 字节的 TS 包
┌──────────────────────────────────────────────┐
│ TS包(188B)                                    │
│  包头(4B)：                                    │
│    sync_byte=0x47(1B)                          │
│    transport_error(1bit) PUSI(1bit) 优先级(1)  │
│    PID(13bit)                                 │
│    scrambling(2) adaptation(2) CC(4)          │
│  载荷：PSI（PAT/PMT）或 PES（音视频）            │
└──────────────────────────────────────────────┘

PID 约定：
  PID=0x0000 → PAT（节目关联表：列出 PMT 的 PID）
  PID=0x0011 → PMT（节目映射表：列出音视频 PID 与编码）
  其余 → PES 音视频（由 PMT 指出哪个 PID 是视频）

PES 载荷 → H.264 NAL（Annex-B）
关键帧(IDR) 分片 → 每个分片从 IDR 开始 → 可独立解码
```

### 6.6 fMP4 分片（新一代）

```
init.mp4: ftyp + moov（编解码参数、轨迹）
chunk.m4s: styp + moof + mdat（媒体数据）
播放器先拉 init.mp4，再按序拉 m4s
好处：与 MP4/DASH/CMAF 生态统一、省带宽、支持字节范围复用
```

### 6.7 播放器行为算法（本项目解码线程逻辑对应）

```
loop {
  // 打开/重载播放列表
  m3u8 = read(index.m3u8)
  // 跳过已播放分片，按 MEDIA-SEQUENCE 推进
  for seg in m3u8.segments where not played:
     data = http_get(seg.uri)
     decode_and_display(data)
  // 直播：等待 hls_reload 周期后重读列表
  if live: sleep(reload_interval)
}
```

本项目 ffmpeg 行为：

- `avformat_open_input` 解析 m3u8
- `av_read_frame` 内部：拉列表 → 拉分片 → 解 TS → 给解码器
- 直播断流（列表不再更新 / 分片 404）→ 返回错误 → 本项目 3 秒后重连

### 6.8 延迟来源逐项（可调项）

| 项         | 默认                   | 可调        |
| ---------- | ---------------------- | ----------- |
| 分片时长   | `-hls_time` 决定     | 0.5~6s      |
| 列表刷新   | hls_reload（~0.5~1s） | 播放器参数  |
| 起始缓冲   | 1~3 分片               | 播放器      |
| GOP/关键帧 | `-g` 决定            | `-g 帧率` |
| CDN 滞后   | —                     | 换节点      |

### 6.9 加密（DRM 简）

```
#EXT-X-KEY:METHOD=AES-128,URI="https://.../key",IV=0x...
分片载荷 = AES-128-CBC
播放器：先 GET key（需要鉴权），再解密分片
```

本项目：无加密（局域网），但代码已能处理 `#EXT-X-KEY`（若源带加密，ffmpeg 3.0 支持 AES-128）。

### 6.10 直播 vs 点播 vs EVENT

| 类型  | ENDLIST    | 行为                                                |
| ----- | ---------- | --------------------------------------------------- |
| LIVE  | 无         | 列表滚动，播放器持续拉新分片                        |
| VOD   | 有         | 固定列表，从头播到尾                                |
| EVENT | 无但有开始 | 从第 0 片开始播，播器从开头一直跟进（播不了"当前"） |

本项目 VINN 测试源是 VOD（有 ENDLIST）→ 播完一次后重连重播。

### 6.11 优缺点 / 适用总结

| 优点                          | 缺点                 |
| ----------------------------- | -------------------- |
| 纯 HTTP，穿透最强、CDN 友好   | 延迟下限 ~2s         |
| 自适应码率成熟（master 列表） | 分片文件多、管理成本 |
| 点播/直播/回放通吃            | 分片必须对齐关键帧   |
| 标准（RFC 8216）、生态最大    | seek 粒度 = 分片粒度 |

---

## 7. RTSP 完全原理

### 7.1 历史与标准

- 1998 由 RealNetworks/Netscape/Columbia 提出，RFC 2326
- 2016 更新为 RFC 7826
- 现状：IP 摄像头、NVR、监控平台事实标准

### 7.2 架构：控制面 + 媒体面

```
┌──────────────┐   RTSP(文本, TCP:554)   ┌──────────────┐
│              │ ◄───────────────────────► │              │
│   播放器      │                          │  服务器        │
│  (RTSP客户端) │   RTP(媒体, UDP或TCP)     │ (摄像头/mediamtx)│
│              │ ◄════════════════════════► │              │
└──────────────┘                          └──────────────┘
```

### 7.3 RTSP 方法（RFC 7826）

| 方法          | 方向        | 作用                             |
| ------------- | ----------- | -------------------------------- |
| OPTIONS       | C→S        | 查询支持的方法                   |
| DESCRIBE      | C→S        | 获取媒体描述（SDP）              |
| ANNOUNCE      | C→S / S→C | 推送/描述（录制/推流）           |
| SETUP         | C→S        | 建立传输通道（选 UDP/TCP、端口） |
| PLAY          | C→S        | 开始播放（可带 Range）           |
| PAUSE         | C→S        | 暂停                             |
| TEARDOWN      | C→S        | 终止会话                         |
| GET_PARAMETER | 双向        | 查询/保活（keepalive）           |
| SET_PARAMETER | 双向        | 设置参数                         |
| REDIRECT      | S→C        | 重定向                           |
| RECORD        | C→S        | 录制（需 ANNOUNCE）              |

### 7.4 响应码（类 HTTP）

```
200 OK / 401 Unauthorized / 404 Not Found
453 Not Enough Bandwidth / 454 Session Not Found
455 Method Not Valid in This State / 456 Header Field Not Valid
```

### 7.5 完整会话（含 SDP 全文）

```
C→S:  OPTIONS rtsp://192.168.137.1:8554/live RTSP/1.0
      CSeq: 1
S→C:  RTSP/1.0 200 OK
      CSeq: 1
      Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER

C→S:  DESCRIBE rtsp://192.168.137.1:8554/live RTSP/1.0
      CSeq: 2
S→C:  RTSP/1.0 200 OK
      CSeq: 2
      Content-Type: application/sdp
      Content-Base: rtsp://192.168.137.1:8554/live/
      Content-Length: 234

      v=0
      o=- 1736349010 1736349011 IN IP4 192.168.137.1
      s=live
      t=0 0
      c=IN IP4 0.0.0.0
      a=tool:mediamtx
      a=control:*
      m=video 0 RTP/AVP 96
      a=rtpmap:96 H264/90000
      a=fmtp:96 packetization-mode=1;profile-level-id=42001F
      a=control:trackID=0
      m=audio 0 RTP/AVP 97
      a=rtpmap:97 MPEG4-GENERIC/44100/2
      a=fmtp:97 streamtype=5;profile-level-id=1;mode=AAC-hbr;config=1210
      a=control:trackID=1

C→S:  SETUP rtsp://192.168.137.1:8554/live/trackID=0 RTSP/1.0
      CSeq: 3
      Transport: RTP/AVP/TCP;unicast;interleaved=0-1
S→C:  RTSP/1.0 200 OK
      CSeq: 3
      Session: 6pI5OvKlZohvB0K2
      Transport: RTP/AVP/TCP;unicast;interleaved=0-1

C→S:  PLAY rtsp://192.168.137.1:8554/live RTSP/1.0
      CSeq: 4
      Session: 6pI5OvKlZohvB0K2
S→C:  RTSP/1.0 200 OK
      CSeq: 4
      Session: 6pI5OvKlZohvB0K2
      RTP-Info: url=rtsp://.../trackID=0;seq=1234;rtptime=567890

      ════ 此后媒体在 interleaved 通道 0-1 上流动 ════

C→S:  TEARDOWN rtsp://.../live RTSP/1.0
      CSeq: 5
      Session: 6pI5OvKlZohvB0K2
```

### 7.6 SDP 字段解释

| 字段                       | 含义                              |
| -------------------------- | --------------------------------- |
| `m=video 0 RTP/AVP 96`   | 视频轨，RTP，payload 96           |
| `a=rtpmap:96 H264/90000` | 载荷类型映射：H.264，时钟 90000Hz |
| `a=fmtp`                 | 参数：分包模式、profile、AAC 配置 |
| `a=control`              | 该轨的控制 URL                    |

### 7.7 RTP 包头位图（12 字节）

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X| CC    |M|     PT      |       sequence number        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            contributing source (CSRC) identifiers            |
|                             ....                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 7.8 H.264 RTP 打包（RFC 6184）

三种模式：

```
1) Single NAL（小帧，一个 RTP 包）：
   ┌──────────┬──────────┐
   │ RTP头(12)│ NAL(1+载荷)│
   └──────────┴──────────┘

2) Aggregation Packet / STAP-A（多小 NAL 合并）：
   ┌──────────┬─────────┬─────────┬───────┐
   │ RTP头(12)│ NAL头(载荷=24)│ len+N │ ...   │
   └──────────┴─────────┴─────────┴───────┘

3) Fragmentation Unit / FU-A（大帧分包）：
   ┌──────────┬─────────┬──────────┐
   │ RTP头(12)│ FU头(1B) │ FU载荷    │
   └──────────┴─────────┴──────────┘
   FU头: S(起始) E(结束) R(0) TYPE(=28)
   播放器用 S/E 重组出完整 NAL
```

### 7.9 RTCP（伴随统计）

```
每 5 秒左右发一个 RTCP：
  SR（发送方报告）：发送者时间戳映射 + 包/字节计数
  RR（接收方报告）：丢包率、抖动、往返时间
用途：监控质量、抗丢包反馈、时间同步（RTP时间↔NTP）
```

### 7.10 TCP 交织（interleaved）帧格式（本项目用）

```
RTSP 控制连接上，每个 RTP 包前面加 4 字节帧头：
┌────────┬─────────┬─────────────┬──────────────────┐
│ 0x24($)│ channel │ length(2B)  │ RTP payload       │
└────────┴─────────┴─────────────┴──────────────────┘
通道：视频 0-1，音频 2-3（RTSP Transport: interleaved=0-1）
```

优点：一个 TCP 连接搞定控制+媒体，穿 NAT；缺点：TCP 丢包重传 → 弱网延迟抖动。

### 7.11 状态机

```
      DESCRIBE
  Init ──────► Ready ──PLAY──► Playing
                ▲                │
                │                │ PAUSE
                └────────────────▼
                     Paused（可再 PLAY）
任何状态 → TEARDOWN → 销毁
```

### 7.12 NAT 与保活

- UDP 模式：RTP 端口动态，需端口映射；保活用 RTCP 或 `GET_PARAMETER` keepalive
- TCP 模式：无端口映射问题；用 `GET_PARAMETER` 每 N 秒保活，防止 NAT/防火墙断连接
- 本项目：`rtsp_transport=tcp` + `stimeout=5s`（socket 超时）+ 3 秒重连

### 7.13 优缺点

| 优点                     | 缺点                   |
| ------------------------ | ---------------------- |
| 延迟 0.5~2s              | 有状态，服务器实现复杂 |
| PLAY/PAUSE/SEEK（Range） | UDP 模式 NAT 麻烦      |
| 摄像头/监控事实标准      | CDN 不友好             |
| 媒体流式，不冗余         | 播放器兼容面窄         |

---

## 8. RTMP 完全原理

### 8.1 历史

- Adobe（Macromedia）私有协议，2002 起用于 Flash 直播
- 2012 年公开规范，2017 Adobe 停止 Flash → 但 RTMP 仍是"推流到服务器"的主流
- 现状：OBS/推流 → nginx-rtmp/mediamtx 等，服务器再转 HLS/HTTP-FLV 分发

### 8.2 握手（精确到字节）

```
客户端                服务器
 C0 (1字节 = 0x03) ──►
 C1 (1536字节)     ──►   (前4B时间戳 + 4B随机 + 1528B随机)
 ◄── S0 (1字节)
 ◄── S1 (1536字节)
 ◄── S2 (1536字节, 回显C1)
 C2 (1536字节, 回显S1) ──►
 │ 握手完成后进入 AMF 命令阶段
```

### 8.3 Chunk 流（TCP 上的消息分块）

```
RTMP 把消息切成 chunk（默认 128B），每 chunk 带头：
基本头(1B)：fmt(2bit) csid(6bit)
  fmt=0: 完整头(12B: 时间戳3+长度3+类型1+流ID4)
  fmt=1: 时间戳增量3B
  fmt=2: 只时间戳增量小
  fmt=3: 完全继承上一个 chunk
消息跨多个 chunk：首 chunk fmt=0/1/2，后续 fmt=3
```

### 8.4 消息类型（AMF）

| 值 | 类型                   | 说明                         |
| -- | ---------------------- | ---------------------------- |
| 1  | Set Chunk Size         | 协商块大小                   |
| 2  | Abort                  | 中断流                       |
| 3  | Acknowledgement        | 收到字节确认                 |
| 4  | User Control           | 流开始/结束、Buffer Length   |
| 5  | Window Acknowledgement | 确认窗口                     |
| 6  | Set Peer Bandwidth     | 带宽协商                     |
| 8  | Audio                  | 音频数据                     |
| 9  | Video                  | 视频数据                     |
| 15 | AMF0 Data              | 元数据（onMetaData）         |
| 18 | AMF0 Command           | 命令（connect/publish/play） |
| 20 | AMF3 Command           | 同上(AMF3)                   |

### 8.5 AMF0 命令示例（connect）

```
connect 命令（AMF0 编码）：
  字符串 "connect"
  数字 事务ID
  null
  对象 { app:"live", tcUrl:"rtmp://host/live", ... }
服务器回 _result 或 _error
```

### 8.6 推流全流程

```
OBS                               nginx-rtmp / mediamtx
  │ connect                        │
  ├───────────────────────────────►│
  │◄───────────────────────────────│ _result(连接成功)
  │ createStream                   │
  ├───────────────────────────────►│
  │◄───────────────────────────────│ _result(streamId=1)
  │ publish("stream","live")       │
  ├───────────────────────────────►│
  │◄───────────────────────────────│ onFCPublish
  │◄───────────────────────────────│ onPublish
  │ Video(tag9)/Audio(tag8) ──────►│  持续推
  │◄───────────────────────────────│ 服务器转发给观众
  │ FCUnpublish / deleteStream     │
```

### 8.7 拉流全流程（播放器→服务器）

```
播放器  connect → _result
        createStream → _result(streamId)
        play("stream") → 服务器把流推给播放器
        Video/Audio tag 流到达
```

### 8.8 FLV tag 结构（媒体封装）

```
FLV 头（9B）：
  "FLV"(3B) 版本(1) flags(1: 有音频0x04/视频0x01) 头长(4B=9)

Tag：
┌──────────┬───────────┬───────────┬──────────────────────────┐
│ 类型(1B) │ 长度(3B)  │ 时间戳(4B) │ 数据                     │
│ 8=音频   │  payload  │ 毫秒      │ 视频: AVC header/AVC NALU │
│ 9=视频   │  长度      │           │ 音频: AAC raw/sequence   │
└──────────┴───────────┴───────────┴──────────────────────────┘
┌──────────────────────┐
│ PreviousTagSize(4B)  │  每 tag 后跟上一 tag 长度（用于校验/定位）
└──────────────────────┘
```

H.264 在 FLV 里的编码方式：AVCC（长度前缀），与 TS 的 Annex-B（StartCode）不同：

- `AVCDecoderConfigurationRecord`（SPS/PPS，在首帧前推送）
- 后续：4 字节长度 + NAL

### 8.9 元数据（onMetaData）

```
onMetaData: {
  width: 1280, height: 720,
  videocodecid: 7,   // H.264
  audiocodecid: 10,  // AAC
  framerate: 30, duration: 0
}
```

### 8.10 优缺点

| 优点                | 缺点                   |
| ------------------- | ---------------------- |
| 推流生态标准（OBS） | Adobe 停支持           |
| 延迟 1~3s           | 实现细节多             |
| TCP 可靠            | 无官方自适应码率       |
|                     | 长连接、服务器资源消耗 |

---

## 9. HTTP-FLV 完全原理

### 9.1 一句话与图

```
播放器 ──GET /live.flv HTTP/1.1──► 服务器
        ◄══ 200 OK + 响应体 = 连续 FLV tag ══
        （连接保持打开，服务器持续写 tag）
```

### 9.2 与 RTMP 的转换

- 推流端：RTMP（OBS 推给 nginx-rtmp）
- 分发端：nginx-rtmp 转 HTTP-FLV 输出 `/live.flv`（flv 模块）
- 播放器：fetch + flv.js demux → MSE 播放

### 9.3 MSE 缓冲

```
浏览器 MSE：
  sourceBuffer（fMP4 格式）
  flv.js: demux FLV → 封装成 fMP4 → appendBuffer
缓冲队列管理：过高则暂停拉取（控制延迟）
```

### 9.4 优缺点

- 优点：穿透好、延迟低（1~3s）、实现简单
- 缺点：无 seek、长连接、仅 FLV 源、浏览器需 flv.js

---

## 10. DASH 完全原理

### 10.1 标准

- MPEG-DASH，ISO/IEC 23009-1
- 与 HLS 同理念：HTTP 分段 + 清单
- 清单用 MPD（XML）；分段用 ISO-BMFF（MP4）

### 10.2 MPD 详解

```xml
<MPD type="static" mediaPresentationDuration="PT1H"
     minBufferTime="PT4S" profiles="urn:mpeg:dash:profile:isoff-live:2011">
  <Period id="p0" start="PT0S">
    <AdaptationSet mimeType="video/mp4" contentType="video" segmentAlignment="true">
      <Representation id="v0" bandwidth="250000" width="320" height="184" codecs="avc1.42001e">
        <SegmentTemplate timescale="1000" duration="2000"
                         media="seg_$Number$.m4s" initialization="init.mp4"/>
      </Representation>
      <Representation id="v1" bandwidth="1500000" width="1280" height="720">...</Representation>
    </AdaptationSet>
    <AdaptationSet mimeType="audio/mp4">...</AdaptationSet>
  </Period>
</MPD>
```

### 10.3 分段

```
init.mp4（ftyp+moov，编解码参数）
seg_1.m4s（moof+mdat，2s 媒体）
```

### 10.4 优缺点

- 优点：行业标准、MP4 生态、多语言/字幕/DRM 描述强
- 缺点：生态/兼容不如 HLS（移动端尤其）、延迟同 HLS
- 本项目：ffmpeg 3.0 支持 DASH 解复用，但无必要（HLS 已够）

---

## 11. SRT 完全原理

### 11.1 标准

- Haivision 开源，2017 起流行；OBS/SRT 服务器支持
- 用途：弱网/公网"可靠传输"（采集端→服务器）

### 11.2 原理（UDP + ARQ + 拥塞控制）

```
发送方                                   接收方
 数据包(带序号) ──► UDP ──►               │
                                        │ 检测序号缺口
 ◄── NAK（通知丢失包号）                    │
 重传缺失包 ──►                           │
                                        │ 定期 ACK（汇总，含RTT/丢包）
 ┌──────────────────────────────────────┐
 │ 拥塞控制：按 RTT/丢包调节发送速率        │
 │ 目标延迟（latency）：接收端去抖缓冲       │
 └──────────────────────────────────────┘
```

### 11.3 包格式

```
SRT 包头（16B 加密头 + 16B 数据头）：
  TYPE（数据/ACK/NAK/握手/...）、序号、时间戳、目的套接字ID
载荷 = 封装好的 MPEG-TS 或其他媒体
```

### 11.4 模式

| 模式       | 说明                  |
| ---------- | --------------------- |
| caller     | 主动连接对端（拉/推） |
| listener   | 监听                  |
| rendezvous | 双方同时连（P2P）     |

### 11.5 优缺点

- 优点：弱网稳定、0.2~1s、加密
- 缺点：播放端生态少、需专门服务器
- 本项目：ffmpeg 3.0 未编译 SRT → 不可用

---

## 12. WebRTC 完全原理

### 12.1 标准与场景

- W3C + IETF 标准；浏览器实时音视频
- 场景：视频通话、低延迟直播、云游戏

### 12.2 架构

```
A ─── 信令(WebSocket/HTTP: SDP offer/answer + ICE) ───► B
A ◄═══ RTP/SRTP (UDP, P2P 尽力直连) ═══► B
        ▲
   ICE: STUN(探测公网映射地址) / TURN(中继兜底)
```

### 12.3 建立流程

```
1) 双方收集 ICE candidate：
   本机地址 / STUN 探测的公网地址 / TURN 中继地址
2) 交换 SDP（含媒体 + codec + candidate）
3) DTLS-SRTP 握手（协商媒体加密密钥）
4) ICE 连通性检查 → 选最优路径
5) 开始 RTP/SRTP 双向传输
```

### 12.4 SDP offer 片段

```
v=0
o=- 1234 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
m=video 9 UDP/TLS/RTP/SAVPF 96 97
a=rtpmap:96 VP8/90000
a=rtpmap:97 H264/90000
a=ice-ufrag:abc  a=ice-pwd:xyz
a=candidate:1 1 udp 2113937151 192.168.137.1 51000 typ host
```

### 12.5 关键组件

| 组件          | 作用                       |
| ------------- | -------------------------- |
| ICE           | 候选地址收集与路径选择     |
| STUN          | 探测公网地址（NAT 打洞）   |
| TURN          | 中继兜底（双向打洞失败时） |
| DTLS-SRTP     | 媒体加密                   |
| RTP/RTCP      | 媒体传输 + 质量反馈        |
| jitter buffer | 接收端去抖                 |

### 12.6 优缺点

- 优点：延迟最低（<0.5s）、加密、自适应码率（拥塞控制）
- 缺点：需要信令+STUN/TURN、播放端生态有限（浏览器为主）、实现复杂
- 本项目：板卡 ffmpeg 3.0 不支持 → 排除

---

# 第四部分 工程实践

## 13. 抓包排障手册

### 13.1 工具与基本过滤器

```bash
# tcpdump（Linux/板卡）
tcpdump -i eth0 -A -s 0 port 8000      # HTTP 文本
tcpdump -i eth0 -X -s 0 'tcp port 554' # RTSP/交织媒体（hex）
tcpdump -i eth0 'udp portrange 5000-5010'  # RTP/UDP

# Wireshark（Windows/PC）
显示过滤器：
  http.request.uri contains "m3u8"
  rtsp
  rtp
  rtmp
```

### 13.2 各协议常见故障与排查

| 症状          | 可能原因                 | 查什么                                     |
| ------------- | ------------------------ | ------------------------------------------ |
| HLS 404 循环  | 服务器没有该分片/列表    | 看 m3u8 的 MEDIA-SEQUENCE 与文件是否对得上 |
| HLS 高延迟    | GOP 太长（分片 17s）     | 检查`-g`；看分片时长                     |
| RTSP 连不上   | 防火墙/服务器没监听      | `netstat -ano\|findstr 554`；`nc -vz`   |
| RTSP UDP 花屏 | UDP 丢包                 | 改`rtsp_transport=tcp`                   |
| RTMP 推流失败 | 服务器没起/stream key 错 | 服务器日志；`ffprobe rtmp://...`         |
| 画面起播慢    | 播放器缓冲大             | 减`-hls_list_size`/缓冲参数              |
| 卡顿          | 带宽不足/服务器缓冲      | 看码率 vs 带宽                             |

### 13.3 板卡上验证网络/流的命令

```sh
curl -s http://192.168.137.1:8000/out.m3u8          # 看列表
curl -s -r 0-99 http://.../record_x.mp4 -o /dev/null -w "%{http_code}"   # Range
nc -vz 192.168.137.1 8000                            # 端口
```

---

## 14. 每协议对应的 ffmpeg 命令

### 14.1 HLS

```bash
# 生成 HLS（本项目直播）
ffmpeg -re -f dshow -i 'video=USB2.0 HD UVC WebCam' \
  -c:v libx264 -preset veryfast -tune zerolatency -g 15 -keyint_min 15 -pix_fmt yuv420p \
  -f hls -hls_time 1 -hls_list_size 4 -hls_flags delete_segments out.m3u8

# 播放 HLS
ffplay http://192.168.137.1:8000/out.m3u8
```

### 14.2 RTSP

```bash
# 推流到 RTSP 服务器（mediamtx）
ffmpeg -re -f dshow -i 'video=...' -c:v libx264 -preset veryfast -tune zerolatency -g 15 \
  -f rtsp -rtsp_transport tcp rtsp://127.0.0.1:8554/live

# 拉流（TCP）
ffplay -rtsp_transport tcp rtsp://192.168.137.1:8554/live
```

### 14.3 RTMP

```bash
# 推流（OBS/ffmpeg → nginx-rtmp）
ffmpeg -re -i in.mp4 -c:v copy -f flv rtmp://server/live/stream

# 拉流
ffplay rtmp://server/live/stream
```

### 14.4 HTTP-FLV

```bash
ffplay http://server/live.flv
```

### 14.5 转封装（协议互转，用于本项目多源）

```bash
# RTMP → HLS（服务器端常见）
ffmpeg -i rtmp://in/live -c copy -f hls -hls_time 1 out.m3u8

# RTSP → HLS
ffmpeg -rtsp_transport tcp -i rtsp://cam/... -c copy -f hls out.m3u8
```

---

# 第五部分 选型与实现

## 15. 综合对比与选型矩阵

| 维度          | HLS      | RTSP            | RTMP   | HTTP-FLV | DASH     | SRT    | WebRTC     |
| ------------- | -------- | --------------- | ------ | -------- | -------- | ------ | ---------- |
| 传输          | HTTP/TCP | RTP(TCP/UDP)    | TCP    | HTTP/TCP | HTTP/TCP | UDP    | UDP        |
| 延迟          | 2~30s    | 0.5~2s          | 1~3s   | 1~3s     | 2~30s    | 0.2~1s | <0.5s      |
| 有状态        | 无       | 有              | 有     | 无       | 无       | 有     | 有         |
| 播放控制      | 播放器   | PLAY/PAUSE/SEEK | 弱     | 无seek   | 播放器   | —     | 弱         |
| CDN 友好      | ★★★   | ★              | ★     | ★★     | ★★★   | ★     | ★         |
| NAT 穿透      | ★★★   | ★(TCP交织★★) | ★★   | ★★★   | ★★★   | ★★   | ★★(TURN) |
| 点播/回放     | ★★★   | ★              | 弱     | ✗       | ★★★   | ✗     | ✗         |
| 低延迟        | ✗       | ★★★          | ★★   | ★★     | ✗       | ★★★ | ★★★★   |
| 板卡ffmpeg3.0 | ✅       | ✅              | ✅(拉) | ⚠️     | ✅       | ❌     | ❌         |

## 16. 本项目深度评估

### 16.1 硬约束

| 约束                       | 影响                                  |
| -------------------------- | ------------------------------------- |
| 单核 Cortex-A7，H.264 软解 | 分辨率 ≤480x272；352x288 可 35~60fps |
| ffmpeg 3.0 无 TLS          | 只能 http://                          |
| 无 WebRTC/SRT              | 排除                                  |
| 只能"拉"流                 | RTMP 推对板卡无意义；RTMP 拉可用      |
| 经 Windows ICS NAT         | 出站 OK；RTSP 必须 TCP 交织           |
| 触摸屏无键盘               | UI 简单，协议不受影响                 |

### 16.2 逐协议可行性

| 协议         | 可用？      | 说明                                      |
| ------------ | ----------- | ----------------------------------------- |
| HLS (http)   | ✅ 首选     | 已跑通，~2s，穿透最好，直播+回放通吃   |
| RTSP         | ✅ 代码就绪 | 需 RTSP 服务器（摄像头/mediamtx），0.5~2s |
| RTMP         | ✅ 可拉     | 需 RTMP 服务器；1~3s                      |
| HTTP-FLV     | ⚠️ 需测   | ffmpeg 3.0 flv demuxer 可拉，无 seek      |
| DASH         | ✅ 无必要   | 与 HLS 相当                               |
| SRT / WebRTC | ❌          | SDK 不支持                                |

### 16.3 选型结论

1. **HLS 主力**：简单、可靠、穿透好、直播+回放、~2s 可接受
2. **RTSP 低延迟备选**：0.5~2s，接摄像头
3. 避开 WebRTC/SRT/DASH

## 17. 本项目实现与演进

### 17.1 已实现

| 模块        | 实现                                                                                 |
| ----------- | ------------------------------------------------------------------------------------ |
| 直播        | HLS（`-hls_time 1 -hls_list_size 4 -g 15`，~2s）                                   |
| 录像回放    | MP4（`-f segment -segment_time 60`）+ HTTP Range seek + 进度条 + 时间戳叠加 + 删除 |
| RTSP 客户端 | `rtsp_transport=tcp` + 5s 超时 + 3s 重连（待启用）                                 |
| 分发        | `stream_server.py`（HTTP + Range + /list + DELETE）                                |
| 编码/解码   | H.264（libx264 编 / libavcodec 解），yuv420p→rgb24                                  |
| 界面        | iOS 桌面（相册/视频两应用），进度条、拍摄时间叠加                                    |

### 17.2 演进路径

| 方向         | 内容                                  | 价值        |
| ------------ | ------------------------------------- | ----------- |
| HLS 压到 ~2s | `-hls_time 0.5 -g 7`                | 小改        |
| 启用 RTSP    | 配 mediamtx/摄像头                    | 0.5~2s      |
| NEON 转换    | 自写 yuv420p→rgb24                   | 软解上限 ↑ |
| 推流配合     | ffmpeg/OBS 推 RTMP → 板卡拉 HLS/RTSP | 多源        |

### 17.3 实测数据记录

| 项                          | 值                  | 备注           |
| --------------------------- | ------------------- | -------------- |
| 本地 demo 480x272           | ~14~20fps          | 节流修复后 ~20 |
| Webcam 352x288@15           | FPS=15，延迟~4s     | 源限速         |
| Webcam 640x480@15           | ~9fps               | 解码上限       |
| 关键：`-g` 不设           | 分片~17s，延迟 20s+ | GOP 16.7s      |
| `-g 15` + `-hls_time 1` | 分片 1s，延迟 ~2s   | 修复后(25fps源) |

## 18. 术语表

| 术语                  | 含义                           |
| --------------------- | ------------------------------ |
| GOP                   | 两个关键帧之间的帧组           |
| IDR / 关键帧          | 可独立解码的帧（解码起点）     |
| SPS / PPS             | H.264 序列/图像参数集          |
| NAL                   | H.264 网络抽象层单元           |
| TS                    | 传输流（188B 包）              |
| fMP4 / m4s            | 分段 MP4                       |
| FLV                   | Flash Video 容器               |
| M3U8                  | HLS 播放列表                   |
| MPD                   | DASH 清单                      |
| SDP                   | 会话描述协议（RTSP/WebRTC 用） |
| RTP / RTCP            | 实时传输协议 / 控制协议        |
| RTSP interleaved      | RTP over TCP 交织              |
| PTS / DTS             | 显示时间戳 / 解码时间戳        |
| jitter buffer         | 去抖缓冲                       |
| ARQ                   | 自动重传请求                   |
| ICE / STUN / TURN     | WebRTC 网络穿越三件套          |
| DTLS-SRTP             | 加密的 RTP                     |
| MSE                   | 媒体源扩展（浏览器）           |
| 端到端/Glass-to-Glass | 镜头到屏幕总延迟               |
| MEDIA-SEQUENCE        | HLS 直播窗口序号               |

---

*配套命令与部署细节见同目录 `README.md`；开发环境拓扑见 `dev-environment` 技能。*
