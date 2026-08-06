#include "page_desktop.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "desktop_app.h"
#include "ui_draw.h"

#include <stddef.h>
#include <stdint.h>

#define DESKTOP_STATUS_HEIGHT 58
#define DESKTOP_TITLE_TOP 72
#define DESKTOP_GRID_TOP 150
#define DESKTOP_GRID_GAP 30
#define DESKTOP_DOCK_HEIGHT 82
#define DESKTOP_ICON_MAX 118
#define DESKTOP_ICON_MIN 66
#define DESKTOP_ICON_RADIUS 24
#define DESKTOP_HIT_PADDING 12

/** @brief 桌面应用图标命中区域和图标坐标。 */
struct desktop_card_bounds {
    int x;
    int y;
    int width;
    int height;
    int icon_x;
    int icon_y;
    int icon_size;
};

/** @brief 将 RGB888 颜色拆分后按百分比混合。 */
static uint32_t desktop_mix_color(uint32_t a, uint32_t b, int b_percent)
{
    int a_percent = 100 - b_percent;
    uint32_t ar = (a >> 16) & 0xffU;
    uint32_t ag = (a >> 8) & 0xffU;
    uint32_t ab = a & 0xffU;
    uint32_t br = (b >> 16) & 0xffU;
    uint32_t bg = (b >> 8) & 0xffU;
    uint32_t bb = b & 0xffU;
    uint32_t r = (ar * (uint32_t)a_percent + br * (uint32_t)b_percent) /
                 100U;
    uint32_t g = (ag * (uint32_t)a_percent + bg * (uint32_t)b_percent) /
                 100U;
    uint32_t bl = (ab * (uint32_t)a_percent + bb * (uint32_t)b_percent) /
                  100U;

    return (r << 16) | (g << 8) | bl;
}

/** @brief 绘制一个像素级实心圆。 */
static void desktop_draw_circle(struct bmp_display *display, int cx, int cy,
                                int radius, uint32_t color)
{
    int y;

    for (y = -radius; y <= radius; y++) {
        int x;

        for (x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                bmp_display_put_rgb(display, cx + x, cy + y,
                                    (uint8_t)(color >> 16),
                                    (uint8_t)(color >> 8),
                                    (uint8_t)color);
            }
        }
    }
}

/** @brief 绘制带圆角的实心矩形。 */
static void desktop_draw_round_rect(struct bmp_display *display, int x, int y,
                                    int width, int height, int radius,
                                    uint32_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    if (radius < 1) {
        ui_draw_rect(display, x, y, width, height, color);
        return;
    }
    if (radius * 2 > width) {
        radius = width / 2;
    }
    if (radius * 2 > height) {
        radius = height / 2;
    }
    ui_draw_rect(display, x + radius, y, width - radius * 2, height, color);
    ui_draw_rect(display, x, y + radius, radius, height - radius * 2, color);
    ui_draw_rect(display, x + width - radius, y + radius, radius,
                 height - radius * 2, color);
    desktop_draw_circle(display, x + radius, y + radius, radius, color);
    desktop_draw_circle(display, x + width - radius - 1, y + radius,
                        radius, color);
    desktop_draw_circle(display, x + radius, y + height - radius - 1,
                        radius, color);
    desktop_draw_circle(display, x + width - radius - 1,
                        y + height - radius - 1, radius, color);
}

/** @brief 绘制带边框的圆角矩形。 */
static void desktop_draw_round_panel(struct bmp_display *display, int x, int y,
                                     int width, int height, int radius,
                                     uint32_t fill, uint32_t border)
{
    desktop_draw_round_rect(display, x + 4, y + 6, width, height, radius,
                            0x081018U);
    desktop_draw_round_rect(display, x, y, width, height, radius, border);
    desktop_draw_round_rect(display, x + 2, y + 2, width - 4, height - 4,
                            radius - 2, fill);
}

