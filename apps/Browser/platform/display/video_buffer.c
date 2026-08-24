#include "video_buffer.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 获取离屏缓冲区的每像素字节数。
 *
 * @param buffer 离屏缓冲区上下文。
 * @return 每像素字节数。
 */
static size_t video_buffer_bytes_per_pixel(const struct video_buffer *buffer)
{
    return buffer->bits_per_pixel / 8;
}

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
                        uint32_t bits_per_pixel, uint32_t line_length)
{
    size_t bytes_per_pixel;
    size_t minimum_line_length;
    size_t buffer_size;

    if (buffer == NULL || width == 0 || height == 0 ||
        bits_per_pixel % 8 != 0) {
        errno = EINVAL;
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));
    bytes_per_pixel = bits_per_pixel / 8;
    if (bytes_per_pixel != 2 && bytes_per_pixel != 3 &&
        bytes_per_pixel != 4) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)width > SIZE_MAX / bytes_per_pixel) {
        errno = EOVERFLOW;
        return -1;
    }

    minimum_line_length = (size_t)width * bytes_per_pixel;
    if ((size_t)line_length < minimum_line_length) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)height > SIZE_MAX / line_length) {
        errno = EOVERFLOW;
        return -1;
    }

    buffer_size = (size_t)line_length * height;
    buffer->data = calloc(1, buffer_size);
    if (buffer->data == NULL) {
        return -1;
    }

    buffer->size = buffer_size;
    buffer->width = width;
    buffer->height = height;
    buffer->bits_per_pixel = bits_per_pixel;
    buffer->line_length = line_length;
    return 0;
}

/**
 * @brief 使用指定像素值清空离屏缓冲区。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param pixel 目标 framebuffer 格式的像素值。
 */
void video_buffer_clear(struct video_buffer *buffer, uint32_t pixel)
{
    size_t bytes_per_pixel;
    uint32_t y;

    if (buffer == NULL || buffer->data == NULL) {
        return;
    }

    if (pixel == 0) {
        memset(buffer->data, 0, buffer->size);
        return;
    }

    bytes_per_pixel = video_buffer_bytes_per_pixel(buffer);
    memset(buffer->data, 0, buffer->size);

    for (y = 0; y < buffer->height; y++) {
        uint32_t x;
        uint8_t *row = buffer->data + (size_t)y * buffer->line_length;

        for (x = 0; x < buffer->width; x++) {
            memcpy(row + (size_t)x * bytes_per_pixel,
                   &pixel, bytes_per_pixel);
        }
    }
}

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
                           int x, int y, uint32_t pixel)
{
    size_t bytes_per_pixel;
    size_t offset;

    if (buffer == NULL || buffer->data == NULL || x < 0 || y < 0 ||
        (uint32_t)x >= buffer->width || (uint32_t)y >= buffer->height) {
        errno = EINVAL;
        return -1;
    }

    bytes_per_pixel = video_buffer_bytes_per_pixel(buffer);
    offset = (size_t)y * buffer->line_length +
             (size_t)x * bytes_per_pixel;

    if (offset > buffer->size ||
        bytes_per_pixel > buffer->size - offset) {
        errno = EOVERFLOW;
        return -1;
    }

    memcpy(buffer->data + offset, &pixel, bytes_per_pixel);
    return 0;
}

/**
 * @brief 将离屏缓冲区刷新到目标显存。
 *
 * @param buffer 离屏缓冲区上下文。
 * @param destination 目标显存地址。
 * @param destination_size 目标显存大小。
 * @return 成功返回 0，失败返回 -1。
 */
int video_buffer_flush(const struct video_buffer *buffer,
                       uint8_t *destination, size_t destination_size)
{
    if (buffer == NULL || buffer->data == NULL || destination == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (destination_size < buffer->size) {
        errno = ENOSPC;
        return -1;
    }

    memcpy(destination, buffer->data, buffer->size);
    return 0;
}

/**
 * @brief 释放离屏缓冲区。
 *
 * @param buffer 离屏缓冲区上下文。
 */
void video_buffer_destroy(struct video_buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}
