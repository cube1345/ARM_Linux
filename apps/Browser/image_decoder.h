#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include "file_list.h"
#include "image_data.h"

/**
 * @brief 按文件类型调用对应图片解码器。
 *
 * @param path 图片路径。
 * @param type 图片文件类型。
 * @param image 输出 RGB888 图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decode(const char *path, enum file_type type,
                 struct image_data *image);

#endif
