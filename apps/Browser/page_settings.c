#include "page_settings.h"

#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "ui_draw.h"

#include <stdio.h>

#define SETTINGS_PANEL_TOP (UI_HEADER_HEIGHT + 30)
#define SETTINGS_PANEL_HEIGHT 250
#define SETTINGS_BUTTON_WIDTH 118
#define SETTINGS_BUTTON_HEIGHT 48
#define SETTINGS_VOLUME_Y (SETTINGS_PANEL_TOP + 118)

/**
 * @brief 获取设置页面按钮起始 X 坐标。
 * @param app 浏览器上下文。
 * @return 减号按钮 X 坐标。
 */
static int settings_button_x(const struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;

    return (width - SETTINGS_BUTTON_WIDTH * 2 - 20) / 2;
}

/**
 * @brief 绘制桌面设置页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_settings_page(struct browser_app *app)
{
    struct audio_player_status status;
    int width = (int)app->display.variable_info.xres;
    int panel_width = width - UI_MARGIN * 2;
    int button_x = settings_button_x(app);
    char volume[48];
    char device[180];

    audio_player_get_status(&app->audio, &status);
    snprintf(volume, sizeof(volume), "Desktop volume  %d%%", status.volume);
    snprintf(device, sizeof(device), "ALSA output: %s", app->alsa_device);
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_navigation_header(&app->display, &app->font,
                                      "Settings",
                                      "Shared desktop preferences");
    browser_ui_draw_panel(&app->display, UI_MARGIN, SETTINGS_PANEL_TOP,
                          panel_width, SETTINGS_PANEL_HEIGHT,
                          UI_SURFACE, UI_BORDER);
    ui_draw_text(&app->display, &app->font, volume, UI_MARGIN + 24,
                 SETTINGS_PANEL_TOP + (int)app->font.pixel_size + 30,
                 panel_width - 48, UI_TEXT, UI_SURFACE);
    browser_ui_draw_progress_bar(&app->display, UI_MARGIN + 24,
                                 SETTINGS_VOLUME_Y,
                                 panel_width - 48, 14,
                                 status.volume, UI_ACCENT);
    browser_ui_draw_button(&app->display, &app->font, button_x,
                           SETTINGS_PANEL_TOP + 158,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "- 5", UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font,
                           button_x + SETTINGS_BUTTON_WIDTH + 20,
                           SETTINGS_PANEL_TOP + 158,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "+ 5", UI_HEADER);
    browser_ui_draw_panel(&app->display, UI_MARGIN,
                          SETTINGS_PANEL_TOP + SETTINGS_PANEL_HEIGHT + 20,
                          panel_width, 92, UI_SURFACE_ALT, UI_BORDER);
    ui_draw_text(&app->display, &app->font, device, UI_MARGIN + 24,
                 SETTINGS_PANEL_TOP + SETTINGS_PANEL_HEIGHT +
                 (int)app->font.pixel_size + 40,
                 panel_width - 48, UI_MUTED, UI_SURFACE_ALT);
    browser_ui_draw_footer_hint(
        &app->display, &app->font,
        "Left/Right or +/- adjusts volume  Esc returns to desktop");
    return bmp_display_flush(&app->display);
}

/**
 * @brief 处理设置页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_settings_key(struct browser_app *app, enum input_action action)
{
    struct audio_player_status status;

    audio_player_get_status(&app->audio, &status);
    if (action == INPUT_ACTION_PREVIOUS ||
        action == INPUT_ACTION_VOLUME_DOWN) {
        audio_player_set_volume(&app->audio, status.volume - 5);
    } else if (action == INPUT_ACTION_NEXT ||
               action == INPUT_ACTION_VOLUME_UP) {
        audio_player_set_volume(&app->audio, status.volume + 5);
    } else {
        return 0;
    }
    return render_settings_page(app);
}

/**
 * @brief 处理设置页面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_settings_touch(struct browser_app *app,
                          const struct browser_input *input)
{
    struct audio_player_status status;
    int button_x = settings_button_x(app);
    int button_y = SETTINGS_PANEL_TOP + 158;

    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        browser_ui_touches_bar(input, SETTINGS_VOLUME_Y)) {
        audio_player_set_volume(&app->audio,
                                browser_ui_bar_percent(&app->display,
                                                       input->x));
        return render_settings_page(app);
    }
    if (input->touch != TOUCH_ACTION_TAP ||
        input->y < button_y ||
        input->y > button_y + SETTINGS_BUTTON_HEIGHT) {
        return 0;
    }
    audio_player_get_status(&app->audio, &status);
    if (input->x >= button_x &&
        input->x <= button_x + SETTINGS_BUTTON_WIDTH) {
        audio_player_set_volume(&app->audio, status.volume - 5);
    } else if (input->x >= button_x + SETTINGS_BUTTON_WIDTH + 20 &&
               input->x <= button_x + SETTINGS_BUTTON_WIDTH * 2 + 20) {
        audio_player_set_volume(&app->audio, status.volume + 5);
    } else {
        return 0;
    }
    return render_settings_page(app);
}
