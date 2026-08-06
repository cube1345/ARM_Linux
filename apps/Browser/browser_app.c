#include "browser_app.h"

#include "audio_player.h"
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
    app->page = BROWSER_PAGE_DESKTOP;
    return render_desktop_page(app);
}
