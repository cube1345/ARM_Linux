#ifndef BMP_DISPLAY_H
#define BMP_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include <linux/fb.h>

#include "video_buffer.h"

/**
 * @brief Framebuffer 显示设备上下文。
 */
struct bmp_display {
    int fd;
    uint8_t *memory;
    size_t memory_size;
    struct fb_var_screeninfo variable_info;
    struct fb_fix_screeninfo fixed_info;
    struct video_buffer back_buffer;
};

/**
 * @brief 打开 framebuffer 设备并映射显存。
 *
 * @param display 显示设备上下文。
 * @param framebuffer_path framebuffer 设备节点路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_open(struct bmp_display *display,
                     const char *framebuffer_path);

/**
 * @brief 使用 RGB 颜色清空离屏缓冲区。
 *
 * @param display 显示设备上下文。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 */
void bmp_display_clear(struct bmp_display *display,
                       uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 向离屏缓冲区写入一个 RGB 像素。
 *
 * @param display 显示设备上下文。
 * @param x 目标 X 坐标。
 * @param y 目标 Y 坐标。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_put_rgb(struct bmp_display *display, int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 将离屏缓冲区刷新到 framebuffer。
 *
 * @param display 显示设备上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_flush(struct bmp_display *display);

/**
 * @brief 将 BMP 图片等比例缩放并居中显示。
 *
 * @param display 已打开的显示设备上下文。
 * @param bmp_path BMP 图片路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_show(struct bmp_display *display, const char *bmp_path);

/**
 * @brief 解除显存映射并关闭 framebuffer 设备。
 *
 * @param display 显示设备上下文。
 */
void bmp_display_close(struct bmp_display *display);

#endif
