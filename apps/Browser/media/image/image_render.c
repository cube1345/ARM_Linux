#include "image_render.h"

#include <errno.h>
#include <stdint.h>

/** @brief 校验 RGB888 图片结构是否可安全读取。 */
static int image_is_valid(const struct image_data *image)
{
    return image != NULL && image->pixels != NULL && image->width > 0 &&
           image->height > 0 && image->channels == 3 &&
           image->line_length >= (size_t)image->width * image->channels &&
           image->size / image->line_length >= image->height;
}

/** @brief 等比例缩放 RGB888 图片到指定最大尺寸。 */
int image_render_scale_fit(const struct image_data *source,
                           uint32_t maximum_width,
                           uint32_t maximum_height,
                           struct image_data *output)
{
    uint32_t width;
    uint32_t height;
    uint32_t y;

    if (!image_is_valid(source) || output == NULL ||
        output->pixels != NULL || maximum_width == 0 || maximum_height == 0) {
        errno = EINVAL;
        return -1;
    }
    width = maximum_width;
    height = (uint32_t)((uint64_t)source->height * width / source->width);
    if (height > maximum_height) {
        height = maximum_height;
        width = (uint32_t)((uint64_t)source->width * height / source->height);
    }
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    if (image_data_create(output, width, height) < 0) return -1;
    for (y = 0; y < height; y++) {
        uint32_t source_y = (uint32_t)((uint64_t)y * source->height / height);
        uint32_t x;

        for (x = 0; x < width; x++) {
            uint32_t source_x = (uint32_t)((uint64_t)x * source->width /
                                           width);
            const uint8_t *input = source->pixels +
                (size_t)source_y * source->line_length +
                (size_t)source_x * source->channels;
            uint8_t *pixel = output->pixels +
                (size_t)y * output->line_length + (size_t)x * 3U;

            pixel[0] = input[0];
            pixel[1] = input[1];
            pixel[2] = input[2];
        }
    }
    return 0;
}

