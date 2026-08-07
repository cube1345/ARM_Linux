#include "page_settings.h"

#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "ui_draw.h"

#include <stdint.h>
#include <stdio.h>

#define SETTINGS_PANEL_TOP (UI_HEADER_HEIGHT + 30)
#define SETTINGS_PANEL_HEIGHT 390
#define SETTINGS_BUTTON_WIDTH 118
#define SETTINGS_BUTTON_HEIGHT 48
#define SETTINGS_VOLUME_LABEL_Y (SETTINGS_PANEL_TOP + 52)
#define SETTINGS_VOLUME_BAR_Y (SETTINGS_PANEL_TOP + 94)
#define SETTINGS_VOLUME_BUTTON_Y (SETTINGS_PANEL_TOP + 120)
#define SETTINGS_FONT_LABEL_Y (SETTINGS_PANEL_TOP + 192)
#define SETTINGS_FONT_BUTTON_Y (SETTINGS_PANEL_TOP + 220)
#define SETTINGS_MODE_LABEL_Y (SETTINGS_PANEL_TOP + 280)
#define SETTINGS_MODE_BUTTON_Y (SETTINGS_PANEL_TOP + 308)
#define SETTINGS_SAMPLE_Y (SETTINGS_PANEL_TOP + 360)

/** @brief 获取播放模式显示名称。 */
static const char *settings_playback_mode_name(
    enum browser_playback_mode mode)
{
    switch (mode) {
    case BROWSER_PLAYBACK_ONCE: return "Play once";
    case BROWSER_PLAYBACK_REPEAT_ONE: return "Repeat one";
    case BROWSER_PLAYBACK_SHUFFLE: return "Shuffle";
    case BROWSER_PLAYBACK_REPEAT_ALL:
    default: return "Repeat list";
    }
}

