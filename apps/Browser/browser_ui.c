#include "browser_ui.h"

#include "ui_draw.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 计算文件列表页面可显示的行数。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @return 至少为 1 的可见行数。
 */
size_t browser_ui_visible_rows(const struct bmp_display *display,
                               const struct font_renderer *font)
{
    int row_height;
    int available;
    size_t visible;

    if (display == NULL || font == NULL) {
        return 1;
    }
    row_height = (int)font->pixel_size + 14;
    available = (int)display->variable_info.yres - UI_HEADER_HEIGHT;
    visible = available > 0 ? (size_t)(available / row_height) : 1;

    return visible > 0 ? visible : 1;
}

/**
 * @brief 绘制媒体页面左上角返回按钮。
 * @param display 显示设备。
 * @param font 字体上下文。
 */
void browser_ui_draw_back_button(struct bmp_display *display,
                                 struct font_renderer *font)
{
    if (display == NULL || font == NULL) {
        return;
    }
    ui_draw_rect(display, 0, 0, UI_BUTTON_SIZE, UI_BUTTON_SIZE, UI_HEADER);
    ui_draw_text(display, font, "<", UI_MARGIN,
                 (int)font->pixel_size + 12, UI_BUTTON_SIZE - UI_MARGIN,
                 UI_TEXT, UI_HEADER);
}

/**
 * @brief 将毫秒时间格式化为 MM:SS。
 * @param milliseconds 时间毫秒值。
 * @param output 输出缓冲区。
 * @param output_size 输出缓冲区大小。
 */
void browser_ui_format_time(uint64_t milliseconds, char *output,
                            size_t output_size)
{
    uint64_t seconds = milliseconds / 1000U;

    if (output == NULL || output_size == 0) {
        return;
    }
    snprintf(output, output_size, "%02llu:%02llu",
             (unsigned long long)(seconds / 60U),
             (unsigned long long)(seconds % 60U));
}

/**
 * @brief 将屏幕 X 坐标转换为水平条控件百分比。
 * @param display 显示设备。
 * @param x 屏幕 X 坐标。
 * @return 0 到 100 的百分比。
 */
int browser_ui_bar_percent(const struct bmp_display *display, int x)
{
    int width;
    int percent;

    if (display == NULL) {
        return 0;
    }
    width = (int)display->variable_info.xres - UI_MARGIN * 2;
    if (width <= 0) {
        return 0;
    }
    percent = (x - UI_MARGIN) * 100 / width;

    if (percent < 0) {
        return 0;
    }
    return percent > 100 ? 100 : percent;
}

/**
 * @brief 判断触摸手势是否命中指定水平条。
 * @param input 触摸输入。
 * @param y 条形控件 Y 坐标。
 * @return 命中返回 1，否则返回 0。
 */
int browser_ui_touches_bar(const struct browser_input *input, int y)
{
    if (input == NULL) {
        return 0;
    }
    return abs(input->start_y - y) <= 28 || abs(input->y - y) <= 28;
}
