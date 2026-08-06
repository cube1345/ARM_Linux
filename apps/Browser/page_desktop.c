#include "page_desktop.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "desktop_app.h"
#include "ui_draw.h"

#include <stddef.h>

#define DESKTOP_GRID_GAP 18
#define DESKTOP_GRID_TOP (UI_HEADER_HEIGHT + 22)

/** @brief 桌面应用卡片坐标。 */
struct desktop_card_bounds {
    int x;
    int y;
    int width;
    int height;
};

/**
 * @brief 根据 framebuffer 宽度选择桌面列数。
 * @param app 浏览器上下文。
 * @return 桌面列数。
 */
static size_t desktop_columns(const struct browser_app *app)
{
    return app->display.variable_info.xres >= 720U ? 3U : 2U;
}

/**
 * @brief 计算指定应用卡片坐标。
 * @param app 浏览器上下文。
 * @param index 应用索引。
 * @param bounds 输出卡片坐标。
 */
static void desktop_card_bounds_at(const struct browser_app *app,
                                   size_t index,
                                   struct desktop_card_bounds *bounds)
{
    size_t columns = desktop_columns(app);
    size_t rows = (app->desktop_apps.count + columns - 1U) / columns;
    int screen_width = (int)app->display.variable_info.xres;
    int screen_height = (int)app->display.variable_info.yres;
    int available_width = screen_width - UI_MARGIN * 2 -
                          (int)(columns - 1U) * DESKTOP_GRID_GAP;
    int available_height = screen_height - DESKTOP_GRID_TOP -
                           UI_FOOTER_HEIGHT - UI_MARGIN -
                           (int)(rows - 1U) * DESKTOP_GRID_GAP;

    bounds->width = available_width / (int)columns;
    bounds->height = available_height / (int)rows;
    bounds->x = UI_MARGIN +
                (int)(index % columns) *
                (bounds->width + DESKTOP_GRID_GAP);
    bounds->y = DESKTOP_GRID_TOP +
                (int)(index / columns) *
                (bounds->height + DESKTOP_GRID_GAP);
}

/**
 * @brief 绘制一个桌面应用卡片。
 * @param app 浏览器上下文。
 * @param operation 桌面应用 operation。
 * @param index 应用索引。
 */
static void draw_desktop_application(
    struct browser_app *app,
    const struct desktop_app_operation *operation, size_t index)
{
    struct desktop_card_bounds bounds;
    uint32_t background = index == app->desktop_selected ?
                          UI_SELECTED :
                          (index % 2U == 0U ? UI_SURFACE : UI_SURFACE_ALT);
    uint32_t border = index == app->desktop_selected ?
                      UI_SELECTED_BORDER : UI_BORDER;
    int badge_size;
    int text_x;
    int title_y;
    int summary_y;

    desktop_card_bounds_at(app, index, &bounds);
    browser_ui_draw_panel(&app->display, bounds.x, bounds.y,
                          bounds.width, bounds.height, background, border);
    ui_draw_rect(&app->display, bounds.x, bounds.y, 7,
                 bounds.height, operation->color);
    badge_size = bounds.height - 34;
    if (badge_size > 70) {
        badge_size = 70;
    }
    if (badge_size < 42) {
        badge_size = 42;
    }
    ui_draw_rect(&app->display, bounds.x + 22,
                 bounds.y + (bounds.height - badge_size) / 2,
                 badge_size, badge_size, operation->color);
    ui_draw_text(&app->display, &app->font, operation->badge,
                 bounds.x + 31,
                 bounds.y + (bounds.height + (int)app->font.pixel_size) / 2,
                 badge_size - 16, UI_BACKGROUND, operation->color);
    text_x = bounds.x + badge_size + 42;
    title_y = bounds.y + bounds.height / 2 - 8;
    summary_y = title_y + (int)app->font.pixel_size + 14;
    ui_draw_text(&app->display, &app->font, operation->name, text_x,
                 title_y,
                 bounds.width - badge_size - 60, UI_TEXT, background);
    ui_draw_text(&app->display, &app->font, operation->summary, text_x,
                 summary_y,
                 bounds.width - badge_size - 60, UI_MUTED, background);
}

/**
 * @brief 绘制桌面应用启动器。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_desktop_page(struct browser_app *app)
{
    size_t index;

    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_header(&app->display, &app->font,
                           "Embedded Desktop", "ARM64 framebuffer workspace");
    for (index = 0; index < app->desktop_apps.count; index++) {
        const struct desktop_app_operation *operation =
            desktop_app_at(&app->desktop_apps, index);

        if (operation != NULL) {
            draw_desktop_application(app, operation, index);
        }
    }
    browser_ui_draw_footer_hint(
        &app->display, &app->font,
        "Arrow keys select  Enter open  Touch an app  Q shutdown");
    return bmp_display_flush(&app->display);
}

/**
 * @brief 处理桌面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_desktop_key(struct browser_app *app, enum input_action action)
{
    size_t columns = desktop_columns(app);
    size_t count = app->desktop_apps.count;

    if (count == 0) {
        return 0;
    }
    if (action == INPUT_ACTION_PREVIOUS) {
        app->desktop_selected = (app->desktop_selected + count - 1U) % count;
    } else if (action == INPUT_ACTION_NEXT) {
        app->desktop_selected = (app->desktop_selected + 1U) % count;
    } else if (action == INPUT_ACTION_UP) {
        app->desktop_selected =
            (app->desktop_selected + count - columns) % count;
    } else if (action == INPUT_ACTION_DOWN) {
        app->desktop_selected =
            (app->desktop_selected + columns) % count;
    } else if (action == INPUT_ACTION_OPEN) {
        return desktop_app_launch(&app->desktop_apps, app,
                                  app->desktop_selected);
    } else {
        return 0;
    }
    return render_desktop_page(app);
}

/**
 * @brief 处理桌面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_desktop_touch(struct browser_app *app,
                         const struct browser_input *input)
{
    size_t index;

    if (input->touch != TOUCH_ACTION_TAP) {
        return 0;
    }
    for (index = 0; index < app->desktop_apps.count; index++) {
        struct desktop_card_bounds bounds;

        desktop_card_bounds_at(app, index, &bounds);
        if (input->x >= bounds.x &&
            input->x < bounds.x + bounds.width &&
            input->y >= bounds.y &&
            input->y < bounds.y + bounds.height) {
            app->desktop_selected = index;
            return desktop_app_launch(&app->desktop_apps, app, index);
        }
    }
    return 0;
}
