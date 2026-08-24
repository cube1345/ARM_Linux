#include "image_data.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 为 RGB888 图片分配内存。
 * @param image 必须是已清零且尚未分配内存的图片对象。
 * @param width 图片宽度。
 * @param height 图片高度。
 * @return 成功返回 0，失败返回 -1。
 */
int image_data_create(struct image_data *image,
                    uint32_t width, uint32_t height)
{
    const size_t channels = 3;
    size_t line_length;
    size_t size;
    uint8_t *pixels;

    if (image == NULL || image->pixels != NULL ||
        width == 0 || height == 0) {
        errno = EINVAL;
        return -1;
    }

    if ((size_t)width > SIZE_MAX / channels) {
        errno = EOVERFLOW;
        return -1;
    }

    line_length = (size_t)width * channels;
    if ((size_t)height > SIZE_MAX / line_length) {
        errno = EOVERFLOW;
        return -1;
    }

    size = line_length * height;
    pixels = malloc(size);
    if (pixels == NULL)
        return -1;

    image->pixels = pixels;
    image->size = size;
    image->line_length = line_length;
    image->width = width;
    image->height = height;
    image->channels = channels;
    return 0;
}

/**
 * @brief 释放图片像素并清空结构体。
 * @param image 图片对象。
 */
void image_data_destroy(struct image_data *image)
{
    if (image == NULL)
        return;

    free(image->pixels);
    memset(image, 0, sizeof(*image));
}