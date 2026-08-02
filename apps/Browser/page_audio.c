#include "page_audio.h"

#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "ui_draw.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 绘制音频播放页面、进度条和音量条。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_audio_page(struct browser_app *app)
{
    struct audio_player_status status;
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    int bar_width = width - UI_MARGIN * 2;
    int progress_y = height / 2 + 34;
    int volume_y = progress_y + 72;
    int progress = 0;
    char timing[64];
    char volume[32];

    audio_player_get_status(&app->audio, &status);
    if (status.duration_ms > 0) {
        progress = (int)(status.position_ms * 100U / status.duration_ms);
        if (progress > 100) {
            progress = 100;
        }
    }
    browser_ui_format_time(status.position_ms, timing, sizeof(timing));
    snprintf(timing + strlen(timing), sizeof(timing) - strlen(timing), " / ");
    browser_ui_format_time(status.duration_ms, timing + strlen(timing),
                           sizeof(timing) - strlen(timing));
    snprintf(volume, sizeof(volume), "VOLUME %d%%", status.volume);
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_back_button(&app->display, &app->font);
    ui_draw_text(&app->display, &app->font,
                 app->files.entries[app->selected].name, UI_MARGIN,
                 height / 2 - 80, bar_width, UI_TEXT, UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font,
                 audio_player_state_name(status.state), UI_MARGIN,
                 height / 2 - 35, bar_width, UI_ACCENT, UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, timing, UI_MARGIN,
                 progress_y - 10, bar_width, UI_MUTED, UI_BACKGROUND);
    ui_draw_rect(&app->display, UI_MARGIN, progress_y, bar_width, 12, UI_TRACK);
    ui_draw_rect(&app->display, UI_MARGIN, progress_y,
                 bar_width * progress / 100, 12, UI_ACCENT);
    ui_draw_text(&app->display, &app->font, volume, UI_MARGIN,
                 volume_y - 10, bar_width, UI_MUTED, UI_BACKGROUND);
    ui_draw_rect(&app->display, UI_MARGIN, volume_y, bar_width, 12, UI_TRACK);
    ui_draw_rect(&app->display, UI_MARGIN, volume_y,
                 bar_width * status.volume / 100, 12, UI_SELECTED);
    ui_draw_rect(&app->display, width / 2 - 65, height / 2 - 20,
                 130, 44, UI_HEADER);
    ui_draw_text(&app->display, &app->font, "PLAY / PAUSE",
                 width / 2 - 56, height / 2 + 10, 116, UI_TEXT, UI_HEADER);
    app->last_audio_refresh_ms = monotonic_ms();
    return bmp_display_flush(&app->display);
}

/**
 * @brief 按当前音频位置相对跳转。
 * @param app 浏览器上下文。
 * @param delta_percent 百分比增量。
 */
void seek_relative(struct browser_app *app, int delta_percent)
{
    struct audio_player_status status;
    int percent = 0;

    audio_player_get_status(&app->audio, &status);
    if (status.duration_ms > 0) {
        percent = (int)(status.position_ms * 100U / status.duration_ms);
    }
    audio_player_seek_percent(&app->audio, percent + delta_percent);
}

/**
 * @brief 处理音频页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_audio_key(struct browser_app *app, enum input_action action)
{
    struct audio_player_status status;

    audio_player_get_status(&app->audio, &status);
    if (action == INPUT_ACTION_TOGGLE) {
        audio_player_toggle_pause(&app->audio);
    } else if (action == INPUT_ACTION_PREVIOUS) {
        seek_relative(app, -5);
    } else if (action == INPUT_ACTION_NEXT) {
        seek_relative(app, 5);
    } else if (action == INPUT_ACTION_VOLUME_UP) {
        audio_player_set_volume(&app->audio, status.volume + 5);
    } else if (action == INPUT_ACTION_VOLUME_DOWN) {
        audio_player_set_volume(&app->audio, status.volume - 5);
    } else {
        return 0;
    }
    return render_audio_page(app);
}

/**
 * @brief 处理音频页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_audio_touch(struct browser_app *app,
                       const struct browser_input *input)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    int progress_y = height / 2 + 34;
    int volume_y = progress_y + 72;

    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        browser_ui_touches_bar(input, progress_y)) {
        audio_player_seek_percent(&app->audio,
                                  browser_ui_bar_percent(&app->display,
                                                         input->x));
        return render_audio_page(app);
    }
    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        browser_ui_touches_bar(input, volume_y)) {
        audio_player_set_volume(&app->audio,
                                browser_ui_bar_percent(&app->display,
                                                       input->x));
        return render_audio_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->x >= width / 2 - 65 && input->x <= width / 2 + 65 &&
        input->y >= height / 2 - 20 && input->y <= height / 2 + 24) {
        audio_player_toggle_pause(&app->audio);
        return render_audio_page(app);
    }
    return 0;
}
