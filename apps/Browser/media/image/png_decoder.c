#include "png_decoder.h"

#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 将 libpng 输出的一行 RGBA 像素合成到黑色背景。
 *
 * @param source RGBA8888 源像素。
 * @param destination RGB888 目标像素。
 * @param width 行宽。
 */
static void composite_row(const uint8_t *source, uint8_t *destination,
                          uint32_t width)
{
    uint32_t x;

    for (x = 0; x < width; x++) {
        unsigned int alpha = source[(size_t)x * 4U + 3U];
        unsigned int channel;

        for (channel = 0; channel < 3; channel++) {
            destination[(size_t)x * 3U + channel] =
                (uint8_t)((source[(size_t)x * 4U + channel] * alpha + 127U) /
                          255U);
        }
    }
}

/**
 * @brief 将 PNG 文件解码为顶行优先的 RGB888 图片。
 *
 * @param path PNG 文件路径。
 * @param image 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int png_decode(const char *path, struct image_data *image)
{
    png_image png;
    png_bytep pixels = NULL;
    png_uint_32 y;

    if (path == NULL || image == NULL || image->pixels != NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_file(&png, path) == 0 ||
        png.width == 0 || png.height == 0) {
        errno = EINVAL;
        png_image_free(&png);
        return -1;
    }
    png.format = PNG_FORMAT_RGBA;
    pixels = malloc(PNG_IMAGE_SIZE(png));
    if (pixels == NULL ||
        png_image_finish_read(&png, NULL, pixels, 0, NULL) == 0 ||
        image_data_create(image, png.width, png.height) < 0) {
        free(pixels);
        png_image_free(&png);
        return -1;
    }
    for (y = 0; y < png.height; y++) {
        composite_row(pixels + (size_t)y * png.width * 4U,
                      image->pixels + (size_t)y * image->line_length,
                      png.width);
    }
    free(pixels);
    png_image_free(&png);
    return 0;
}