/** @brief 绘制水平方向渐变圆角图标底色。 */
static void desktop_draw_icon_background(struct bmp_display *display,
                                         int x, int y, int size,
                                         uint32_t color)
{
    int row;
    uint32_t light = desktop_mix_color(color, 0xffffffU, 32);
    uint32_t deep = desktop_mix_color(color, 0x05070aU, 24);

    desktop_draw_round_rect(display, x + 5, y + 8, size, size,
                            DESKTOP_ICON_RADIUS, 0x071019U);
    for (row = 0; row < size; row++) {
        uint32_t shade = desktop_mix_color(light, deep,
                                           row * 100 / size);

        desktop_draw_round_rect(display, x, y + row, size, 1,
                                DESKTOP_ICON_RADIUS, shade);
    }
    desktop_draw_round_rect(display, x + 8, y + 8, size - 16, size / 5,
                            size / 10, desktop_mix_color(light, 0xffffffU, 32));
}

/** @brief 绘制三角播放符号。 */
static void desktop_draw_play_triangle(struct bmp_display *display,
                                       int x, int y, int width, int height,
                                       uint32_t color)
{
    int row;

    for (row = 0; row < height; row++) {
        int half = row < height / 2 ? row : height - row;
        int run = width * (half + 1) / (height / 2 + 1);

        ui_draw_rect(display, x, y + row, run, 1, color);
    }
}

/** @brief 绘制图片相册图标内部图形。 */
static void desktop_draw_gallery_glyph(struct bmp_display *display,
                                       int x, int y, int size)
{
    int frame = size / 6;
    int body = size - frame * 2;

    desktop_draw_round_rect(display, x + frame, y + frame, body, body,
                            size / 10, 0xf7fbffU);
    ui_draw_rect(display, x + frame + 8, y + frame + body / 2,
                 body - 16, body / 2 - 8, 0x55c7f7U);
    desktop_draw_circle(display, x + frame + body - 22, y + frame + 22,
                        size / 11, 0xffe082U);
    desktop_draw_play_triangle(display, x + frame + 15,
                               y + frame + body - 18,
                               body / 2, body / 2, 0x3ccf98U);
}

/** @brief 绘制播放器图标内部图形。 */
static void desktop_draw_player_glyph(struct bmp_display *display,
                                      int x, int y, int size)
{
    desktop_draw_circle(display, x + size / 2, y + size / 2, size / 3,
                        0xf8fbffU);
    desktop_draw_play_triangle(display, x + size / 2 - size / 10,
                               y + size / 2 - size / 5,
                               size / 3, size / 2, 0x1a2734U);
    ui_draw_rect(display, x + size / 4, y + size - size / 4,
                 size / 2, 5, 0xcfe3ffU);
}

/** @brief 绘制文件夹图标内部图形。 */
static void desktop_draw_files_glyph(struct bmp_display *display,
                                     int x, int y, int size)
{
    int left = x + size / 6;
    int top = y + size / 4;
    int width = size * 2 / 3;

    desktop_draw_round_rect(display, left, top, width / 2, size / 5,
                            size / 14, 0xfff0b0U);
    desktop_draw_round_rect(display, left, top + size / 8, width,
                            size / 2, size / 9, 0xfffbdfU);
    ui_draw_rect(display, left + 8, top + size / 3, width - 16, 4,
                 0xe0a822U);
}

/** @brief 绘制阅读器图标内部图形。 */
static void desktop_draw_reader_glyph(struct bmp_display *display,
                                      int x, int y, int size)
{
    int page_w = size / 3;
    int page_h = size * 3 / 5;
    int top = y + size / 5;

    desktop_draw_round_rect(display, x + size / 5, top, page_w, page_h,
                            size / 16, 0xfff6fbU);
    desktop_draw_round_rect(display, x + size / 2, top, page_w, page_h,
                            size / 16, 0xf4ecffU);
    ui_draw_rect(display, x + size / 5 + 8, top + 18, page_w - 14, 3,
                 0xb74f7dU);
    ui_draw_rect(display, x + size / 2 + 8, top + 18, page_w - 14, 3,
                 0xb74f7dU);
    ui_draw_rect(display, x + size / 5 + 8, top + 32, page_w - 14, 3,
                 0xb74f7dU);
}