/** @brief 将 RGB888 图片居中绘制到指定矩形，不执行 flush。 */
int image_render_draw_region(struct bmp_display *display,
                             const struct image_data *image,
                             int x, int y, int width, int height,
                             uint32_t background)
{
    int start_x;
    int start_y;
    int row;

    if (display == NULL || !image_is_valid(image) || width <= 0 ||
        height <= 0 || image->width > (uint32_t)width ||
        image->height > (uint32_t)height) {
        errno = EINVAL;
        return -1;
    }
    for (row = 0; row < height; row++) {
        int column;

        for (column = 0; column < width; column++) {
            if (bmp_display_put_rgb(display, x + column, y + row,
                                    (uint8_t)(background >> 16),
                                    (uint8_t)(background >> 8),
                                    (uint8_t)background) < 0) {
                return -1;
            }
        }
    }
    start_x = x + (width - (int)image->width) / 2;
    start_y = y + (height - (int)image->height) / 2;
    for (row = 0; row < (int)image->height; row++) {
        uint32_t column;

        for (column = 0; column < image->width; column++) {
            const uint8_t *pixel = image->pixels +
                (size_t)row * image->line_length + (size_t)column * 3U;

            if (bmp_display_put_rgb(display, start_x + (int)column,
                                    start_y + (int)row,
                                    pixel[0], pixel[1], pixel[2]) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

/**
 * @brief 等比例缩放并居中绘制 RGB888 图片。
 *
 * @param display 显示设备上下文。
 * @param image 已解码的 RGB888 图片。
 * @param rotation 顺时针旋转角度，必须是 0、90、180 或 270。
 * @return 成功返回 0，失败返回 -1。
 */
int image_render_draw(struct bmp_display *display,
                      const struct image_data *image, unsigned int rotation)
{
    return image_render_draw_mode(display, image, rotation,
                                  IMAGE_RENDER_FIT);
}

/**
 * @brief 按指定缩放策略等比例居中绘制 RGB888 图片。
 * @param display 显示设备上下文。
 * @param image 已解码的 RGB888 图片。
 * @param rotation 顺时针旋转角度，必须是 0、90、180 或 270。
 * @param mode FIT、FILL 或 ORIGINAL 缩放策略。
 * @return 成功返回 0，失败返回 -1。
 */
int image_render_draw_mode(struct bmp_display *display,
                           const struct image_data *image,
                           unsigned int rotation,
                           enum image_render_mode mode)
{
    uint32_t rotated_width;
    uint32_t rotated_height;
    uint64_t target_width;
    uint64_t target_height;
    int64_t start_x;
    int64_t start_y;
    uint32_t screen_y;

    if (display == NULL || !image_is_valid(image) ||
        display->variable_info.xres == 0 ||
        display->variable_info.yres == 0 ||
        (rotation != 0 && rotation != 90 && rotation != 180 &&
         rotation != 270) || mode < IMAGE_RENDER_FIT ||
        mode > IMAGE_RENDER_ORIGINAL) {
        errno = EINVAL;
        return -1;
    }

    rotated_width = rotation == 90 || rotation == 270 ?
                    image->height : image->width;
    rotated_height = rotation == 90 || rotation == 270 ?
                     image->width : image->height;
    if (mode == IMAGE_RENDER_ORIGINAL) {
        target_width = rotated_width;
        target_height = rotated_height;
    } else {
        target_width = display->variable_info.xres;
        target_height = (uint64_t)rotated_height * target_width /
                        rotated_width;
        if ((mode == IMAGE_RENDER_FIT &&
             target_height > display->variable_info.yres) ||
            (mode == IMAGE_RENDER_FILL &&
             target_height < display->variable_info.yres)) {
            target_height = display->variable_info.yres;
            target_width = (uint64_t)rotated_width * target_height /
                           rotated_height;
        }
    }
    if (target_width == 0) target_width = 1;
    if (target_height == 0) target_height = 1;
    start_x = ((int64_t)display->variable_info.xres -
               (int64_t)target_width) / 2;
    start_y = ((int64_t)display->variable_info.yres -
               (int64_t)target_height) / 2;
    bmp_display_clear(display, 0, 0, 0);

    for (screen_y = 0; screen_y < display->variable_info.yres; screen_y++) {
        int64_t relative_y = (int64_t)screen_y - start_y;
        uint32_t rotated_y;
        uint32_t screen_x;

        if (relative_y < 0 || (uint64_t)relative_y >= target_height) continue;
        rotated_y = (uint32_t)((uint64_t)relative_y * rotated_height /
                               target_height);
        for (screen_x = 0; screen_x < display->variable_info.xres;
             screen_x++) {
            int64_t relative_x = (int64_t)screen_x - start_x;
            uint32_t rotated_x;
            uint32_t source_x;
            uint32_t source_y;

            if (relative_x < 0 ||
                (uint64_t)relative_x >= target_width) continue;
            rotated_x = (uint32_t)((uint64_t)relative_x * rotated_width /
                                   target_width);

            if (rotation == 90) {
                source_x = rotated_y;
                source_y = image->height - 1U - rotated_x;
            } else if (rotation == 180) {
                source_x = image->width - 1U - rotated_x;
                source_y = image->height - 1U - rotated_y;
            } else if (rotation == 270) {
                source_x = image->width - 1U - rotated_y;
                source_y = rotated_x;
            } else {
                source_x = rotated_x;
                source_y = rotated_y;
            }
            const uint8_t *pixel = image->pixels +
                                   (size_t)source_y * image->line_length +
                                   (size_t)source_x * image->channels;

            if (bmp_display_put_rgb(display,
                                    (int)screen_x, (int)screen_y,
                                    pixel[0], pixel[1], pixel[2]) < 0) {
                return -1;
            }
        }
    }

    return bmp_display_flush(display);
}
