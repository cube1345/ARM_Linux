#include "image_decoder.h"

#include "bmp_decoder.h"
#include "jpeg_decoder.h"
#include "png_decoder.h"

#include <errno.h>

/**
 * @brief 按文件类型调用对应图片解码器。
 *
 * @param path 图片路径。
 * @param type 图片文件类型。
 * @param image 输出 RGB888 图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int image_decode(const char *path, enum file_type type,
                 struct image_data *image)
{
    if (path == NULL || image == NULL) {
        errno = EINVAL;
        return -1;
    }
    switch (type) {
    case FILE_TYPE_BMP:
        return bmp_decode(path, image);
    case FILE_TYPE_JPEG:
        return jpeg_decode(path, image);
    case FILE_TYPE_PNG:
        return png_decode(path, image);
    default:
        errno = ENOTSUP;
        return -1;
    }
}
