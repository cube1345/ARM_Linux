#include "page_audio.h"

#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "ui_draw.h"

#include <stdint.h>
#include <stdio.h>

#define AUDIO_PANEL_HEIGHT 270
#define AUDIO_PANEL_INSET 24
#define AUDIO_PLAY_BUTTON_WIDTH 150
#define AUDIO_PLAY_BUTTON_HEIGHT 46
#define AUDIO_PLAY_BUTTON_OFFSET_Y 64
#define AUDIO_PROGRESS_OFFSET_Y 154
#define AUDIO_VOLUME_OFFSET_Y 220

/**
 * @brief 获取音频播放器卡片宽度。
 * @param app 浏览器上下文。
 * @return 卡片宽度。
 */
static int audio_panel_width(const struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres - UI_MARGIN * 2;

    return width > 1 ? width : 1;
}

/**
 * @brief 获取音频播放器卡片顶部 Y 坐标。
 * @param app 浏览器上下文。
 * @return 卡片 Y 坐标。
 */
static int audio_panel_y(const struct browser_app *app)
{
    int height = (int)app->display.variable_info.yres;
    int y = (height - AUDIO_PANEL_HEIGHT) / 2;

    if (y < UI_BUTTON_SIZE + 12) {
        y = UI_BUTTON_SIZE + 12;
    }
    if (y + AUDIO_PANEL_HEIGHT > height - UI_FOOTER_HEIGHT - 8) {
        y = height - UI_FOOTER_HEIGHT - AUDIO_PANEL_HEIGHT - 8;
    }
    return y > UI_MARGIN ? y : UI_MARGIN;
}

/**
 * @brief 获取音频页条形控件左上角 X 坐标。
 * @return 条形控件 X 坐标。
 */
static int audio_bar_x(void)
{
    return UI_MARGIN + AUDIO_PANEL_INSET;
}

/**
 * @brief 获取音频页条形控件宽度。
 * @param app 浏览器上下文。
 * @return 条形控件宽度。
 */
static int audio_bar_width(const struct browser_app *app)
{
    int width = audio_panel_width(app) - AUDIO_PANEL_INSET * 2;

    return width > 1 ? width : 1;
}

/**
 * @brief 获取音频页进度条 Y 坐标。
 * @param app 浏览器上下文。
 * @return 进度条 Y 坐标。
 */
static int audio_progress_y(const struct browser_app *app)
{
    return audio_panel_y(app) + AUDIO_PROGRESS_OFFSET_Y;
}

/**
 * @brief 获取音频页音量条 Y 坐标。
 * @param app 浏览器上下文。
 * @return 音量条 Y 坐标。
 */
static int audio_volume_y(const struct browser_app *app)
{
    return audio_panel_y(app) + AUDIO_VOLUME_OFFSET_Y;
}

/**
 * @brief 获取音频播放按钮左上角 X 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 X 坐标。
 */
static int audio_play_button_x(const struct browser_app *app)
{
    return UI_MARGIN + audio_panel_width(app) - AUDIO_PANEL_INSET -
           AUDIO_PLAY_BUTTON_WIDTH;
}

/**
 * @brief 获取音频播放按钮左上角 Y 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 Y 坐标。
 */
static int audio_play_button_y(const struct browser_app *app)
{
    return audio_panel_y(app) + AUDIO_PLAY_BUTTON_OFFSET_Y;
}

/**
 * @brief 将音频位置转换为百分比。
 * @param status 播放状态。
 * @return 0 到 100 的播放进度。
 */
static int audio_status_progress(const struct audio_player_status *status)
{
    uint64_t progress;

    if (status->duration_ms == 0) {
        return 0;
    }
    progress = status->position_ms * 100U / status->duration_ms;
    return progress > 100U ? 100 : (int)progress;
}

/**
 * @brief 将屏幕 X 坐标转换为音频条形控件百分比。
 * @param app 浏览器上下文。
 * @param x 屏幕 X 坐标。
 * @return 0 到 100 的百分比。
 */
static int audio_bar_percent(const struct browser_app *app, int x)
{
    int bar_x = audio_bar_x();
    int bar_width = audio_bar_width(app);
    int percent = (x - bar_x) * 100 / bar_width;

    if (percent < 0) {
        return 0;
    }
    return percent > 100 ? 100 : percent;
}

/**
 * @brief 判断触摸手势是否命中音频条形控件。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @param y 条形控件 Y 坐标。
 * @return 命中返回 1，否则返回 0。
 */
static int audio_touches_bar(const struct browser_app *app,
                             const struct browser_input *input, int y)
{
    int bar_x = audio_bar_x();
    int bar_width = audio_bar_width(app);

    return input->x >= bar_x && input->x <= bar_x + bar_width &&
           browser_ui_touches_bar(input, y);
}