/** @brief 绘制诊断工具图标内部图形。 */
static void desktop_draw_diagnostics_glyph(struct bmp_display *display,
                                           int x, int y, int size)
{
    int base = y + size * 2 / 3;
    int left = x + size / 5;
    int step = size / 7;
    int index;

    for (index = 0; index < 5; index++) {
        int bar = (index % 2 == 0 ? size / 3 : size / 2);

        ui_draw_rect(display, left + index * step, base - bar,
                     step / 2 + 2, bar, 0xeaffffU);
    }
    desktop_draw_circle(display, x + size / 2, y + size / 3,
                        size / 8, 0x18313aU);
}

/** @brief 绘制设置图标内部图形。 */
static void desktop_draw_settings_glyph(struct bmp_display *display,
                                        int x, int y, int size)
{
    int cx = x + size / 2;
    int cy = y + size / 2;
    int arm = size / 5;

    ui_draw_rect(display, cx - 4, cy - arm - 8, 8, arm, 0xf6fbffU);
    ui_draw_rect(display, cx - 4, cy + 8, 8, arm, 0xf6fbffU);
    ui_draw_rect(display, cx - arm - 8, cy - 4, arm, 8, 0xf6fbffU);
    ui_draw_rect(display, cx + 8, cy - 4, arm, 8, 0xf6fbffU);
    desktop_draw_circle(display, cx, cy, size / 4, 0xf6fbffU);
    desktop_draw_circle(display, cx, cy, size / 9, 0x25313cU);
}

/** @brief 绘制应用专属图标图形。 */
static void desktop_draw_app_icon(struct browser_app *app,
                                  const struct desktop_app_operation *op,
                                  const struct desktop_card_bounds *bounds)
{
    int x = bounds->icon_x;
    int y = bounds->icon_y;
    int size = bounds->icon_size;

    desktop_draw_icon_background(&app->display, x, y, size, op->color);
    if (op->id == DESKTOP_APP_GALLERY) {
        desktop_draw_gallery_glyph(&app->display, x, y, size);
    } else if (op->id == DESKTOP_APP_PLAYER) {
        desktop_draw_player_glyph(&app->display, x, y, size);
    } else if (op->id == DESKTOP_APP_FILES) {
        desktop_draw_files_glyph(&app->display, x, y, size);
    } else if (op->id == DESKTOP_APP_READER) {
        desktop_draw_reader_glyph(&app->display, x, y, size);
    } else if (op->id == DESKTOP_APP_DIAGNOSTICS) {
        desktop_draw_diagnostics_glyph(&app->display, x, y, size);
    } else {
        desktop_draw_settings_glyph(&app->display, x, y, size);
    }
}

/** @brief 测量单行文本宽度。 */
static int desktop_measure_text(struct font_renderer *font, const char *text,
                                int max_width)
{
    size_t offset = 0;
    size_t length = 0;
    int width = 0;

    while (text[length] != '\0') {
        length++;
    }
    while (offset < length && width < max_width) {
        uint32_t codepoint;
        size_t consumed = font_renderer_decode_utf8(
            (const uint8_t *)text + offset, length - offset, &codepoint);
        int advance = 0;

        if (consumed == 0 || codepoint == '\n' || codepoint == '\r' ||
            font_renderer_measure_codepoint(font, codepoint, &advance) < 0 ||
            width + advance > max_width) {
            break;
        }
        width += advance;
        offset += consumed;
    }
    return width;
}

