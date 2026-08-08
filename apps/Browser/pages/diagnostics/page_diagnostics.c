#include "page_diagnostics.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "debug_manager.h"
#include "ui_draw.h"

#include <stdio.h>

#define DIAGNOSTICS_ROW_TOP (UI_HEADER_HEIGHT + 22)
#define DIAGNOSTICS_ROW_HEIGHT 58
#define DIAGNOSTICS_ROW_GAP 4

/** @brief 按诊断行选择当前主题的标签颜色。 */
static uint32_t diagnostics_row_color(size_t row)
{
    static const unsigned int roles[] = { 0U, 1U, 2U, 1U, 3U };

    if (row >= sizeof(roles) / sizeof(roles[0])) return UI_MUTED;
    switch (roles[row]) {
    case 0U: return UI_ACCENT;
    case 1U: return UI_ACCENT_2;
    case 2U: return UI_WARNING;
    default: return UI_SELECTED_BORDER;
    }
}

/**
 * @brief 绘制一条诊断状态。
 * @param app 浏览器上下文。
 * @param row 行索引。
 * @param label 状态名称。
 * @param value 状态内容。
 * @param color 标签颜色。
 */
static void draw_diagnostics_row(struct browser_app *app, int row,
                                 const char *label, const char *value,
                                 uint32_t color)
{
    int width = (int)app->display.variable_info.xres - UI_MARGIN * 2;
    int y = DIAGNOSTICS_ROW_TOP +
            row * (DIAGNOSTICS_ROW_HEIGHT + DIAGNOSTICS_ROW_GAP);

    browser_ui_draw_panel(&app->display, UI_MARGIN, y, width,
                          DIAGNOSTICS_ROW_HEIGHT, UI_SURFACE, UI_BORDER);
    ui_draw_rect(&app->display, UI_MARGIN + 16, y + 18,
                 116, DIAGNOSTICS_ROW_HEIGHT - 36, color);
    ui_draw_text(&app->display, &app->font, label, UI_MARGIN + 28,
                 y + 18 + (int)app->font.pixel_size - 3,
                 92, UI_BACKGROUND, color);
    ui_draw_text(&app->display, &app->font, value, UI_MARGIN + 154,
                 y + (DIAGNOSTICS_ROW_HEIGHT +
                      (int)app->font.pixel_size) / 2 - 4,
                 width - 174, UI_TEXT, UI_SURFACE);
}

/**
 * @brief 绘制设备与运行环境诊断页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_diagnostics_page(struct browser_app *app)
{
    size_t index;

    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_navigation_header(&app->display, &app->font,
                                      "Diagnostics",
                                      "Runtime devices and support tools");
    for (index = 0; index < app->debug.count; index++) {
        const struct debug_operation *operation =
            debug_manager_at(&app->debug, index);
        char value[180];

        if (operation == NULL) {
            continue;
        }
        if (operation->status(app, value, sizeof(value)) < 0) {
            snprintf(value, sizeof(value), "unavailable");
        }
        draw_diagnostics_row(app, (int)index, operation->name, value,
                             diagnostics_row_color(index));
    }
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Esc or top-left button returns to desktop");
    return bmp_display_flush(&app->display);
}

/**
 * @brief 处理诊断页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0。
 */
int handle_diagnostics_key(struct browser_app *app,
                           enum input_action action)
{
    (void)app;
    (void)action;
    return 0;
}

/**
 * @brief 处理诊断页面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0。
 */
int handle_diagnostics_touch(struct browser_app *app,
                             const struct browser_input *input)
{
    (void)app;
    (void)input;
    return 0;
}
