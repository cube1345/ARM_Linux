#include "page_audio.h"

#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "page_queue.h"
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
#define AUDIO_QUEUE_GAP 12

/** @brief 判断文件类型是否属于 WAV/MP3 音频队列。 */
static int audio_entry_supported(enum file_type type)
{
    return type == FILE_TYPE_WAV || type == FILE_TYPE_MP3 ||
           type == FILE_TYPE_PLUGIN_AUDIO;
}

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

/** @brief 查找播放列表中相邻的 WAV/MP3 条目。 */
static int find_adjacent_audio(const struct browser_app *app, int direction)
{
    size_t count = app->files.count;
    size_t offset;
    size_t index;

    if (count == 0 || app->selected >= count) return -1;
    if (app->playback_mode == BROWSER_PLAYBACK_SHUFFLE && direction > 0) {
        size_t candidate = (size_t)(monotonic_ms() % count);

        for (offset = 0; offset < count; offset++) {
            index = (candidate + offset) % count;
            if (index != app->selected &&
                audio_entry_supported(app->files.entries[index].type)) {
                return (int)index;
            }
        }
        return -1;
    }
    for (offset = 1; offset <= count; offset++) {
        if (direction > 0) {
            if (app->playback_mode == BROWSER_PLAYBACK_ONCE &&
                app->selected + offset >= count) return -1;
            index = (app->selected + offset) % count;
        } else {
            if (app->playback_mode == BROWSER_PLAYBACK_ONCE &&
                offset > app->selected) return -1;
            index = (app->selected + count - offset) % count;
        }
        if (audio_entry_supported(app->files.entries[index].type)) {
            return (int)index;
        }
    }
    return -1;
}

/** @brief 启动指定播放列表条目的 WAV/MP3。 */
static int start_audio_index(struct browser_app *app, size_t index)
{
    char path[PATH_MAX];
    size_t previous = app->selected;

    if (file_list_path(&app->files, index, path, sizeof(path)) < 0) {
        return -1;
    }
    audio_player_stop(&app->audio);
    if (audio_player_start(&app->audio, path, app->alsa_device) < 0) {
        app->selected = previous;
        return -1;
    }
    snprintf(app->current_path, sizeof(app->current_path), "%s", path);
    browser_app_restore_playback(app, path, BROWSER_PAGE_AUDIO);
    (void)audio_metadata_read(path, &app->audio_metadata);
    app->selected = index;
    app->page = BROWSER_PAGE_AUDIO;
    return 0;
}

/** @brief 打开相邻音频条目。 */
static int play_adjacent_audio(struct browser_app *app, int direction)
{
    int index = find_adjacent_audio(app, direction);

    if (index < 0) return -1;
    if (start_audio_index(app, (size_t)index) < 0) return -1;
    return render_audio_page(app);
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
    char metadata_line[AUDIO_METADATA_TEXT_SIZE * 2 + 8];
    const char *title = app->audio_metadata.title[0] != '\0' ?
                        app->audio_metadata.title :
                        app->files.entries[app->selected].name;

    audio_player_get_status(&app->audio, &status);
    progress = audio_status_progress(&status);
    browser_ui_format_time(status.position_ms, elapsed, sizeof(elapsed));
    browser_ui_format_time(status.duration_ms, duration, sizeof(duration));
    snprintf(timing, sizeof(timing), "%s / %s", elapsed, duration);
    snprintf(volume, sizeof(volume), "Volume %d%%", status.volume);
    if (app->audio_metadata.artist[0] != '\0' &&
        app->audio_metadata.album[0] != '\0') {
        snprintf(metadata_line, sizeof(metadata_line), "%s - %s",
                 app->audio_metadata.artist, app->audio_metadata.album);
    } else {
        snprintf(metadata_line, sizeof(metadata_line), "%s",
                 app->audio_metadata.artist[0] != '\0' ?
                 app->audio_metadata.artist : app->audio_metadata.album);
    }
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
                 title,
                 UI_MARGIN + AUDIO_PANEL_INSET + 92,
                 panel_y + 28 + (int)app->font.pixel_size + 4,
                 panel_width - AUDIO_PANEL_INSET * 2 - 92,
                 UI_TEXT, UI_SURFACE);
    if (metadata_line[0] != '\0') {
        ui_draw_text(&app->display, &app->font, metadata_line,
                     UI_MARGIN + AUDIO_PANEL_INSET + 92,
                     panel_y + 62 + (int)app->font.pixel_size,
                     panel_width - AUDIO_PANEL_INSET * 2 - 92,
                     UI_MUTED, UI_SURFACE);
    }
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
    page_queue_draw(app, audio_entry_supported, "QUEUE", UI_MARGIN,
                    panel_y + AUDIO_PANEL_HEIGHT + AUDIO_QUEUE_GAP,
                    panel_width,
                    (int)app->display.variable_info.yres -
                    (panel_y + AUDIO_PANEL_HEIGHT + AUDIO_QUEUE_GAP) -
                    UI_FOOTER_HEIGHT - UI_MARGIN);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Space play/pause  ↑/↓ track  ←/→ seek  +/- volume");
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

/** @brief 打开当前播放列表中的下一首 WAV/MP3。 */
int audio_play_next(struct browser_app *app)
{
    return play_adjacent_audio(app, 1);
}

/** @brief 打开当前播放列表中的上一首 WAV/MP3。 */
int audio_play_previous(struct browser_app *app)
{
    return play_adjacent_audio(app, -1);
}

/** @brief 根据播放模式处理当前音频自然结束。 */
int audio_handle_completion(struct browser_app *app)
{
    if (app->playback_mode == BROWSER_PLAYBACK_ONCE) return -1;
    if (app->playback_mode == BROWSER_PLAYBACK_REPEAT_ONE) {
        if (start_audio_index(app, app->selected) < 0) return -1;
        return render_audio_page(app);
    }
    return audio_play_next(app);
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
    } else if (action == INPUT_ACTION_UP) {
        return audio_play_previous(app);
    } else if (action == INPUT_ACTION_DOWN) {
        return audio_play_next(app);
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
