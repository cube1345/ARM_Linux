#include "png_decoder.h"

#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep row = NULL;
    FILE *file = NULL;
    png_uint_32 width;
    png_uint_32 height;
    int color_type;
    int bit_depth;
    int result = -1;
    png_uint_32 y;

    if (path == NULL || image == NULL || image->pixels != NULL) {
        errno = EINVAL;
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return -1;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png != NULL ? png_create_info_struct(png) : NULL;
    if (png == NULL || info == NULL) {
        errno = ENOMEM;
        goto cleanup;
    }
    if (setjmp(png_jmpbuf(png)) != 0) {
        errno = EINVAL;
        goto cleanup;
    }
    png_init_io(png, file);
    png_read_info(png, info);
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);
    if (width == 0 || height == 0) {
        errno = EINVAL;
        goto cleanup;
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0 &&
        png_get_valid(png, info, PNG_INFO_tRNS) == 0) {
        png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    }
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    if (png_get_channels(png, info) != 4 ||
        png_get_rowbytes(png, info) < (png_size_t)width * 4U ||
        image_data_create(image, width, height) < 0) {
        goto cleanup;
    }
    row = malloc(png_get_rowbytes(png, info));
    if (row == NULL) {
        goto cleanup;
    }
    for (y = 0; y < height; y++) {
        png_read_row(png, row, NULL);
        composite_row(row, image->pixels + (size_t)y * image->line_length,
                      width);
    }
    png_read_end(png, NULL);
    result = 0;

cleanup:
    free(row);
    if (png != NULL) {
        png_destroy_read_struct(&png, info != NULL ? &info : NULL, NULL);
    }
    if (file != NULL) {
        fclose(file);
    }
    if (result < 0) {
        image_data_destroy(image);
    }
    return result;
}
