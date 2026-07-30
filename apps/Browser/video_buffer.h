#ifndef VIDEO_BUFFER_H
#define VIDEO_BUFFER_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 用户空间离屏图像缓冲区。
 */
struct video_buffer {
    uint8_t *data;
    size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel; // 一行可见像素的字节数
    uint32_t line_length;    // 实际一行占用的字节数
};

/**
 * @brief 创建离屏缓冲区。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param width 可见区域宽度。
 * @param height 可见区域高度。
 * @param bits_per_pixel 每像素位数。
 * @param line_length 每行占用的字节数。
 * @return 成功返回 0，失败返回 -1。
 */
int video_buffer_create(struct video_buffer *buffer,
                        uint32_t width, uint32_t height,
                        uint32_t bits_per_pixel, uint32_t line_length);

/**
 * @brief 使用指定像素值清空离屏缓冲区。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param pixel 目标 framebuffer 格式的像素值。
 */
void video_buffer_clear(struct video_buffer *buffer, uint32_t pixel);

/**
 * @brief 在离屏缓冲区写入一个像素。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param x 目标 X 坐标。
 * @param y 目标 Y 坐标。
 * @param pixel 目标 framebuffer 格式的像素值。
 * @return 成功返回 0，坐标或参数无效返回 -1。
 */
int video_buffer_put_pixel(struct video_buffer *buffer,
                           int x, int y, uint32_t pixel);

/**
 * @brief 将离屏缓冲区刷新到目标显存。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param destination 目标显存地址。
 * @param destination_size 目标显存大小。
 * @return 成功返回 0，失败返回 -1。
 */
int video_buffer_flush(const struct video_buffer *buffer,
                       uint8_t *destination, size_t destination_size);

/**
 * @brief 释放离屏缓冲区。
 *
 * @param buffer 离屏缓冲区上下文。
 */
void video_buffer_destroy(struct video_buffer *buffer);

#endif
