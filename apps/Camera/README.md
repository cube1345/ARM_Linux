  # i.MX6ULL 触摸相册应用计划

  ## Summary

  - 新增 ARM_Linux/IMX6ll/07_photo_viewer，实现原生 Linux C 相册应用，不依赖 X11/Qt/Debian。
  - 使用 /dev/fb0 显示 480x272 RGB565 图片，使用 /dev/input/event* 读取电容触摸事件。
  - 默认扫描 /root/photos，按文件名排序，支持 JPEG/PNG/BMP/GIF。
  - 支持：左右切换、单指拖动、双指缩放、双击复位、长按进入裁剪模式；裁剪只影响当前视图，不修改或保
    存图片文件。

  - 第一版使用 CPU 双线性缩放；PXP 硬件加速作为后续优化，不阻塞 V1。

  ## Key Changes

  - 应用结构
      - 新建 photo_viewer.c、framebuffer.c/.h、touch.c/.h、image.c/.h、gesture.c/.h。
      - 加入 header-only stb_image.h 解码 JPEG/PNG/BMP/GIF，避免依赖板端 libjpeg/libpng。
      - 新增 Makefile，使用现有 Linaro GCC 4.9.4 交叉编译。

  - 显示管线
      - 打开 /dev/fb0，读取 fb_var_screeninfo / fb_fix_screeninfo，按 RGB 位域转换到屏幕格式。
      - 使用内存 back buffer 绘制，再写入 framebuffer；程序启动保存原屏幕内容，退出时恢复。
      - 图片初始按屏幕比例适配显示，背景为黑色，顶部显示索引、文件名、缩放比例和模式。

  - 触摸与手势
      - 自动选择第一个具有 ABS_MT_POSITION_X/Y 的 input 设备，也允许启动参数指定 /dev/input/
        eventX。

      - 支持 Linux multitouch Protocol B，最多跟踪 2 指。
      - 单指水平快速滑动：上一张/下一张。
      - 放大后单指拖动：平移图片。
      - 双指 pinch：以两指中点为中心缩放，范围 1x～8x。
      - 双击：在 1x 和 2.5x 之间切换。
      - 长按约 600ms：进入裁剪模式；单指拖出矩形后，通过屏幕底部 Apply / Cancel / Reset 按钮应用或
        取消。

      - 裁剪结果只是改变后续渲染的源区域；切换图片或 Reset 后恢复原图。

  - 硬件验收前置
      - 上板确认 /dev/fb0 存在且分辨率 480x272、bpp 为 16。
      - 查看触摸设备：cat /proc/bus/input/devices。
      - V1 不修改内核和 DTS；若没有触摸事件，再单独排查 FT5306 与 Goodix 节点。

  ## Test Plan

  - 主机侧
      - 运行 make，确认生成 ARM ELF。
      - 用错误图片、空目录、超分辨率图片验证加载错误提示和资源释放。
      - 将应用与测试图片传入 /root/photos。
      - 验证 3 张以上图片能按文件名顺序切换。
      - 验证拖动、pinch 缩放、边界限制、双击复位、长按裁剪、Apply/Cancel/Reset。
      - 验证程序退出后 framebuffer 恢复，串口无报错，重复运行无内存泄漏。

  - 性能目标
      - 普通图片全屏重绘低于 150ms。
      - 单指平移和双指缩放操作可连续响应，无明显卡死。

  ## Assumptions

  - 当前 4.3 寸屏继续使用官方 imx6ull-14x14-emmc-4.3-480x272-c.dtb。
  - 电容触摸硬件可用，实际触摸事件由 edt-ft5x06 或 Goodix 驱动提供。
  - V1 图片来源固定为 /root/photos。
  - V1 不实现图片另存、旋转、幻灯片、缩略图墙和 PXP 加速。