/** @brief 居中绘制一行文本。 */
static void desktop_draw_center_text(struct browser_app *app, const char *text,
                                     int x, int y, int width,
                                     uint32_t fg, uint32_t bg)
{
    int text_width = desktop_measure_text(&app->font, text, width);
    int text_x = x + (width - text_width) / 2;

    ui_draw_text(&app->display, &app->font, text, text_x, y, width, fg, bg);
}

/** @brief 根据 framebuffer 宽度选择桌面列数。 */
static size_t desktop_columns(const struct browser_app *app)
{
    if (app->display.variable_info.xres >= 1180U) {
        return 4U;
    }
    return app->display.variable_info.xres >= 720U ? 3U : 2U;
}

/** @brief 计算指定应用的手机桌面图标区域。 */
static void desktop_card_bounds_at(const struct browser_app *app,
                                   size_t index,
                                   struct desktop_card_bounds *bounds)
{
    size_t columns = desktop_columns(app);
    size_t rows = (app->desktop_apps.count + columns - 1U) / columns;
    int screen_width = (int)app->display.variable_info.xres;
    int screen_height = (int)app->display.variable_info.yres;
    int bottom = DESKTOP_DOCK_HEIGHT + 28;
    int available_width = screen_width - UI_MARGIN * 2 -
                          (int)(columns - 1U) * DESKTOP_GRID_GAP;
    int available_height = screen_height - DESKTOP_GRID_TOP - bottom -
                           (int)(rows - 1U) * DESKTOP_GRID_GAP;
    int icon_size;

    bounds->width = available_width / (int)columns;
    bounds->height = available_height / (int)rows;
    bounds->x = UI_MARGIN + (int)(index % columns) *
                (bounds->width + DESKTOP_GRID_GAP);
    bounds->y = DESKTOP_GRID_TOP + (int)(index / columns) *
                (bounds->height + DESKTOP_GRID_GAP);
    icon_size = bounds->height - 54;
    if (icon_size > bounds->width - 42) {
        icon_size = bounds->width - 42;
    }
    if (icon_size > DESKTOP_ICON_MAX) {
        icon_size = DESKTOP_ICON_MAX;
    }
    if (icon_size < DESKTOP_ICON_MIN) {
        icon_size = DESKTOP_ICON_MIN;
    }
    bounds->icon_size = icon_size;
    bounds->icon_x = bounds->x + (bounds->width - icon_size) / 2;
    bounds->icon_y = bounds->y + 8;
}

/** @brief 绘制手机桌面壁纸和状态栏。 */
static void desktop_draw_wallpaper(struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    int row;

    for (row = 0; row < height; row++) {
        uint32_t color = desktop_mix_color(0x111827U, 0x132f3bU,
                                           row * 100 / height);

        ui_draw_rect(&app->display, 0, row, width, 1, color);
    }
    desktop_draw_circle(&app->display, width - 110, 118, 148, 0x183f4dU);
    desktop_draw_circle(&app->display, 76, height - 150, 120, 0x1c2f45U);
    ui_draw_rect(&app->display, 0, 0, width, DESKTOP_STATUS_HEIGHT,
                 0x111b25U);
    ui_draw_rect(&app->display, 0, DESKTOP_STATUS_HEIGHT - 1, width, 1,
                 0x263745U);
    ui_draw_text(&app->display, &app->font, "Media OS", UI_MARGIN,
                 (int)app->font.pixel_size + 12, 180, UI_TEXT, 0x111b25U);
    ui_draw_text(&app->display, &app->font, "FB", width - 148,
                 (int)app->font.pixel_size + 12, 36, UI_MUTED, 0x111b25U);
    ui_draw_rect(&app->display, width - 98, 19, 42, 18, UI_BORDER);
    ui_draw_rect(&app->display, width - 56, 24, 4, 8, UI_BORDER);
    ui_draw_rect(&app->display, width - 94, 23, 28, 10, UI_ACCENT);
}

