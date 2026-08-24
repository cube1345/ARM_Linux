#ifndef JPEG_DECODER_H
#define JPEG_DECODER_H

#include "image_data.h"

/**
 * @brief 将 JPEG 文件解码成 RGB888 图片。
 *
 * @param path JPEG 文件路径。
 * @param image 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int jpeg_decode(const char *path, struct image_data *image);

#endif
