#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdint.h>

#include "font_renderer.h"

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
                  int width, int height, uint32_t color);

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
                 uint32_t foreground, uint32_t background);

#endif
