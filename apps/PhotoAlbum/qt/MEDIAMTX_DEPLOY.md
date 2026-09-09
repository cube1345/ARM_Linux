# mediamtx 部署手册（Windows 流媒体服务器版）

> 面向 PhotoAlbum 项目：用 mediamtx 在 Windows/办公服务器上做"拉摄像头 → 出 HLS/RTSP/RTMP"，
> 板卡经以太网连 Windows 实时查看/回放。本文是当前"ffmpeg 中转 + stream_server.py"方案的
> 升级/替代版，功能对应办公服务器上的 mediamtx / SRS / nginx-rtmp。

---

## 目录

1. [为什么用 mediamtx](#1-为什么用-mediamtx)
2. [安装（Windows）](#2-安装windows)
3. [两种部署形态（重要：HEVC 决定选型）](#3-两种部署形态重要hevc-决定选型)
4. [形态A：摄像头改 H.264 + mediamtx 单用](#4-形态a摄像头改-h264--mediamtx-单用)
5. [形态B：ffmpeg 转码 + mediamtx](#5-形态bffmpeg-转码--mediamtx)
6. [mediamtx.yml 配置详解](#6-mediamtxyml-配置详解)
7. [录像（mediamtx 内置）](#7-录像mediamtx-内置)
8. [板卡接入配置](#8-板卡接入配置)
9. [防火墙与端口](#9-防火墙与端口)
10. [与现状方案对比](#10-与现状方案对比)
11. [办公服务器（Linux）部署对照](#11-办公服务器linux部署对照)
12. [排障手册](#12-排障手册)
13. [安全注意](#13-安全注意)

---

## 1. 为什么用 mediamtx

|              | 现状（ffmpeg 中转 + stream_server.py） | mediamtx                             |
| ------------ | -------------------------------------- | ------------------------------------ |
| 拉摄像头     | ffmpeg 拉 RTSP                         | mediamtx 内置（配置 source）         |
| 转码         | ffmpeg 必须                            | mediamtx**不转码**（只转封装） |
| 出 HLS       | 自己写分片 + py 服务器                 | **内置 HTTP 服务器自动出 HLS** |
| 出 RTSP/RTMP | 无                                     | 内置（低延迟口）                     |
| 录像         | ffmpeg -f segment                      | 内置 recordPath                      |
| 多路/并发    | 弱                                     | 强                                   |
| 维护         | 两个进程手动拼                         | 单进程单配置                         |

mediamtx 是单个跨平台可执行文件，功能 = "迷你 SRS/nginx-rtmp"，自带 RTSP/RTMP/HLS/WebRTC 输出与 HTTP 服务器。

---

## 2. 安装（Windows）

```powershell
# 下载 Windows 版（GitHub releases）
# https://github.com/bluenviron/mediamtx/releases
#   选 mediamtx_windows_amd64.zip
# 解压到 E:\mediamtx\，得到：
#   mediamtx.exe
#   mediamtx.yml      （配置文件）
```

启动：

```powershell
cd E:\mediamtx
.\mediamtx.exe
```

默认监听：

- RTSP :8554
- RTMP :1935
- HLS / HTTP :8888

---

## 3. 两种部署形态（重要：HEVC 决定选型）

我们的摄像头（192.168.114.48 录像机）子码流是 **HEVC(H.265)**，而**板卡软解不了 HEVC**。mediamtx 不转码，所以必须二选一：

```
形态A：把摄像头子码流改成 H.264 → mediamtx 单独即可（最省事）
形态B：摄像头保持 HEVC → ffmpeg 先转成 H.264 再推给 mediamtx（不动摄像头配置）
```

|              | 形态A                      | 形态B             |
| ------------ | -------------------------- | ----------------- |
| 摄像头子码流 | 改成 H.264                 | 保持 HEVC         |
| 进程         | 仅 mediamtx                | ffmpeg + mediamtx |
| 影响他人     | 改监控配置（可能影响他人） | 无                |
| 推荐         | 能改就用                   | 改不了时用        |

---

## 4. 形态A：摄像头改 H.264 + mediamtx 单用

### 4.1 改摄像头子码流编码

1. 浏览器打开 `http://192.168.114.48`（录像机管理页）
2. 登录（admin + 密码）
3. **配置 → 通道管理 → 选通道（第 5 路）→ 视频/编码参数 → 子码流**
4. 视频编码：`H.265 → H.264`，分辨率 `640x360` 或 `704x576`
5. 保存

### 4.2 配置 mediamtx.yml

```yaml
paths:
  live:
    source: rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502
    sourceOnDemand: no
    sourceProtocol: tcp          # RTSP 走 TCP，穿 NAT
```

### 4.3 运行与访问

```powershell
.\mediamtx.exe
```

| 输出 | 地址                                          |
| ---- | --------------------------------------------- |
| HLS  | `http://192.168.137.1:8888/live/index.m3u8` |
| RTSP | `rtsp://192.168.137.1:8554/live`            |
| RTMP | `rtmp://192.168.137.1:1935/live`            |

---

## 5. 形态B：ffmpeg 转码 + mediamtx

摄像头保持 HEVC，Windows 上跑两个进程：

### 5.1 mediamtx.yml（只监听，路径 live/stream 接收推流）

```yaml
paths:
  stream:
    # 不写 source → 等待外部推流（RTMP 1935 或 RTSP 8554 推送）
    sourceOnDemand: no
```

### 5.2 ffmpeg 转码推流

```powershell
ffmpeg -rtsp_transport tcp -i "rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502" `
  -vf scale=480:270 -an `
  -c:v libx264 -preset veryfast -tune zerolatency -g 15 -keyint_min 15 -pix_fmt yuv420p `
  -f flv rtmp://127.0.0.1:1935/live/stream
```

参数说明：

- `-rtsp_transport tcp`：拉摄像头用 TCP（NAT 友好）
- `-vf scale=480:270`：压到板卡友好分辨率（画质不够改 640:360）
- `-an`：去掉音频（板卡只显示画面）
- `-g 15`：1 秒关键帧 → HLS 分片 1s → 低延迟
- 推流地址：`rtmp://127.0.0.1:1935/live/stream`

### 5.3 访问

| 输出 | 地址                                                 |
| ---- | ---------------------------------------------------- |
| HLS  | `http://192.168.137.1:8888/live/stream/index.m3u8` |
| RTSP | `rtsp://192.168.137.1:8554/live/stream`            |
| RTMP | `rtmp://192.168.137.1:1935/live/stream`            |

---

## 6. mediamtx.yml 配置详解

```yaml
# 服务器监听
rtspAddress: :8554
rtmpAddress: :1935
hlsAddress: :8888          # HLS 的 HTTP 端口
webrtcAddress: :8889       # WebRTC（本项目不用）

# HLS 切片（控制延迟，对应我们 ffmpeg 的 -g/-hls_time）
hlsSegmentCount: 7
hlsSegmentDuration: 1s     # 1 秒分片
hlsAllowOrigin: '*'

paths:
  # —— 形态A：直接拉摄像头 ——
  live:
    source: rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502
    sourceOnDemand: no
    sourceProtocol: tcp

  # —— 形态B：接收 ffmpeg 推流 ——
  stream:
    sourceOnDemand: no

  # —— 录像（可选）——
  rec:
    source: rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502
    sourceOnDemand: yes
    recordPath: E:/mediamtx/record/%path/%Y-%m-%d_%H-%M-%S.mp4
    recordSegmentDuration: 60s    # 每 60 秒一段（对应我们之前的录像）
```

关键参数：

| 参数                      | 说明                                                   |
| ------------------------- | ------------------------------------------------------ |
| `hlsSegmentDuration`    | HLS 分片时长（越低延迟越低，别低于 GOP）               |
| `sourceOnDemand`        | 有观众才拉源（省资源）                                 |
| `sourceProtocol`        | RTSP 传输（`tcp` 穿 NAT）                            |
| `recordPath`            | 录像路径模板（`%path`=流路径，`%Y-%m-%d...`=时间） |
| `recordSegmentDuration` | 录像切段时间                                           |

---

## 7. 录像（mediamtx 内置）

开录像只需在路径里加 `recordPath`：

```yaml
paths:
  live:
    source: rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502
    sourceProtocol: tcp
    recordPath: E:/mediamtx/record/%path/%Y-%m-%d_%H-%M-%S.mp4
    recordSegmentDuration: 60s
```

- 自动按 `%Y-%m-%d_%H-%M-%S` 命名、每 60 秒切段
- 录像文件可直接用任意播放器打开
- 若板卡要"回放+进度条+删除"，把录像目录放到 `stream_server.py` 的服务目录，由它提供 `/list` 和 `DELETE`（见 `README.md`）

---

## 8. 板卡接入配置

```sh
# 形态A
echo "http://192.168.137.1:8888/live/index.m3u8" > ./streams.txt
# 形态B
echo "http://192.168.137.1:8888/live/stream/index.m3u8" > ./streams.txt

./photo-album ./photos
```

「视频」页 `网络流:` → 实时画面（H.264、480x270、板卡软解无压力）。

> 也可直接走 mediamtx 的 RTSP 口（更低延迟 0.5~2s）：
>
> ```sh
> echo "rtsp://192.168.137.1:8554/live/stream" > ./streams.txt
> ```
>
> 播放器已支持 RTSP（TCP 传输 + 重连）。

---

## 9. 防火墙与端口

Windows 放行（管理员 PowerShell）：

```powershell
netsh advfirewall firewall add rule name="mediamtx RTSP" dir=in action=allow protocol=TCP localport=8554
netsh advfirewall firewall add rule name="mediamtx RTMP" dir=in action=allow protocol=TCP localport=1935
netsh advfirewall firewall add rule name="mediamtx HLS"  dir=in action=allow protocol=TCP localport=8888
netsh advfirewall firewall add rule name="stream_server" dir=in action=allow protocol=TCP localport=8000
```

验证监听：

```powershell
netstat -ano | findstr "8554 1935 8888"
```

---

## 10. 与现状方案对比

| 项                 | 现状（ffmpeg + stream_server.py） | mediamtx（形态B）            |
| ------------------ | --------------------------------- | ---------------------------- |
| 拉摄像头           | ffmpeg`-i rtsp://...`           | mediamtx`source:`          |
| HEVC→H.264        | ffmpeg 转码                       | ffmpeg 转码（同样需要）      |
| 出 HLS             | ffmpeg`-f hls` 写分片           | mediamtx 内置                |
| HTTP 分发          | stream_server.py                  | mediamtx 内置 :8888          |
| 录像               | ffmpeg`-f segment`              | mediamtx`recordPath`       |
| RTSP/RTMP 低延迟口 | 无                                | 有（8554/1935）              |
| 进程数             | 2 个                              | 2 个（形态B）/ 1 个（形态A） |
| 多路/并发          | 弱                                | 强                           |
| 稳定性             | 手动拼装                          | 单进程框架更稳               |

**结论**：mediamtx 把"出 HLS / HTTP 服务 / 录像 / 低延迟口"统一进一个进程，比手写 ffmpeg+py 更稳、更接近生产。代价是要装 mediamtx；HEVC 的转码仍需 ffmpeg（或改摄像头为 H.264 后免掉）。

---

## 11. 办公服务器（Linux）部署对照

同一套 mediamtx 直接跑 Linux，配置完全一样：

```bash
# Ubuntu/Debian 下载
wget https://github.com/bluenviron/mediamtx/releases/latest/download/mediamtx_linux_amd64.tar.gz
tar xzf mediamtx_linux_amd64.tar.gz
./mediamtx

# systemd 开机自启
sudo cp mediamtx /usr/local/bin/
sudo useradd -r mediamtx
# 配置 /etc/mediamtx.yml 后：
sudo systemctl enable --now mediamtx
```

- Linux 版同样支持形态A/B
- 服务器若有公网入口，板卡直接拉 `http://<服务器公网>:8888/live/index.m3u8`（经 Windows ICS 出站公网）
- 服务器纯内网 → Windows 用反向代理（nginx）或继续做轻量中继

---

## 12. 排障手册

| 症状               | 排查                                                                                              |
| ------------------ | ------------------------------------------------------------------------------------------------- |
| mediamtx 起不来    | 看终端报错；`netstat` 端口是否被占（旧 stream_server/py 可能占 8888？不冲突，但别占 1935/8554） |
| HLS 打开 404       | 确认路径：`/live/index.m3u8`（形态A）vs `/live/stream/index.m3u8`（形态B）                    |
| 板卡黑屏           | 板卡`curl -s http://192.168.137.1:8888/live/index.m3u8` 看返回                                  |
| 形态A 拉不到摄像头 | `sourceProtocol: tcp` 没写 / 摄像头账号错（mediamtx 日志有报错）                                |
| 形态B 没有流       | ffmpeg 是否还在推（窗口有没有报错）；mediamtx 日志是否收到推流                                    |
| 延迟高             | 摄像头 GOP 太长 → 改子码流帧率/`-g`；或 mediamtx `hlsSegmentDuration` 调小                   |
| 板卡解不动         | 确认输出是 H.264（ffprobe 看）；分辨率 ≤480x270                                                  |

验证工具：

```powershell
ffprobe http://127.0.0.1:8888/live/index.m3u8        # 看编码/分辨率
curl -s http://127.0.0.1:8888/live/index.m3u8 | Select-Object -First 10
```

---

## 13. 安全注意

- mediamtx 默认无鉴权：加 `paths.*.publishUser/publishPass`（推流）与 HTTP 鉴权
- 若开放公网：务必加鉴权 + 非默认端口，否则摄像头流会公网裸奔
- 摄像头账号密码写进配置/命令，注意不要提交到 git

---

*配套项目说明见 `README.md`；流协议原理见 `STREAMING_PROTOCOLS.md`。*
