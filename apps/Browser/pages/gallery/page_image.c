#include "page_image.h"

#include "animation_decoder.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "file_list.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_decoder.h"
#include "image_render.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SLIDESHOW_INTERVAL_MS 3000U
#define IMAGE_AUTOPLAY_BUTTON_WIDTH 118
#define IMAGE_ROTATE_BUTTON_WIDTH 118
#define IMAGE_ROTATE_BUTTON_HEIGHT 42
#define IMAGE_ROTATE_BUTTON_MARGIN 14

/**
 * @brief 获取图片页旋转按钮左上角 X 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 X 坐标。
 */
static int image_rotate_button_x(const struct browser_app *app)
{
    return (int)app->display.variable_info.xres - IMAGE_ROTATE_BUTTON_WIDTH -
           IMAGE_ROTATE_BUTTON_MARGIN;
}

/**
 * @brief 获取图片页旋转按钮左上角 Y 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 Y 坐标。
 */
static int image_rotate_button_y(const struct browser_app *app)
{
    return (int)app->display.variable_info.yres - UI_FOOTER_HEIGHT -
           IMAGE_ROTATE_BUTTON_HEIGHT - IMAGE_ROTATE_BUTTON_MARGIN;
}

/**
 * @brief 获取图片页自动播放按钮左上角 X 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 X 坐标。
 */
static int image_autoplay_button_x(const struct browser_app *app)
{
    return image_rotate_button_x(app) - IMAGE_AUTOPLAY_BUTTON_WIDTH -
           IMAGE_ROTATE_BUTTON_MARGIN;
}

/**
 * @brief 获取图片页自动播放按钮左上角 Y 坐标。
 * @param app 浏览器上下文。
 * @return 按钮 Y 坐标。
 */
static int image_autoplay_button_y(const struct browser_app *app)
{
    return image_rotate_button_y(app);
}

/**
 * @brief 重置下一次自动切图时间。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 */
static void schedule_slideshow(struct browser_app *app, uint64_t now_ms)
{
    app->next_slideshow_ms = app->slideshow_enabled ?
                             now_ms + IMAGE_SLIDESHOW_INTERVAL_MS : 0;
}

/**
 * @brief 在切换图片时保留自动播放开关状态。
 * @param app 浏览器上下文。
 */
static void preserve_slideshow_after_load(struct browser_app *app)
{
    if (app->slideshow_enabled) {
        schedule_slideshow(app, monotonic_ms());
    }
}

/**
 * @brief 判断文件类型是否可用静态图片缓存预解码。
 * @param type 文件类型。
 * @return 可预解码返回 1，否则返回 0。
 */
static int is_static_image_type(enum file_type type)
{
    return type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG ||
           type == FILE_TYPE_PNG;
}

/**
 * @brief 查找相邻图片条目索引。
 * @param app 浏览器上下文。
 * @param direction 正数向后，负数向前。
 * @param output 输出索引。
 * @return 找到返回 1，未找到返回 0。
 */
