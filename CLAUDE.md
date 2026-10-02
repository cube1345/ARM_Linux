# CLAUDE.md — 仓库协作公约

面向在此仓库工作的 Claude Code / 协作者的跨项目规范。**每项目各自的 CLAUDE.md / README.md 优先于本文件**；本文件只约定「多项目共存」的公共事务。

## 仓库本质：单仓库多独立项目

- 同一根目录托管多个**互相独立**的嵌入式项目（monorepo）：Linux 应用、驱动、手册、SDK、产物。
- 项目之间没有编译依赖（各自的 Makefile / Kbuild / CMake），**改一个项目不要顺手动另一个**。
- 新增项目 → 新建目录 + README（首行声明：目标板 / 工具链 / 构建命令 / 部署路径）+ 单一构建入口 + 产物忽略规则。命名用 kebab-case（存量目录如 `PhotoAlbum`/`IMX6ll` 保留原名，避免破坏引用）。

## 两套工具链：构建前必须认准

| 链 | 目标 | 工具链位置 | 覆盖项目 |
|---|---|---|---|
| AArch64 | QEMU 虚拟机 / RK3506 | `~/WorkSpace/Linux/ARM_Linux/host/bin/`（buildroot，仓库外） | Browser、FB/IIC/input/SPI/UART、update_agent |
| ARMv7hf | i.MX6ULL 真机 | `sdk/fsl-imx-x11/4.1.15-2.1.0/` 内 arm-poky，或内核 Kbuild | PhotoAlbum、Camera、drivers/imx6ull、drivers |

注意：
- `apps/PhotoAlbum` 权威源码在 `qt/`；构建 `bash apps/PhotoAlbum/qt/build.sh`（CMake），部署与排查见 `apps/PhotoAlbum/CLAUDE.md`。
- 内核驱动（`drivers/imx6ull/*`）的 Makefile 残留教程作者路径 `KERNELDIR=/home/zuozhongkai/...`，编译前需改成本机 i.MX6ULL 内核源码路径。
- 摄像头/监控涉及 Windows 侧服务链（ffmpeg/mediamtx），只在 Windows 上联调，板卡只拉流软解。

## Git 纪律

- 产物不入库：已由 `.gitignore`（构建中间件/可执行/dtb/镜像/日志/PDF/视频）与 `.gitattributes`（统一 LF）覆盖。误入库产物用 `git rm --cached` 移出并补规则。
- 原子提交：一次一个项目一个意图；message 带项目前缀（如 `photoalbum: 修复双指残留`）。
- 动架构 / 新增项目 → 同步更新根 `Readme.md` 的板卡矩阵与索引。
- 提交信息结尾保留 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。

## 常用命令

```sh
# i.MX6ULL 相册（交叉编译 + 单测）
bash apps/PhotoAlbum/qt/build.sh
cd /tmp/pa-host && ./tests/tests          # 主机单元测试

# Browser（AArch64 虚拟机）
cd apps/Browser && make && ./scripts/start-qemu.sh --fb

# 外设验证 demo（FB/IIC/input/SPI/UART/update_agent）
make -C apps/<name>
```

## 板卡访问（i.MX6ULL 真机）

- 以太网直连，IP 通常 `10.42.0.91`（NetworkManager 共享分配，扫 `10.42.0.0/24` 确定）；`root` 无密码。
- ssh/scp 必须带兼容参数（板端 dropbear 只提供 ssh-rsa）：
  `-o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa`
- 部署：`scp apps/PhotoAlbum/qt/.dist/photo-album apps/PhotoAlbum/qt/qt_env.sh root@<ip>:/home/root/`，板端 `cd /home/root && . ./qt_env.sh && ./photo-album /root/photos`。
- 启动前必须杀 psplash 等占用 `/dev/fb0` 的进程（linuxfb 无合成器，会抢画面/触摸）。