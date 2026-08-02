#include "image_render.h"

#include <errno.h>
#include <stdint.h>

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
    uint32_t rotated_width;
    uint32_t rotated_height;
    uint32_t target_width;
    uint32_t target_height;
    uint32_t start_x;
    uint32_t start_y;
    uint32_t target_y;

    if (display == NULL || image == NULL || image->pixels == NULL ||
        image->width == 0 || image->height == 0 ||
        image->channels != 3 ||
        image->line_length < (size_t)image->width * image->channels ||
        image->size / image->line_length < image->height ||
        display->variable_info.xres == 0 ||
        display->variable_info.yres == 0 ||
        (rotation != 0 && rotation != 90 && rotation != 180 &&
         rotation != 270)) {
        errno = EINVAL;
        return -1;
    }

    rotated_width = rotation == 90 || rotation == 270 ?
                    image->height : image->width;
    rotated_height = rotation == 90 || rotation == 270 ?
                     image->width : image->height;
    target_width = display->variable_info.xres;
    target_height = (uint32_t)((uint64_t)rotated_height * target_width /
                               rotated_width);

    if (target_height > display->variable_info.yres) {
        target_height = display->variable_info.yres;
        target_width = (uint32_t)((uint64_t)rotated_width * target_height /
                                  rotated_height);
    }

    if (target_width == 0) {
        target_width = 1;
    }
    if (target_height == 0) {
        target_height = 1;
    }

    start_x = (display->variable_info.xres - target_width) / 2;
    start_y = (display->variable_info.yres - target_height) / 2;
    bmp_display_clear(display, 0, 0, 0);

    for (target_y = 0; target_y < target_height; target_y++) {
        uint32_t rotated_y = (uint32_t)((uint64_t)target_y * rotated_height /
                                        target_height);
        uint32_t target_x;

        for (target_x = 0; target_x < target_width; target_x++) {
            uint32_t rotated_x = (uint32_t)(
                (uint64_t)target_x * rotated_width / target_width);
            uint32_t source_x;
            uint32_t source_y;

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
                                    (int)(start_x + target_x),
                                    (int)(start_y + target_y),
                                    pixel[0], pixel[1], pixel[2]) < 0) {
                return -1;
            }
        }
    }

    return bmp_display_flush(display);
}