static int find_adjacent_image_index(const struct browser_app *app,
                                     int direction, size_t *output)
{
    size_t checked;
    size_t index = app->selected;

    for (checked = 0; checked < app->files.count; checked++) {
        index = direction > 0 ? (index + 1U) % app->files.count :
                (index + app->files.count - 1U) % app->files.count;
        if (browser_file_type_is_image(app->files.entries[index].type)) {
            *output = index;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 等待图片预解码线程结束。
 * @param app 浏览器上下文。
 */
static void join_image_preload(struct browser_app *app)
{
    if (app->preload_thread_created) {
        pthread_join(app->preload_thread, NULL);
        app->preload_thread_created = 0;
    }
}

/**
 * @brief 清空图片预解码缓存状态。
 * @param app 浏览器上下文。
 */
static void reset_image_preload(struct browser_app *app)
{
    image_data_destroy(&app->preloaded_image);
    app->preload_ready = 0;
    app->preload_result = 0;
    app->preloaded_index = 0;
    app->preloaded_type = FILE_TYPE_UNKNOWN;
    app->preloaded_path[0] = '\0';
}

/**
 * @brief 停止并丢弃图片预解码缓存。
 * @param app 浏览器上下文。
 */
static void discard_image_preload(struct browser_app *app)
{
    join_image_preload(app);
    reset_image_preload(app);
}

/**
 * @brief 释放当前正在显示的图片或 GIF。
 * @param app 浏览器上下文。
 */
static void close_active_image(struct browser_app *app)
{
    image_data_destroy(&app->image);
    gif_animation_close(&app->gif);
    app->rotation = 0;
}

/**
 * @brief 图片预解码线程入口。
 * @param argument 浏览器上下文。
 * @return 始终返回 NULL。
 */
static void *image_preload_thread(void *argument)
{
    struct browser_app *app = argument;

    app->preload_result = image_decode(app->preloaded_path,
                                       app->preloaded_type,
                                       &app->preloaded_image);
    app->preload_ready = app->preload_result == 0;
    return NULL;
}

/**
 * @brief 启动下一张静态图片预解码。
 * @param app 浏览器上下文。
 */
static void start_image_preload(struct browser_app *app)
{
    size_t index;
    enum file_type type;

    discard_image_preload(app);
    if (!find_adjacent_image_index(app, 1, &index) || index == app->selected) {
        return;
    }
    type = app->files.entries[index].type;
    if (!is_static_image_type(type) ||
        file_list_path(&app->files, index, app->preloaded_path,
                       sizeof(app->preloaded_path)) < 0) {
        return;
    }
    app->preloaded_index = index;
    app->preloaded_type = type;
    if (pthread_create(&app->preload_thread, NULL,
                       image_preload_thread, app) == 0) {
        app->preload_thread_created = 1;
    } else {
        reset_image_preload(app);
    }
}

/**
 * @brief 尝试消费指定索引的预解码缓存。
 * @param app 浏览器上下文。
 * @param index 目标文件索引。
 * @return 命中并消费返回 1，否则返回 0。
 */
static int consume_image_preload(struct browser_app *app, size_t index)
{
    join_image_preload(app);
    if (app->preload_ready && app->preload_result == 0 &&
        app->preloaded_index == index) {
        close_active_image(app);
        app->image = app->preloaded_image;
        memset(&app->preloaded_image, 0, sizeof(app->preloaded_image));
        memcpy(app->current_path, app->preloaded_path,
               sizeof(app->current_path));
        app->current_path[sizeof(app->current_path) - 1U] = '\0';
        app->selected = index;
        reset_image_preload(app);
        return 1;
    }
    reset_image_preload(app);
    return 0;
}

/**
 * @brief 加载指定索引图片并刷新预解码缓存。
 * @param app 浏览器上下文。
 * @param index 目标文件索引。
 * @return 成功返回 0，失败返回 -1。
 */
static int load_image_at(struct browser_app *app, size_t index)
{
    enum file_type type = app->files.entries[index].type;
    int slideshow_enabled = app->slideshow_enabled;

    if (is_static_image_type(type) && consume_image_preload(app, index)) {
        app->slideshow_enabled = slideshow_enabled;
        preserve_slideshow_after_load(app);
        start_image_preload(app);
        return 0;
    }
    discard_image_preload(app);
    close_active_image(app);
    app->slideshow_enabled = slideshow_enabled;
    app->selected = index;
    if (file_list_path(&app->files, index,
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
    } else if (image_decode(app->current_path, type, &app->image) < 0) {
        return -1;
    }
    preserve_slideshow_after_load(app);
    start_image_preload(app);
    return 0;
}

/**
 * @brief 释放当前图片或 GIF 资源。
 * @param app 浏览器上下文。
 */
void close_image(struct browser_app *app)
{
    discard_image_preload(app);
    close_active_image(app);
    app->slideshow_enabled = 0;
    app->next_slideshow_ms = 0;
}

/**
 * @brief 解码当前选择的普通图片或 GIF。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int load_selected_image(struct browser_app *app)
{
    return load_image_at(app, app->selected);
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
    int rotate_x = image_rotate_button_x(app);
    int rotate_y = image_rotate_button_y(app);
    int autoplay_x = image_autoplay_button_x(app);
    int autoplay_y = image_autoplay_button_y(app);
    char autoplay_label[16];

    if (image == NULL || image_render_draw(&app->display, image,
                                            app->rotation) < 0) {
        return -1;
    }
    snprintf(autoplay_label, sizeof(autoplay_label), "AUTO %s",
             app->slideshow_enabled ? "ON" : "OFF");
    browser_ui_draw_back_button(&app->display, &app->font);
    browser_ui_draw_button(&app->display, &app->font, autoplay_x, autoplay_y,
                           IMAGE_AUTOPLAY_BUTTON_WIDTH,
                           IMAGE_ROTATE_BUTTON_HEIGHT,
                           autoplay_label, UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font, rotate_x, rotate_y,
                           IMAGE_ROTATE_BUTTON_WIDTH,
                           IMAGE_ROTATE_BUTTON_HEIGHT, "ROTATE", UI_HEADER);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "←/→ switch  Space auto  R rotate  Esc back");
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
    size_t index;

    if (find_adjacent_image_index(app, direction, &index)) {
        return load_image_at(app, index) < 0 ? -1 : 1;
    }
    return 0;
}

/**
 * @brief 执行图片页面周期任务。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
int update_image_page(struct browser_app *app, uint64_t now_ms)
{
    int redraw = 0;

    if (app->gif.frame_count > 0 &&
        gif_animation_advance(&app->gif, now_ms)) {
        redraw = 1;
    }
    if (app->slideshow_enabled && now_ms >= app->next_slideshow_ms) {
        int result = select_adjacent_image(app, 1);

        schedule_slideshow(app, now_ms);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            redraw = 1;
        }
    }
    return redraw ? render_image_page(app) : 0;
}

/**
 * @brief 由图片页面状态调整事件等待时间。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @param current_timeout 当前等待时间。
 * @return 调整后的等待时间。
 */
int image_page_event_timeout(const struct browser_app *app, uint64_t now_ms,
                             int current_timeout)
{
    int timeout = current_timeout;

    if (app->gif.frame_count > 0) {
        timeout = gif_animation_timeout(&app->gif, now_ms, timeout);
    }
    if (app->slideshow_enabled) {
        uint64_t remaining;

        if (app->next_slideshow_ms <= now_ms) {
            return 0;
        }
        remaining = app->next_slideshow_ms - now_ms;
        if (remaining > (uint64_t)INT_MAX) {
            remaining = INT_MAX;
        }
        if ((int)remaining < timeout) {
            timeout = (int)remaining;
        }
    }
    return timeout;
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

        if (result > 0) {
            preserve_slideshow_after_load(app);
        }
        return result < 0 ? -1 : render_image_page(app);
    }
    if (action == INPUT_ACTION_ROTATE) {
        app->rotation = (app->rotation + 90U) % 360U;
        return render_image_page(app);
    }
    if (action == INPUT_ACTION_TOGGLE) {
        app->slideshow_enabled = !app->slideshow_enabled;
        schedule_slideshow(app, monotonic_ms());
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
    int rotate_x = image_rotate_button_x(app);
    int rotate_y = image_rotate_button_y(app);
    int autoplay_x = image_autoplay_button_x(app);
    int autoplay_y = image_autoplay_button_y(app);

    if (input->touch == TOUCH_ACTION_TAP &&
        input->x >= autoplay_x &&
        input->x < autoplay_x + IMAGE_AUTOPLAY_BUTTON_WIDTH &&
        input->y >= autoplay_y &&
        input->y < autoplay_y + IMAGE_ROTATE_BUTTON_HEIGHT) {
        app->slideshow_enabled = !app->slideshow_enabled;
        schedule_slideshow(app, monotonic_ms());
        return render_image_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->x >= rotate_x &&
        input->x < rotate_x + IMAGE_ROTATE_BUTTON_WIDTH &&
        input->y >= rotate_y &&
        input->y < rotate_y + IMAGE_ROTATE_BUTTON_HEIGHT) {
        app->rotation = (app->rotation + 90U) % 360U;
        return render_image_page(app);
    }
    if (input->touch == TOUCH_ACTION_SWIPE &&
        abs(input->dx) > abs(input->dy)) {
        int result = select_adjacent_image(app, input->dx < 0 ? 1 : -1);

        if (result > 0) {
            preserve_slideshow_after_load(app);
        }
        return result < 0 ? -1 : render_image_page(app);
    }
    return 0;
}
