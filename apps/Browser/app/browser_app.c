#include "browser_app.h"

#include "audio_player.h"
#include "browser_config.h"
#include "browser_log.h"
#include "page_desktop.h"
#include "page_file.h"
#include "page_image.h"
#include "page_video.h"
#include "text_reader.h"

#include <stdio.h>
#include <string.h>

#define BROWSER_RESUME_MIN_POSITION_MS 3000U
#define BROWSER_RESUME_END_MARGIN_MS 3000U

/**
 * @brief 记录当前音频或 FFmpeg 媒体的断点位置。
 * @param app 浏览器上下文。
 */
void browser_app_remember_playback(struct browser_app *app)
{
    uint64_t position_ms;
    uint64_t duration_ms;

    if (app == NULL || app->current_path[0] == '\0') return;
    if (app->page == BROWSER_PAGE_AUDIO) {
        struct audio_player_status status;

        audio_player_get_status(&app->audio, &status);
        position_ms = status.position_ms;
        duration_ms = status.duration_ms;
    } else if (app->page == BROWSER_PAGE_VIDEO) {
        struct media_player_status status;

        media_player_get_status(&app->media, &status);
        position_ms = status.position_ms;
        duration_ms = status.duration_ms;
    } else {
        return;
    }
    if (position_ms >= BROWSER_RESUME_MIN_POSITION_MS &&
        duration_ms > BROWSER_RESUME_END_MARGIN_MS &&
        position_ms < duration_ms - BROWSER_RESUME_END_MARGIN_MS) {
        snprintf(app->config.resume_path, sizeof(app->config.resume_path),
                 "%s", app->current_path);
        app->config.resume_position_ms = position_ms;
    } else if (strcmp(app->config.resume_path, app->current_path) == 0) {
        app->config.resume_path[0] = '\0';
        app->config.resume_position_ms = 0;
    }
}

/**
 * @brief 取出并消费指定媒体文件的断点位置。
 * @param app 浏览器上下文。
 * @param path 媒体文件绝对路径。
 * @return 可恢复的位置毫秒值，无匹配断点返回 0。
 */
static uint64_t take_resume_position(struct browser_app *app,
                                     const char *path)
{
    uint64_t position_ms;

    if (app == NULL || path == NULL ||
        strcmp(app->config.resume_path, path) != 0) {
        return 0;
    }
    position_ms = app->config.resume_position_ms;
    app->config.resume_path[0] = '\0';
    app->config.resume_position_ms = 0;
    return position_ms;
}

/**
 * @brief 恢复指定音频或 FFmpeg 媒体的断点位置。
 * @param app 浏览器上下文。
 * @param path 媒体文件绝对路径。
 * @param page 目标播放器页面。
 */
void browser_app_restore_playback(struct browser_app *app, const char *path,
                                  enum browser_page page)
{
    uint64_t position_ms;

    if (page != BROWSER_PAGE_AUDIO && page != BROWSER_PAGE_VIDEO) return;
    position_ms = take_resume_position(app, path);
    if (position_ms == 0) return;
    if (page == BROWSER_PAGE_AUDIO) {
        audio_player_seek_ms(&app->audio, (int64_t)position_ms);
    } else {
        media_player_seek_ms(&app->media, (int64_t)position_ms);
    }
    (void)browser_app_save_config(app);
}

/**
 * @brief 关闭当前媒体资源并返回文件列表页。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_close_media_page(struct browser_app *app)
{
    browser_app_remember_playback(app);
    if (app->page == BROWSER_PAGE_IMAGE) {
        close_image(app);
    } else if (app->page == BROWSER_PAGE_TEXT) {
        text_reader_close(&app->text);
    } else if (app->page == BROWSER_PAGE_AUDIO) {
        audio_player_stop(&app->audio);
    } else if (app->page == BROWSER_PAGE_VIDEO) {
        media_player_stop(&app->media);
        image_data_destroy(&app->media_frame);
        app->media_frame_serial = 0;
    }
    (void)browser_app_save_config(app);
    app->page = BROWSER_PAGE_FILES;
    return render_file_page(app);
}

/**
 * @brief 关闭当前应用并返回桌面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_return_to_desktop(struct browser_app *app)
{
    browser_app_remember_playback(app);
    if (app->page == BROWSER_PAGE_IMAGE) {
        close_image(app);
    } else if (app->page == BROWSER_PAGE_TEXT) {
        text_reader_close(&app->text);
    } else if (app->page == BROWSER_PAGE_AUDIO) {
        audio_player_stop(&app->audio);
    } else if (app->page == BROWSER_PAGE_VIDEO) {
        media_player_stop(&app->media);
        image_data_destroy(&app->media_frame);
        app->media_frame_serial = 0;
    }
    (void)browser_app_save_config(app);
    app->active_app = DESKTOP_APP_NONE;
    app->file_filter = FILE_LIST_FILTER_ALL;
    app->search_active = 0;
    app->search_query[0] = '\0';
    app->page = BROWSER_PAGE_DESKTOP;
    return render_desktop_page(app);
}

/**
 * @brief 同时设置传统音频和 FFmpeg 播放器的软件音量。
 * @param app 浏览器上下文。
 * @param volume 音量百分比，自动限制到 0 到 100。
 */
void browser_app_set_volume(struct browser_app *app, int volume)
{
    if (app == NULL) {
        return;
    }
    audio_player_set_volume(&app->audio, volume);
    media_player_set_volume(&app->media, volume);
}

/**
 * @brief 设置全局 UI 与文本阅读字体大小。
 * @param app 浏览器上下文。
 * @param pixel_size 字体像素高度，自动限制在允许范围。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_set_font_size(struct browser_app *app, uint32_t pixel_size)
{
    if (app == NULL) {
        return -1;
    }
    if (pixel_size < BROWSER_FONT_MIN_SIZE) {
        pixel_size = BROWSER_FONT_MIN_SIZE;
    } else if (pixel_size > BROWSER_FONT_MAX_SIZE) {
        pixel_size = BROWSER_FONT_MAX_SIZE;
    }
    return font_manager_set_size(&app->fonts, &app->font, pixel_size);
}

/**
 * @brief 按步长调整全局 UI 与文本阅读字体大小。
 * @param app 浏览器上下文。
 * @param delta 像素高度增量。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_adjust_font_size(struct browser_app *app, int delta)
{
    uint32_t next_size;

    if (app == NULL) {
        return -1;
    }
    if (delta < 0 && (uint32_t)(-delta) > app->font.pixel_size) {
        next_size = 0;
    } else {
        next_size = (uint32_t)((int)app->font.pixel_size + delta);
    }
    return browser_app_set_font_size(app, next_size);
}

/**
 * @brief 保存当前音量、字体和文件排序设置。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_save_config(struct browser_app *app)
{
    struct audio_player_status status;

    if (app == NULL || app->config_path[0] == '\0') return -1;
    audio_player_get_status(&app->audio, &status);
    app->config.font_size = app->font.pixel_size;
    app->config.volume = status.volume;
    app->config.file_sort = app->file_sort;
    app->config.playback_mode = app->playback_mode;
    if (browser_config_save(app->config_path, &app->config) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "save browser config");
        return -1;
    }
    return 0;
}
