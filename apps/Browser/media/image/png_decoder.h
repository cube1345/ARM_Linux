#ifndef PNG_DECODER_H
#define PNG_DECODER_H

#include "image_data.h"

/**
 * @brief 将 PNG 文件解码为顶行优先的 RGB888 图片。
 *
 * @param path PNG 文件路径。
 * @param image 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int png_decode(const char *path, struct image_data *image);

#endif
