#ifndef BROWSER_UI_H
#define BROWSER_UI_H

#include "bmp_display.h"
#include "font_renderer.h"
#include "input_keyboard.h"

#include <stddef.h>
#include <stdint.h>

#define UI_BACKGROUND 0x0f141aU
#define UI_HEADER 0x18212bU
#define UI_SURFACE 0x17202aU
#define UI_SURFACE_ALT 0x202b36U
#define UI_PANEL_SHADOW 0x0a0d11U
#define UI_BORDER 0x2b3a46U
#define UI_SELECTED 0x254f5aU
#define UI_SELECTED_BORDER 0x6ee7c8U
#define UI_TEXT 0xf2f5f7U
#define UI_MUTED 0xaab5bdU
#define UI_ACCENT 0x4fc3a1U
#define UI_ACCENT_2 0x7aa2ffU
#define UI_WARNING 0xffc857U
#define UI_TRACK 0x3c464eU
#define UI_MARGIN 20
#define UI_HEADER_HEIGHT 64
#define UI_FOOTER_HEIGHT 36
#define UI_BUTTON_SIZE 54
#define UI_AUDIO_REFRESH_MS 100

/**
 * @brief 计算文件列表页面可显示的行数。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @return 至少为 1 的可见行数。
 */
size_t browser_ui_visible_rows(const struct bmp_display *display,
                               const struct font_renderer *font);

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
                           uint32_t fill, uint32_t border);

/**
 * @brief 绘制页面顶栏。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param title 主标题。
 * @param subtitle 右侧或次级提示，可为 NULL。
 */
void browser_ui_draw_header(struct bmp_display *display,
                            struct font_renderer *font,
                            const char *title, const char *subtitle);

/**
 * @brief 绘制媒体页面左上角返回按钮。
 * @param display 显示设备。
 * @param font 字体上下文。
 */
void browser_ui_draw_back_button(struct bmp_display *display,
                                 struct font_renderer *font);

/**
 * @brief 绘制底部操作提示。
 * @param display 显示设备。
 * @param font 字体上下文。
 * @param hint 提示文本。
 */
void browser_ui_draw_footer_hint(struct bmp_display *display,
                                 struct font_renderer *font,
                                 const char *hint);

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
                            const char *label, uint32_t color);

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
                                  uint32_t fill);

/**
 * @brief 将毫秒时间格式化为 MM:SS。
 * @param milliseconds 时间毫秒值。
 * @param output 输出缓冲区。
 * @param output_size 输出缓冲区大小。
 */
void browser_ui_format_time(uint64_t milliseconds, char *output,
                            size_t output_size);

/**
 * @brief 将屏幕 X 坐标转换为水平条控件百分比。
 * @param display 显示设备。
 * @param x 屏幕 X 坐标。
 * @return 0 到 100 的百分比。
 */
int browser_ui_bar_percent(const struct bmp_display *display, int x);

/**
 * @brief 判断触摸手势是否命中指定水平条。
 * @param input 触摸输入。
 * @param y 条形控件 Y 坐标。
 * @return 命中返回 1，否则返回 0。
 */
int browser_ui_touches_bar(const struct browser_input *input, int y);

#endif
