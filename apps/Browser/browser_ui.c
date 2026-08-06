#include "browser_ui.h"

#include "ui_draw.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 将数值限制到闭区间。
 * @param value 输入值。
 * @param minimum 最小值。
 * @param maximum 最大值。
 * @return 限制后的值。
 */
static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

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
    row_height = (int)font->pixel_size + 18;
    available = (int)display->variable_info.yres - UI_HEADER_HEIGHT -
                UI_FOOTER_HEIGHT - UI_MARGIN;
    visible = available > 0 ? (size_t)(available / row_height) : 1;

    return visible > 0 ? visible : 1;
}

/**
 * @brief 绘制带边框和轻微阴影的面板。
 * @param display 显示设备。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param width 宽度。
 * @param height 高度。
 * @param fill 填充色。
 * @param border 边框色。
 */
void browser_ui_draw_panel(struct bmp_display *display, int x, int y,
                           int width, int height,
                           uint32_t fill, uint32_t border)
{
    if (display == NULL || width <= 0 || height <= 0) {
        return;
    }
    ui_draw_rect(display, x + 3, y + 3, width, height, UI_PANEL_SHADOW);
    ui_draw_rect(display, x, y, width, height, fill);
    ui_draw_rect(display, x, y, width, 1, border);
    ui_draw_rect(display, x, y + height - 1, width, 1, border);
    ui_draw_rect(display, x, y, 1, height, border);
    ui_draw_rect(display, x + width - 1, y, 1, height, border);
}

/**
 * @brief 绘制页面顶栏。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param title 主标题。
 * @param subtitle 右侧或次级提示，可为 NULL。
 */
void browser_ui_draw_header(struct bmp_display *display,
                            struct font_renderer *font,
                            const char *title, const char *subtitle)
{
    int width;

    if (display == NULL || font == NULL || title == NULL) {
        return;
    }
    width = (int)display->variable_info.xres;
    ui_draw_rect(display, 0, 0, width, UI_HEADER_HEIGHT, UI_HEADER);
    ui_draw_rect(display, 0, 0, 6, UI_HEADER_HEIGHT, UI_ACCENT);
    ui_draw_rect(display, 0, UI_HEADER_HEIGHT - 2, width, 2, UI_BORDER);
    ui_draw_text(display, font, title, UI_MARGIN,
                 (int)font->pixel_size + 12, width - UI_MARGIN * 2,
                 UI_TEXT, UI_HEADER);
    if (subtitle != NULL) {
        ui_draw_text(display, font, subtitle, UI_MARGIN,
                     UI_HEADER_HEIGHT - 10, width - UI_MARGIN * 2,
                     UI_MUTED, UI_HEADER);
    }
}

/**
 * @brief 绘制带返回按钮的应用顶栏。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param title 主标题。
 * @param subtitle 次级提示，可为 NULL。
 */
void browser_ui_draw_navigation_header(struct bmp_display *display,
                                       struct font_renderer *font,
                                       const char *title,
                                       const char *subtitle)
{
    int width;
    int text_x = UI_BUTTON_SIZE + 18;

    if (display == NULL || font == NULL || title == NULL) {
        return;
    }
    width = (int)display->variable_info.xres;
    ui_draw_rect(display, 0, 0, width, UI_HEADER_HEIGHT, UI_HEADER);
    ui_draw_rect(display, 0, UI_HEADER_HEIGHT - 2, width, 2, UI_BORDER);
    browser_ui_draw_back_button(display, font);
    ui_draw_text(display, font, title, text_x,
                 (int)font->pixel_size + 12, width - text_x - UI_MARGIN,
                 UI_TEXT, UI_HEADER);
    if (subtitle != NULL) {
        ui_draw_text(display, font, subtitle, text_x,
                     UI_HEADER_HEIGHT - 10, width - text_x - UI_MARGIN,
                     UI_MUTED, UI_HEADER);
    }
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
    browser_ui_draw_panel(display, 10, 10, UI_BUTTON_SIZE - 8,
                          UI_BUTTON_SIZE - 10, UI_HEADER, UI_ACCENT);
    ui_draw_text(display, font, "<", 27, (int)font->pixel_size + 10,
                 UI_BUTTON_SIZE - UI_MARGIN, UI_TEXT, UI_HEADER);
}

/**
 * @brief 绘制底部操作提示。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param hint 提示文本。
 */
void browser_ui_draw_footer_hint(struct bmp_display *display,
                                 struct font_renderer *font,
                                 const char *hint)
{
    int width;
    int y;

    if (display == NULL || font == NULL || hint == NULL) {
        return;
    }
    width = (int)display->variable_info.xres;
    y = (int)display->variable_info.yres - UI_FOOTER_HEIGHT;
    ui_draw_rect(display, 0, y, width, UI_FOOTER_HEIGHT, UI_HEADER);
    ui_draw_rect(display, 0, y, width, 1, UI_BORDER);
    ui_draw_text(display, font, hint, UI_MARGIN, y + (int)font->pixel_size,
                 width - UI_MARGIN * 2, UI_MUTED, UI_HEADER);
}

/**
 * @brief 绘制一个矩形按钮。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param width 宽度。
 * @param height 高度。
 * @param label 按钮文本。
 * @param color 按钮颜色。
 */
void browser_ui_draw_button(struct bmp_display *display,
                            struct font_renderer *font,
                            int x, int y, int width, int height,
                            const char *label, uint32_t color)
{
    if (display == NULL || font == NULL || label == NULL) {
        return;
    }
    browser_ui_draw_panel(display, x, y, width, height, color, UI_ACCENT);
    ui_draw_text(display, font, label, x + 12,
                 y + (height + (int)font->pixel_size) / 2 - 5,
                 width - 24, UI_TEXT, color);
}

/**
 * @brief 绘制水平进度条。
 * @param display 显示设备。
 * @param x 左上角 X。
 * @param y 左上角 Y。
 * @param width 宽度。
 * @param height 高度。
 * @param percent 0 到 100 的百分比。
 * @param fill 填充色。
 */
void browser_ui_draw_progress_bar(struct bmp_display *display, int x, int y,
                                  int width, int height, int percent,
                                  uint32_t fill)
{
    int filled;
    int knob_x;

    if (display == NULL || width <= 0 || height <= 0) {
        return;
    }
    percent = clamp_int(percent, 0, 100);
    filled = width * percent / 100;
    ui_draw_rect(display, x, y, width, height, UI_TRACK);
    if (filled > 0) {
        ui_draw_rect(display, x, y, filled, height, fill);
    }
    knob_x = x + clamp_int(filled - 3, 0, width - 6);
    ui_draw_rect(display, knob_x, y - 3, 6, height + 6, UI_TEXT);
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
