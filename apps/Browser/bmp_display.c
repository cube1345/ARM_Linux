#include "bmp_display.h"

#include "browser_log.h"
#include "bmp_decoder.h"
#include "image_render.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * @brief 将 8 位颜色分量缩放到 framebuffer 位域。
 *
 * @param value 0 到 255 的颜色值。
 * @param bit_length 目标位域长度。
 * @return 缩放后的颜色值。
 */
static uint32_t scale_color(uint8_t value, uint32_t bit_length)
{
    uint64_t maximum;

    if (bit_length == 0) {
        return 0;
    }

    if (bit_length >= 32) {
        maximum = UINT32_MAX;
    } else {
        maximum = ((uint64_t)1 << bit_length) - 1;
    }

    return (uint32_t)(((uint64_t)value * maximum + 127) / 255);
}

/**
 * @brief 按 framebuffer RGB 位域生成目标像素值。
 *
 * @param info framebuffer 可变参数。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 * @return 可写入 framebuffer 的像素值。
 */
static uint32_t make_pixel(const struct fb_var_screeninfo *info,
                           uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel = 0;

    pixel |= scale_color(red, info->red.length) << info->red.offset;
    pixel |= scale_color(green, info->green.length) << info->green.offset;
    pixel |= scale_color(blue, info->blue.length) << info->blue.offset;

    if (info->transp.length > 0) {
        pixel |= scale_color(255, info->transp.length)
                 << info->transp.offset;
    }

    return pixel;
}

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
                        uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel;

    if (display == NULL) {
        errno = EINVAL;
        return -1;
    }

    pixel = make_pixel(&display->variable_info, red, green, blue);
    return video_buffer_put_pixel(&display->back_buffer, x, y, pixel);
}

/**
 * @brief 使用 RGB 颜色清空离屏缓冲区。
 *
 * @param display 显示设备上下文。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 */
void bmp_display_clear(struct bmp_display *display,
                       uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel;

    if (display == NULL) {
        return;
    }

    pixel = make_pixel(&display->variable_info, red, green, blue);
    video_buffer_clear(&display->back_buffer, pixel);
}

/**
 * @brief 将离屏缓冲区刷新到 framebuffer。
 *
 * @param display 显示设备上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_flush(struct bmp_display *display)
{
    if (display == NULL || display->memory == NULL ||
        display->memory == MAP_FAILED) {
        errno = EINVAL;
        return -1;
    }

    return video_buffer_flush(&display->back_buffer, display->memory,
                              display->memory_size);
}

/**
 * @brief 打开 framebuffer 设备并映射显存。
 *
 * @param display 显示设备上下文。
 * @param framebuffer_path framebuffer 设备节点路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_open(struct bmp_display *display,
                     const char *framebuffer_path)
{
    size_t bytes_per_pixel;

    if (display == NULL || framebuffer_path == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(display, 0, sizeof(*display));
    display->fd = -1;
    display->memory = MAP_FAILED;

    display->fd = open(framebuffer_path, O_RDWR);
    if (display->fd < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "open framebuffer");
        return -1;
    }

    if (ioctl(display->fd, FBIOGET_FSCREENINFO,
              &display->fixed_info) < 0 ||
        ioctl(display->fd, FBIOGET_VSCREENINFO,
              &display->variable_info) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR,
                          "get framebuffer information");
        bmp_display_close(display);
        return -1;
    }

    bytes_per_pixel = display->variable_info.bits_per_pixel / 8;
    if (bytes_per_pixel != 2 && bytes_per_pixel != 3 &&
        bytes_per_pixel != 4) {
        browser_log(BROWSER_LOG_ERROR,
                    "unsupported framebuffer depth: %u bpp",
                    display->variable_info.bits_per_pixel);
        bmp_display_close(display);
        return -1;
    }

    display->memory_size = display->fixed_info.smem_len;
    if (display->memory_size == 0) {
        browser_log(BROWSER_LOG_ERROR, "framebuffer memory size is zero");
        bmp_display_close(display);
        return -1;
    }

    display->memory = mmap(NULL, display->memory_size,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           display->fd, 0);
    if (display->memory == MAP_FAILED) {
        browser_log_errno(BROWSER_LOG_ERROR, "map framebuffer");
        bmp_display_close(display);
        return -1;
    }

    if (video_buffer_create(&display->back_buffer,
                            display->variable_info.xres,
                            display->variable_info.yres,
                            display->variable_info.bits_per_pixel,
                            display->fixed_info.line_length) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "create video buffer");
        bmp_display_close(display);
        return -1;
    }

    browser_log(BROWSER_LOG_INFO, "framebuffer: %ux%u, %u bpp",
                display->variable_info.xres,
                display->variable_info.yres,
                display->variable_info.bits_per_pixel);
    return 0;
}

/**
 * @brief 解码 BMP 图片并等比例缩放到屏幕中央。
 *
 * @param display 已打开的显示设备上下文。
 * @param bmp_path BMP 图片路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_show(struct bmp_display *display, const char *bmp_path)
{
    struct image_data image = {0};
    int result;

    if (display == NULL || display->fd < 0 ||
        display->memory == MAP_FAILED || bmp_path == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (bmp_decode(bmp_path, &image) < 0) {
        return -1;
    }

    result = image_render_draw(display, &image, 0);
    if (result < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "render image");
    }

    image_data_destroy(&image);
    return result;
}

/**
 * @brief 解除显存映射并关闭 framebuffer 设备。
 *
 * @param display 显示设备上下文。
 */
void bmp_display_close(struct bmp_display *display)
{
    if (display == NULL) {
        return;
    }

    video_buffer_destroy(&display->back_buffer);

    if (display->memory != NULL && display->memory != MAP_FAILED) {
        munmap(display->memory, display->memory_size);
    }

    if (display->fd >= 0) {
        close(display->fd);
    }

    display->memory = MAP_FAILED;
    display->memory_size = 0;
    display->fd = -1;
}
