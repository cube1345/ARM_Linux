#include "image_render.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 4U
#define SCREEN_HEIGHT 4U
#define SCREEN_CHANNELS 3U

/** @brief 清空测试用 RGB888 framebuffer。 */
void bmp_display_clear(struct bmp_display *display,
                       uint8_t red, uint8_t green, uint8_t blue)
{
    size_t index;

    for (index = 0; index < SCREEN_WIDTH * SCREEN_HEIGHT; index++) {
        display->memory[index * SCREEN_CHANNELS] = red;
        display->memory[index * SCREEN_CHANNELS + 1U] = green;
        display->memory[index * SCREEN_CHANNELS + 2U] = blue;
    }
}

/** @brief 写入测试用 RGB888 framebuffer 像素。 */
int bmp_display_put_rgb(struct bmp_display *display, int x, int y,
                        uint8_t red, uint8_t green, uint8_t blue)
{
    size_t offset;

    if (x < 0 || y < 0 || x >= (int)SCREEN_WIDTH ||
        y >= (int)SCREEN_HEIGHT) {
        errno = EINVAL;
        return -1;
    }
    offset = ((size_t)y * SCREEN_WIDTH + (size_t)x) * SCREEN_CHANNELS;
    display->memory[offset] = red;
    display->memory[offset + 1U] = green;
    display->memory[offset + 2U] = blue;
    return 0;
}

/** @brief 测试 framebuffer 无需真实 flush。 */
int bmp_display_flush(struct bmp_display *display)
{
    return display != NULL ? 0 : -1;
}

/** @brief 判断指定位置是否为预期 RGB 颜色。 */
static int pixel_is(const struct bmp_display *display, unsigned int x,
                    unsigned int y, uint8_t red, uint8_t green, uint8_t blue)
{
    size_t offset = ((size_t)y * SCREEN_WIDTH + x) * SCREEN_CHANNELS;

    return display->memory[offset] == red &&
           display->memory[offset + 1U] == green &&
           display->memory[offset + 2U] == blue;
}

/** @brief 验证 FIT、FILL 和 ORIGINAL 三种画面缩放模式。 */
int main(void)
{
    struct bmp_display display = {0};
    struct image_data image = {0};
    uint8_t screen[SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_CHANNELS];

    display.memory = screen;
    display.variable_info.xres = SCREEN_WIDTH;
    display.variable_info.yres = SCREEN_HEIGHT;
    if (image_data_create(&image, 2, 1) < 0) return EXIT_FAILURE;
    image.pixels[0] = 255;
    image.pixels[1] = 0;
    image.pixels[2] = 0;
    image.pixels[3] = 0;
    image.pixels[4] = 255;
    image.pixels[5] = 0;

    if (image_render_draw_mode(&display, &image, 0, IMAGE_RENDER_FIT) < 0 ||
        !pixel_is(&display, 0, 0, 0, 0, 0) ||
        !pixel_is(&display, 0, 1, 255, 0, 0) ||
        !pixel_is(&display, 3, 2, 0, 255, 0) ||
        image_render_draw_mode(&display, &image, 0, IMAGE_RENDER_FILL) < 0 ||
        !pixel_is(&display, 0, 0, 255, 0, 0) ||
        !pixel_is(&display, 3, 3, 0, 255, 0) ||
        image_render_draw_mode(&display, &image, 0,
                               IMAGE_RENDER_ORIGINAL) < 0 ||
        !pixel_is(&display, 0, 1, 0, 0, 0) ||
        !pixel_is(&display, 1, 1, 255, 0, 0) ||
        !pixel_is(&display, 2, 1, 0, 255, 0) ||
        !pixel_is(&display, 1, 2, 0, 0, 0) ||
        image_render_draw(&display, &image, 90) < 0 ||
        !pixel_is(&display, 1, 0, 255, 0, 0) ||
        !pixel_is(&display, 2, 3, 0, 255, 0)) {
        fprintf(stderr, "FAIL image render modes\n");
        image_data_destroy(&image);
        return EXIT_FAILURE;
    }
    image_data_destroy(&image);
    printf("PASS image render modes\n");
    return EXIT_SUCCESS;
}
