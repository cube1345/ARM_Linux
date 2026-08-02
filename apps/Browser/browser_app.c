#include "browser_app.h"

#include "audio_player.h"
#include "page_file.h"
#include "page_image.h"
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
    }
    app->page = BROWSER_PAGE_FILES;
    return render_file_page(app);
}
