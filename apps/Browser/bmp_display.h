#ifndef BMP_DISPLAY_H
#define BMP_DISPLAY_H

#include <stddef.h>
#include <stdint.h>

#include <linux/fb.h>

/**
 * @brief Framebuffer 显示设备上下文。
 */
struct bmp_display {
    int fd;
    uint8_t *memory;
    size_t memory_size;
    struct fb_var_screeninfo variable_info;
    struct fb_fix_screeninfo fixed_info;
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
