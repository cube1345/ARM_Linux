# ARM_Linux_WS — 嵌入式 Linux 多项目工作区

单仓库（monorepo）托管多个**互相独立的嵌入式 Linux（ARM 端）项目**：用户态应用、内核驱动、学习手册，以及各自独立的脚本、构建产物。每个项目在目录内自包含、自治，共享仓库级公约与 `sdk/` 工具链。

## 板卡矩阵：项目 → 目标环境 / 工具链 / 构建 / 部署

| 项目 | 目标环境 | 工具链 | 构建入口 | 部署 / 运行 |
|---|---|---|---|---|
| `apps/Browser` | QEMU AArch64 虚拟机；RK3506 移植中 | buildroot aarch64 | `make` | `./scripts/start-qemu.sh --fb` |
| `apps/Browser/qt` | RK3506 | RK3506 SDK Qt 5.15 | `media-browser-qt.pro` | `media-browser-qt` |
| `apps/PhotoAlbum` | i.MX6ULL 真机 | NXP fsl-imx-x11（arm-poky） | `qt/build.sh`（CMake） | scp + `qt_env.sh` |
| `apps/Camera` | 交叉编译实验 | 自带 libjpeg 源码 | jpeg-10 autotools | `djpeg` 等 |
| `apps/FB` / `IIC` / `input` / `SPI` / `UART` | QEMU AArch64 | aarch64-buildroot | `make` | 直接运行 |
| `apps/update_agent` | QEMU AArch64 | aarch64-buildroot | `make` | `update_agent` |
| `IMX6ll/` `drivers/` | i.MX6ULL | 内核 Kbuild | `make build` | `insmod / rmmod` |

> **两条工具链条并行**：
> - **AArch64 线**（QEMU 虚拟机 / RK3506）：`~/WorkSpace/Linux/ARM_Linux/host/bin/`（buildroot 输出，在仓库外）
> - **ARMv7hf 线**（i.MX6ULL 真机）：NXP SDK（`sdk/` 内 arm-poky）或内核 Kbuild
>
> 构建前先认准目标环境，别把 AArch64 产物当成 i.MX6ULL 用。

## 目录职能

| 目录 | 职责 |
|---|---|
| `apps/` | 用户态应用，每应用一个子目录（自治骨架：README + 单一构建入口） |
| `IMX6ll/` `drivers/` | 内核驱动实验（历史分两处命名，规划归一） |
| `docx/` | 学习手册（正点原子教程 + 自写笔记） |
| `sdk/` | NXP fsl-imx-x11 交叉工具链（gitignored） |
| `MC632X/` | 外设固件镜像（gitignored） |

## 项目索引

| 项目 | 一句话 | 入口文档 |
|---|---|---|
| Browser | 多媒体文件浏览器桌面（FB + Input + FreeType + FFmpeg），含独立 Qt 前端 | `apps/Browser/README.md` |
| PhotoAlbum | i.MX6ULL 智能桌面：相册 + 9 路监控 + 视频 + 迷你游戏 + IMU 方向锁定 | `apps/PhotoAlbum/CLAUDE.md` |
| Camera | libjpeg 源码内嵌 + JPEG 解码工具 | `apps/Camera/README.md` |
| FB / IIC / input / SPI / UART | AArch64 虚拟机外设验证 demo | — |
| update_agent | AArch64 OTA 升级客户端 | — |
| IMX6ll / drivers | i.MX6ULL 驱动实验（字符设备 → LED → DTS → 蜂鸣器） | `docx/` 笔记 |

## 仓库级公约

- **每项目自治**：目录 = 项目，携带自己的 README、构建入口、docs、产物（`build/`）
- **产物不入库**：由 `.gitignore` 统一覆盖（构建中间件、可执行、dtb/镜像/日志）；行尾统一 LF（`.gitattributes`）
- **原子提交**：一次一个项目一个意图，commit message 带项目前缀（如 `photoalbum: xxx`）
- **改动同步索引**：动架构 / 接口 / 新增项目 → 同步更新本 README 的板卡矩阵
- 协作细节见根 `CLAUDE.md`；每项目自己的 `CLAUDE.md` / `README.md` 优先于仓库级规则