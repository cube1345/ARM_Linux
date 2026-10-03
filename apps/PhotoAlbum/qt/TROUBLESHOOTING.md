# 故障排查与问题解决记录

本文档记录本项目开发中实际遇到并已解决的问题，按主题分类。
每条含「问题 / 根因 / 解决」；涉及代码的给出引用位置，涉及设施的给出做法。

---

## 一、硬件 / 系统层

### 1. 仓库迁移后 SDK 交叉编译器无法运行（`No such file or directory` / cmake toolchain 127）
- **根因**：NXP SDK 的 `gcc/g++` 二进制把动态链接器（interpreter）硬编码进旧路径 `~/WorkSpace/ARM_Linux/sdk/...`，仓库移到 `Linux/ARM_Linux_WS` 后加载失败。
- **解决**：建立符号链接让旧路径可解析：`ln -s ~/WorkSpace/Linux/ARM_Linux_WS ~/WorkSpace/ARM_Linux`。SDK 二进制所有写死的旧路径随之可用。
- **补充**：`build.sh` 与 `cmake/toolchain-arm.cmake` 里 `PROJECT_DIR` / `SDK_ROOT` 硬编码路径也要改到当前仓库。

### 2. 板卡 root 家目录是 `/home/root`，不是 `/root`
- **现象**：`scp ... root@板卡:/root/` 报 `No such file or directory`。
- **根因**：正点原子 rootfs 的 root 家目录实为 `/home/root`。
- **解决**：部署、自启脚本、文档统一用 `/home/root`。

### 3. 系统自启挂在 `/etc/rc.local` 却不生效
- **根因**：该 Buildroot 的 boot 链是 `inittab → rcS → /etc/rcS.d/S*`，**不执行 `/etc/rc.local`**。
- **解决**：自启脚本放 `/etc/rcS.d/S20photoalbum.sh`（name 带编号紧跟 S 前缀），并在其中 kill 占 `/dev/fb0` 的进程 + `dd` 清屏后再起应用。

### 4. 禁用 psplash 的方式是坑：mv 成 `.bak` 无效
- **现象**：禁用 psplash 后开机仍出现 logo；psplash 与 app 抢 `/dev/fb0` 导致黑屏/混乱。
- **根因**：rc 脚本用 `for i in /etc/rcS.d/S*` 遍历，**`.bak` 也以 S 开头会被执行**。
- **解决**：删除该软链（`rm /etc/rcS.d/S00psplash.sh`，init.d 本体保留可恢复）；自启脚本首行加 `pkill -9 psplash` 兜底。

### 5. 中文歌名/文件名乱码
- **根因**：Qt 在非 UTF-8 locale 下用 `local8bit` 解码 UTF-8 文件名。
- **解决**：`qt_env.sh` 加 `export LANG=C.UTF-8; export LC_ALL=C.UTF-8`。

---

## 二、相册 / UI / 手势

### 6. 开屏动画显示成“假黑屏”（cover 缩放失效）
- **根因**：`QSize` 的 `operator*` 只接收 `int`，`qreal` 系数被截断成 1，cover 缩放退化成原尺寸居中 → 四周大片黑边。
- **解决**：改用 `qRound(pm.width()*scale)` 等整数 cover 计算。

### 7. 应用图标空白（无 SVG 的 4 个游戏）
- **根因**：`locateSvg("")` 对空串返回程序目录本身（`QFile::exists` 对目录也为 true），被误判“找到 SVG”，加载目录失败只画底、无图形。
- **解决**：`locateSvg` 开头 `if (rel.isEmpty()) return QString();`。

### 8. 桌面 SVG 图标不显示（有底色无白线条）
- **根因**：板卡 Qt 的 `libqsvgicon.so` 在 `/usr/lib/plugins`，Qt 默认插件路径不含它。
- **解决**：`qt_env.sh` 加 `export QT_PLUGIN_PATH=/usr/lib/plugins`。

### 9. 壁纸拉伸变形
- **根因**：`QLabel::setScaledContents(true)` 强拉宽高比。
- **解决**：手动 cover（等比放大 + 居中裁剪）。

### 10. 多页桌面滑动不生效（桌面整屏）
- **根因**：Qt 子部件的鼠标事件不会自动冒泡到父级 `eventFilter`，且鼠标第一站是 `QGraphicsView::viewport`，页面子树收不到 `MouseMove`（被 scene 拦截），只能收到 press。
- **解决**：手势捕获上移到 `m_view->viewport()->installEventFilter(this)`；桌面整屏切页在 MainWindow 全局手势中处理（HomePageView 内部已自处理，外围由主窗口补）。

