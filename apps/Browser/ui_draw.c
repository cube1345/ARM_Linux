#include "ui_draw.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief 绘制纯色矩形。
 *
 * @param display 显示设备。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param width 宽度。
 * @param height 高度。
 * @param color RGB888 颜色。
 */
void ui_draw_rect(struct bmp_display *display, int x, int y,
                  int width, int height, uint32_t color)
{
    int row;

    if (display == NULL || width <= 0 || height <= 0) {
        return;
    }
    for (row = 0; row < height; row++) {
        int column;

        for (column = 0; column < width; column++) {
            bmp_display_put_rgb(display, x + column, y + row,
                                (uint8_t)(color >> 16),
                                (uint8_t)(color >> 8),
                                (uint8_t)color);
        }
    }
}

/**
 * @brief 绘制单行 UTF-8 文本并按宽度裁剪。
 *
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param text UTF-8 文本。
 * @param x 起始 X。
 * @param baseline_y 基线 Y。
 * @param max_width 最大宽度。
 * @param foreground 前景 RGB888。
 * @param background 背景 RGB888。
 * @return 实际绘制宽度。
 */
int ui_draw_text(struct bmp_display *display, struct font_renderer *font,
                 const char *text, int x, int baseline_y, int max_width,
                 uint32_t foreground, uint32_t background)
{
    size_t offset = 0;
    size_t length;
    int cursor = x;

    if (display == NULL || font == NULL || text == NULL || max_width <= 0) {
        return 0;
    }
    length = strlen(text);
    while (offset < length) {
        uint32_t codepoint;
        size_t consumed = font_renderer_decode_utf8(
            (const uint8_t *)text + offset, length - offset, &codepoint);
        int advance;

        if (consumed == 0 || codepoint == '\n' || codepoint == '\r') {
            break;
        }
        if (font_renderer_measure_codepoint(font, codepoint, &advance) < 0 ||
            cursor + advance > x + max_width) {
            break;
        }
        if (font_renderer_draw_codepoint(font, display, codepoint,
                                         cursor, baseline_y,
                                         foreground, background,
                                         &advance) < 0) {
            break;
        }
        cursor += advance;
        offset += consumed;
    }
    return cursor - x;
}