/**
 * @brief 绘制音频播放页面、进度条和音量条。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_audio_page(struct browser_app *app)
{
    struct audio_player_status status;
    int panel_y = audio_panel_y(app);
    int panel_width = audio_panel_width(app);
    int bar_x = audio_bar_x();
    int bar_width = audio_bar_width(app);
    int progress_y = audio_progress_y(app);
    int volume_y = audio_volume_y(app);
    int button_x = audio_play_button_x(app);
    int button_y = audio_play_button_y(app);
    int progress;
    char elapsed[16];
    char duration[16];
    char timing[40];
    char volume[32];

    audio_player_get_status(&app->audio, &status);
    progress = audio_status_progress(&status);
    browser_ui_format_time(status.position_ms, elapsed, sizeof(elapsed));
    browser_ui_format_time(status.duration_ms, duration, sizeof(duration));
    snprintf(timing, sizeof(timing), "%s / %s", elapsed, duration);
    snprintf(volume, sizeof(volume), "Volume %d%%", status.volume);
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_back_button(&app->display, &app->font);
    browser_ui_draw_panel(&app->display, UI_MARGIN, panel_y, panel_width,
                          AUDIO_PANEL_HEIGHT, UI_SURFACE, UI_BORDER);
    ui_draw_rect(&app->display, UI_MARGIN + AUDIO_PANEL_INSET,
                 panel_y + 28, 74, 38, UI_ACCENT_2);
    ui_draw_text(&app->display, &app->font, "AUDIO",
                 UI_MARGIN + AUDIO_PANEL_INSET + 10,
                 panel_y + 28 + (int)app->font.pixel_size + 7,
                 58, UI_BACKGROUND, UI_ACCENT_2);
    ui_draw_text(&app->display, &app->font,
                 app->files.entries[app->selected].name,
                 UI_MARGIN + AUDIO_PANEL_INSET + 92,
                 panel_y + 28 + (int)app->font.pixel_size + 4,
                 panel_width - AUDIO_PANEL_INSET * 2 - 92,
                 UI_TEXT, UI_SURFACE);
    ui_draw_text(&app->display, &app->font,
                 audio_player_state_name(status.state),
                 UI_MARGIN + AUDIO_PANEL_INSET,
                 panel_y + 102 + (int)app->font.pixel_size,
                 panel_width / 2, UI_ACCENT, UI_SURFACE);
    browser_ui_draw_button(&app->display, &app->font, button_x, button_y,
                           AUDIO_PLAY_BUTTON_WIDTH,
                           AUDIO_PLAY_BUTTON_HEIGHT,
                           "PLAY / PAUSE", UI_HEADER);
    ui_draw_text(&app->display, &app->font, timing, bar_x,
                 progress_y - 12, bar_width, UI_MUTED, UI_SURFACE);
    browser_ui_draw_progress_bar(&app->display, bar_x, progress_y,
                                 bar_width, 12, progress, UI_ACCENT);
    ui_draw_text(&app->display, &app->font, volume, bar_x,
                 volume_y - 12, bar_width, UI_MUTED, UI_SURFACE);
    browser_ui_draw_progress_bar(&app->display, bar_x, volume_y,
                                 bar_width, 12, status.volume, UI_SELECTED);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Space play/pause  ←/→ seek  +/- volume");
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
        browser_app_set_volume(app, status.volume + 5);
    } else if (action == INPUT_ACTION_VOLUME_DOWN) {
        browser_app_set_volume(app, status.volume - 5);
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
    int button_x = audio_play_button_x(app);
    int button_y = audio_play_button_y(app);
    int progress_y = audio_progress_y(app);
    int volume_y = audio_volume_y(app);

    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        audio_touches_bar(app, input, progress_y)) {
        audio_player_seek_percent(&app->audio, audio_bar_percent(app,
                                                                 input->x));
        return render_audio_page(app);
    }
    if ((input->touch == TOUCH_ACTION_MOVE ||
         input->touch == TOUCH_ACTION_TAP) &&
        audio_touches_bar(app, input, volume_y)) {
        browser_app_set_volume(app, audio_bar_percent(app, input->x));
        return render_audio_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->x >= button_x &&
        input->x <= button_x + AUDIO_PLAY_BUTTON_WIDTH &&
        input->y >= button_y &&
        input->y <= button_y + AUDIO_PLAY_BUTTON_HEIGHT) {
        audio_player_toggle_pause(&app->audio);
        return render_audio_page(app);
    }
    return 0;
}