### 11. 触摸多指只能识别一个触点
- **根因**：linuxfb 触摸在未接受 `WA_AcceptTouchEvents` 的控件上被合成为单点鼠标。
- **解决**：只对桌面页 `homePage` 设 `WA_AcceptTouchEvents`（多指识别）；相册 photo_view 自带双指，勿改全局 viewport（否则双指链路被干扰）。

### 12. 3D/自绘控件事件被全局过滤器误吞
- **注意**：给全局手势装 filter 时，需对“点击型”控件返回 false 放行；滑动（阈值 42px）才拦截。

---

## 三、小游戏手势

### 13. 游戏方向只能用屏幕方向键
- **根因**：2048/贪吃蛇/俄罗斯方块/打砖块方向全靠 UI 按钮，无全屏滑动手势。
- **解决**：MainWindow 全局方向分发：viewport 层捕获单指滑动，取水平/垂直主导方向，调用各游戏对应接口（2048.`move`、蛇.`setDirection`、俄罗斯方块 左/右/上旋转/下软落、打砖块 左右短移动）。

---

## 四、音乐模块

### 14. 播放卡死 / 切歌卡 UI / 切歌后不播放
- **根因**：解码线程阻塞；切歌时 UI 线程 `stop()+wait()` 死等，或在线程仍在运行对其 `start()` 造成状态错乱。
- **解决**：
  - ALSA 写入 `snd_pcm_recover` 处理 underrun，失败才退出；
  - 切歌改**异步排队**：旧曲在播则 `stop()` + 50ms 轮询，线程结束后再 `launch` 目标曲（不阻塞 UI、不吞切歌）；
  - `playbackFinished` 尊重 pending（不自动跳歌抢占切歌目标）。

### 15. 坏曲秒退 → 无限自动跳歌
- **根因**：读文件/解码异常立即结束，`playbackFinished→next()` 死循环。
- **解决**：`QElapsedTimer` 记录单曲时长，连续 3 首播放 <2s 即停止自动轮播。

### 16. 音量调节不生效（WAV 直放路径）
- **根因**：`runWav` 的增益在播放开始时读一次存局部变量，拖动滑杆不刷新。
- **解决**：循环内每块数据重读目标音量并平滑渐变（与 FFmpeg 路径一致）。

### 17. 音量突变爆音
- **根因**：增益 0↔100 突变。
- **解决**：跨帧增益斜坡（每帧限定 ±0.02），0 音量样本清零真静音。

### 18. 单核 CPU 下界面卡顿
- **根因**：i.MX6ULL 单核，实时软解 MP3 占满 CPU；UI 每次重绘还全量画封面。
- **解决**：
  - **预解码**：离线把 mp3 → PCM WAV（44.1kHz/16bit 双声道），播放“读 PCM 写 ALSA”零实时解码；
  - **封面/背景缓存**：paintEvent 只在尺寸变化时重建背景，播放中仅重绘文字。

### 19. 不同歌曲同一音量听感不一致
- **根因**：各下载源本身响度差异大（不同母带）。
- **解决**：转 WAV 时用 `loudnorm=I=-16:TP=-1.5:LRA=11`（EBU R128）统一响度（归一后各曲 mean_volume 差异 < 1dB）。

### 20. 用 bash 批量下载歌曲：中文文件名乱码 / 误匹配非原曲
- **根因**：bash 处理 heredoc 中文在部分 locale 下破坏字节；bilibili 搜索命中错误视频（如“水.mp3”）。
- **解决**：下载脚本改 Python（UTF-8 稳定），并对候选做**标题含歌名 + 过滤鼓谱/伴奏/解说 + 时长 150-340s**三重校验；歌词经网易云 search+lyric 接口获取 `.lrc`。

---

## 常用修复命令速查

```sh
# 板卡自启（rcS 而非 rc.local）
cat > /etc/rcS.d/S20photoalbum.sh <<'EOF'
#!/bin/sh
pkill -9 psplash
sleep 1
for p in /proc/[0-9]*; do ls $p/fd 2>/dev/null | grep -q /dev/fb0 && kill -9 ${p##*/}; done
dd if=/dev/zero of=/dev/fb0 bs=4096 count=300 2>/dev/null
cd /home/root && . ./qt_env.sh && ./photo-album /home/root/photos &
exit 0
EOF

# qt_env.sh 关键环境
export LANG=C.UTF-8 LC_ALL=C.UTF-8
export QT_PLUGIN_PATH=/usr/lib/plugins
# QT_QPA_FONTDIR=/usr/share/fonts/ttf  (字体子目录)
```