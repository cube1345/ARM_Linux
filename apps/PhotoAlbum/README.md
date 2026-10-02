# PhotoAlbum — i.MX6ULL 智能桌面（相册 + 监控 + 视频）

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | i.MX6ULL 真机（7 寸 1024×600 触摸屏，linuxfb） |
| 工具链 | NXP fsl-imx-x11（arm-poky，`sdk/` 内 Qt 5.12.9） |
| 构建 | `bash qt/build.sh`（CMake 交叉编译） |
| 部署 | scp `qt/.dist/photo-album` + `qt/qt_env.sh` → ssh `root@<ip>`，板端 `. ./qt_env.sh && ./photo-album /root/photos` |

## 说明

- **权威源码在 `qt/`**；根目录的 `stream_server.py` / `windows/` / `*.err` / `snap/` / `record/` 等是板端运行产物，不入库。
- 功能：相册（单/双指手势、缩放裁剪）、9 路监控同屏、视频 / 直播 / 录像回放、迷你游戏、ICM-20608 IMU 方向锁定。
- 摄像头转码 / 推流是 Windows 侧服务链（ffmpeg + mediamtx），只在 Windows 上联调。
- **完整构建 / 部署 / 排障文档见本目录 `CLAUDE.md`**；板卡访问（IP / ssh 参数）见仓库根 `CLAUDE.md`。