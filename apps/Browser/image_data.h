#ifndef IMAGE_DATA_H
#define IMAGE_DATA_H

#include <stddef.h>
#include <stdint.h>

/** @brief 解码后的 RGB888 图片。 */
struct image_data {
    uint8_t *pixels;
    size_t size;
    size_t line_length;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
};

/**
 * @brief 为 RGB888 图片分配内存。
 * @param image 必须是已清零且尚未分配内存的图片对象。
 * @param width 图片宽度。
 * @param height 图片高度。
 * @return 成功返回 0，失败返回 -1。
 */
int image_data_create(struct image_data *image,
                    uint32_t width, uint32_t height);

/**
 * @brief 释放图片像素并清空结构体。
 * @param image 图片对象。
 */
void image_data_destroy(struct image_data *image);

#endif