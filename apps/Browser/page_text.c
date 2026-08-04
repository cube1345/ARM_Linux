#include "page_text.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "text_reader.h"

#include <stdlib.h>

/**
 * @brief 绘制文本页并叠加返回按钮。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_text_page(struct browser_app *app)
{
    if (text_reader_render(&app->text, &app->display, &app->font) < 0) {
        return -1;
    }
    browser_ui_draw_back_button(&app->display, &app->font);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "←/→ page  Swipe page  Esc back");
    return bmp_display_flush(&app->display);
}

/**
 * @brief 处理文本页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_text_key(struct browser_app *app, enum input_action action)
{
    if (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT) {
        if (action == INPUT_ACTION_NEXT) {
            text_reader_next(&app->text);
        } else {
            text_reader_previous(&app->text);
        }
        return render_text_page(app);
    }
    return 0;
}

/**
 * @brief 处理文本页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_text_touch(struct browser_app *app,
                      const struct browser_input *input)
{
    if (input->touch == TOUCH_ACTION_SWIPE &&
        abs(input->dx) > abs(input->dy)) {
        if (input->dx < 0) {
            text_reader_next(&app->text);
        } else {
            text_reader_previous(&app->text);
        }
        return render_text_page(app);
    }
    return 0;
}
