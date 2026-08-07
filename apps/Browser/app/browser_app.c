#include "browser_app.h"

#include "audio_player.h"
#include "browser_config.h"
#include "browser_log.h"
#include "page_desktop.h"
#include "page_file.h"
#include "page_image.h"
#include "page_video.h"
#include "text_reader.h"

/**
 * @brief 关闭当前媒体资源并返回文件列表页。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_close_media_page(struct browser_app *app)
{
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
    if (browser_config_save(app->config_path, &app->config) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "save browser config");
        return -1;
    }
    return 0;
}