/** @brief 循环切换播放模式。 */
static void settings_cycle_playback_mode(struct browser_app *app)
{
    app->playback_mode = app->playback_mode >= BROWSER_PLAYBACK_SHUFFLE ?
                         BROWSER_PLAYBACK_ONCE :
                         (enum browser_playback_mode)((int)app->playback_mode +
                                                      1);
}

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
    char font_size[64];
    char playback_mode[64];
    char device[180];

    audio_player_get_status(&app->audio, &status);
    snprintf(volume, sizeof(volume), "Desktop volume  %d%%", status.volume);
    snprintf(font_size, sizeof(font_size), "Font size  %upx",
             app->font.pixel_size);
    snprintf(playback_mode, sizeof(playback_mode), "Playback  %s",
             settings_playback_mode_name(app->playback_mode));
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
                 SETTINGS_VOLUME_LABEL_Y,
                 panel_width - 48, UI_TEXT, UI_SURFACE);
    browser_ui_draw_progress_bar(&app->display, UI_MARGIN + 24,
                                 SETTINGS_VOLUME_BAR_Y,
                                 panel_width - 48, 14,
                                 status.volume, UI_ACCENT);
    browser_ui_draw_button(&app->display, &app->font, button_x,
                           SETTINGS_VOLUME_BUTTON_Y,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "- 5", UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font,
                           button_x + SETTINGS_BUTTON_WIDTH + 20,
                           SETTINGS_VOLUME_BUTTON_Y,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "+ 5", UI_HEADER);
    ui_draw_text(&app->display, &app->font, font_size, UI_MARGIN + 24,
                 SETTINGS_FONT_LABEL_Y, panel_width - 48,
                 UI_TEXT, UI_SURFACE);
    browser_ui_draw_button(&app->display, &app->font, button_x,
                           SETTINGS_FONT_BUTTON_Y,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "A -", UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font,
                           button_x + SETTINGS_BUTTON_WIDTH + 20,
                           SETTINGS_FONT_BUTTON_Y,
                           SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT,
                           "A +", UI_HEADER);
    ui_draw_text(&app->display, &app->font, playback_mode, UI_MARGIN + 24,
                 SETTINGS_MODE_LABEL_Y, panel_width - 48,
                 UI_TEXT, UI_SURFACE);
    browser_ui_draw_button(&app->display, &app->font, button_x,
                           SETTINGS_MODE_BUTTON_Y, SETTINGS_BUTTON_WIDTH * 2 + 20,
                           SETTINGS_BUTTON_HEIGHT, "CHANGE", UI_HEADER);
    ui_draw_text(&app->display, &app->font,
                 "Sample: embedded Linux media desktop",
                 UI_MARGIN + 24, SETTINGS_SAMPLE_Y,
                 panel_width - 48, UI_MUTED, UI_SURFACE);
    browser_ui_draw_panel(&app->display, UI_MARGIN,
                          SETTINGS_PANEL_TOP + SETTINGS_PANEL_HEIGHT + 12,
                          panel_width, 72, UI_SURFACE_ALT, UI_BORDER);
    ui_draw_text(&app->display, &app->font, device, UI_MARGIN + 24,
                 SETTINGS_PANEL_TOP + SETTINGS_PANEL_HEIGHT +
                 (int)app->font.pixel_size + 30,
                 panel_width - 48, UI_MUTED, UI_SURFACE_ALT);
    browser_ui_draw_footer_hint(
        &app->display, &app->font,
        "Left/Right volume  Up/Down font  R changes playback  Esc returns");
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
        browser_app_set_volume(app, status.volume - 5);
    } else if (action == INPUT_ACTION_NEXT ||
               action == INPUT_ACTION_VOLUME_UP) {
        browser_app_set_volume(app, status.volume + 5);
    } else if (action == INPUT_ACTION_UP) {
        if (browser_app_adjust_font_size(app,
                                         (int)BROWSER_FONT_STEP_SIZE) < 0) {
            return -1;
        }
    } else if (action == INPUT_ACTION_DOWN) {
        if (browser_app_adjust_font_size(app,
                                         -(int)BROWSER_FONT_STEP_SIZE) < 0) {
            return -1;
        }
    } else if (action == INPUT_ACTION_ROTATE) {
        settings_cycle_playback_mode(app);
    } else {
        return 0;
    }
    (void)browser_app_save_config(app);
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
    int width = (int)app->display.variable_info.xres;
    int panel_width = width - UI_MARGIN * 2;
    int button_x = settings_button_x(app);

    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        input->x >= UI_MARGIN + 24 &&
        input->x <= UI_MARGIN + 24 + panel_width - 48 &&
        browser_ui_touches_bar(input, SETTINGS_VOLUME_BAR_Y)) {
        browser_app_set_volume(app,
                               browser_ui_bar_percent_at(input->x,
                                                        UI_MARGIN + 24,
                                                        panel_width - 48));
        return render_settings_page(app);
    }
    if (input->touch != TOUCH_ACTION_TAP) {
        return 0;
    }
    audio_player_get_status(&app->audio, &status);
    if (input->y >= SETTINGS_VOLUME_BUTTON_Y &&
        input->y <= SETTINGS_VOLUME_BUTTON_Y + SETTINGS_BUTTON_HEIGHT) {
        if (input->x >= button_x &&
            input->x <= button_x + SETTINGS_BUTTON_WIDTH) {
            browser_app_set_volume(app, status.volume - 5);
        } else if (input->x >= button_x + SETTINGS_BUTTON_WIDTH + 20 &&
                   input->x <= button_x + SETTINGS_BUTTON_WIDTH * 2 + 20) {
            browser_app_set_volume(app, status.volume + 5);
        } else {
            return 0;
        }
    } else if (input->y >= SETTINGS_MODE_BUTTON_Y &&
               input->y <= SETTINGS_MODE_BUTTON_Y + SETTINGS_BUTTON_HEIGHT &&
               input->x >= button_x &&
               input->x <= button_x + SETTINGS_BUTTON_WIDTH * 2 + 20) {
        settings_cycle_playback_mode(app);
    } else if (input->y >= SETTINGS_FONT_BUTTON_Y &&
               input->y <= SETTINGS_FONT_BUTTON_Y +
               SETTINGS_BUTTON_HEIGHT) {
        if (input->x >= button_x &&
            input->x <= button_x + SETTINGS_BUTTON_WIDTH) {
            if (browser_app_adjust_font_size(
                    app, -(int)BROWSER_FONT_STEP_SIZE) < 0) {
                return -1;
            }
        } else if (input->x >= button_x + SETTINGS_BUTTON_WIDTH + 20 &&
                   input->x <= button_x + SETTINGS_BUTTON_WIDTH * 2 + 20) {
            if (browser_app_adjust_font_size(
                    app, (int)BROWSER_FONT_STEP_SIZE) < 0) {
                return -1;
            }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
    (void)browser_app_save_config(app);
    return render_settings_page(app);
}
