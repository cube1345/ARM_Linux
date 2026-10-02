# FB — Framebuffer 显示实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机（linuxfb） |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物直接拷入虚拟机执行（读 `/dev/fb0`） |

## 说明

`/dev/fb0` 帧缓冲基础实验：查询屏幕信息（分辨率/色深/位域）、绘制图形、显示 BMP。对应学习笔记 `docx/` 的 Framebuffer 章节。

## 产物

- `fb_info` — 读取并打印 fb_var_screeninfo / fb_fix_screeninfo
- `fb_draw` — 在帧缓冲上绘制填充矩形/线条
- `fb_bmp` — 把 BMP 位图画到屏幕