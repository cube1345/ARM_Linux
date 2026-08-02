#include "page_image.h"

#include "animation_decoder.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "file_list.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_decoder.h"
#include "image_render.h"
#include "ui_draw.h"

#include <stdlib.h>

/**
 * @brief 释放当前图片或 GIF 资源。
 * @param app 浏览器上下文。
 */
void close_image(struct browser_app *app)
{
    image_data_destroy(&app->image);
    gif_animation_close(&app->gif);
    app->rotation = 0;
}

/**
 * @brief 解码当前选择的普通图片或 GIF。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int load_selected_image(struct browser_app *app)
{
    enum file_type type = app->files.entries[app->selected].type;

    close_image(app);
    if (file_list_path(&app->files, app->selected,
                       app->current_path, sizeof(app->current_path)) < 0) {
        return -1;
    }
    if (type == FILE_TYPE_GIF) {
        if (animation_decoder_manager_open(&app->animations,
                                           app->current_path, type,
                                           &app->gif) < 0) {
            return -1;
        }
        gif_animation_reset(&app->gif, monotonic_ms());
        return 0;
    }
    return image_decode(app->current_path, type, &app->image);
}

/**
 * @brief 绘制当前普通图片或 GIF 帧及图片工具按钮。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_image_page(struct browser_app *app)
{
    const struct image_data *image = app->gif.frame_count > 0 ?
                                     gif_animation_current(&app->gif) :
                                     &app->image;
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;

    if (image == NULL || image_render_draw(&app->display, image,
                                            app->rotation) < 0) {
        return -1;
    }
    browser_ui_draw_back_button(&app->display, &app->font);
    ui_draw_rect(&app->display, width - 116, height - UI_BUTTON_SIZE,
                 116, UI_BUTTON_SIZE, UI_HEADER);
    ui_draw_text(&app->display, &app->font, "ROTATE", width - 106,
                 height - 15, 100, UI_TEXT, UI_HEADER);
    return bmp_display_flush(&app->display);
}

/**
 * @brief 在当前目录选择相邻图片并加载资源。
 * @param app 浏览器上下文。
 * @param direction 正数向后，负数向前。
 * @return 找到并加载返回 1，无图片返回 0，失败返回 -1。
 */
int select_adjacent_image(struct browser_app *app, int direction)
{
    size_t checked;
    size_t index = app->selected;

    for (checked = 0; checked < app->files.count; checked++) {
        index = direction > 0 ? (index + 1U) % app->files.count :
                (index + app->files.count - 1U) % app->files.count;
        if (browser_file_type_is_image(app->files.entries[index].type)) {
            app->selected = index;
            return load_selected_image(app) < 0 ? -1 : 1;
        }
    }
    return 0;
}

/**
 * @brief 处理图片页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_image_key(struct browser_app *app, enum input_action action)
{
    if (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT) {
        int result = select_adjacent_image(
            app, action == INPUT_ACTION_NEXT ? 1 : -1);

        return result < 0 ? -1 : render_image_page(app);
    }
    if (action == INPUT_ACTION_ROTATE) {
        app->rotation = (app->rotation + 90U) % 360U;
        return render_image_page(app);
    }
    return 0;
}

/**
 * @brief 处理图片页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_image_touch(struct browser_app *app,
                       const struct browser_input *input)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;

    if (input->touch == TOUCH_ACTION_TAP && input->x >= width - 116 &&
        input->y >= height - UI_BUTTON_SIZE) {
        app->rotation = (app->rotation + 90U) % 360U;
        return render_image_page(app);
    }
    if (input->touch == TOUCH_ACTION_SWIPE &&
        abs(input->dx) > abs(input->dy)) {
        int result = select_adjacent_image(app, input->dx < 0 ? 1 : -1);

        return result < 0 ? -1 : render_image_page(app);
    }
    return 0;
}