/** @brief 绘制桌面标题文案。 */
static void desktop_draw_title(struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;

    ui_draw_text(&app->display, &app->font, "Embedded Desktop", UI_MARGIN,
                 DESKTOP_TITLE_TOP + (int)app->font.pixel_size,
                 width - UI_MARGIN * 2, UI_TEXT, 0x13202cU);
    ui_draw_text(&app->display, &app->font,
                 "Touch an icon to launch an app", UI_MARGIN,
                 DESKTOP_TITLE_TOP + (int)app->font.pixel_size * 2 + 10,
                 width - UI_MARGIN * 2, UI_MUTED, 0x13202cU);
}

/** @brief 绘制一个桌面应用图标。 */
static void draw_desktop_application(
    struct browser_app *app,
    const struct desktop_app_operation *operation, size_t index)
{
    struct desktop_card_bounds bounds;
    uint32_t label_bg = 0x13202cU;
    int label_y;

    desktop_card_bounds_at(app, index, &bounds);
    if (index == app->desktop_selected) {
        desktop_draw_round_panel(&app->display,
                                 bounds.icon_x - DESKTOP_HIT_PADDING,
                                 bounds.icon_y - DESKTOP_HIT_PADDING,
                                 bounds.icon_size + DESKTOP_HIT_PADDING * 2,
                                 bounds.icon_size + 50,
                                 DESKTOP_ICON_RADIUS + 8,
                                 0x213848U, UI_SELECTED_BORDER);
        label_bg = 0x213848U;
    }
    desktop_draw_app_icon(app, operation, &bounds);
    label_y = bounds.icon_y + bounds.icon_size + (int)app->font.pixel_size + 8;
    desktop_draw_center_text(app, operation->name, bounds.x, label_y,
                             bounds.width, UI_TEXT, label_bg);
}

/** @brief 绘制底部 dock、分页点和当前应用说明。 */
static void desktop_draw_dock(struct browser_app *app)
{
    const struct desktop_app_operation *selected =
        desktop_app_at(&app->desktop_apps, app->desktop_selected);
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    int dock_y = height - DESKTOP_DOCK_HEIGHT - 12;
    char hint[128];

    desktop_draw_round_panel(&app->display, UI_MARGIN, dock_y,
                             width - UI_MARGIN * 2, DESKTOP_DOCK_HEIGHT,
                             24, 0x182633U, 0x324557U);
    if (selected != NULL) {
        int text_y = dock_y + (int)app->font.pixel_size + 18;

        desktop_draw_round_rect(&app->display, UI_MARGIN + 18, dock_y + 20,
                                42, 42, 16, selected->color);
        ui_draw_text(&app->display, &app->font, selected->name,
                     UI_MARGIN + 76, text_y,
                     width - UI_MARGIN * 2 - 96, UI_TEXT, 0x182633U);
        ui_draw_text(&app->display, &app->font, selected->summary,
                     UI_MARGIN + 76, text_y + (int)app->font.pixel_size + 10,
                     width - UI_MARGIN * 2 - 96, UI_MUTED, 0x182633U);
    }
    desktop_draw_round_rect(&app->display, width / 2 - 44, height - 10,
                            88, 4, 2, 0xc9d5ddU);
    (void)snprintf(hint, sizeof(hint), "Arrows select  Enter open  Q shutdown");
}

/** @brief 绘制桌面应用启动器。 */
int render_desktop_page(struct browser_app *app)
{
    size_t index;

    desktop_draw_wallpaper(app);
    desktop_draw_title(app);
    for (index = 0; index < app->desktop_apps.count; index++) {
        const struct desktop_app_operation *operation =
            desktop_app_at(&app->desktop_apps, index);

        if (operation != NULL) {
            draw_desktop_application(app, operation, index);
        }
    }
    desktop_draw_dock(app);
    return bmp_display_flush(&app->display);
}

/** @brief 处理桌面键盘动作。 */
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

/** @brief 处理桌面触摸动作。 */
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
