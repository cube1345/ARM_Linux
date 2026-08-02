#include "animation_decoder.h"
#include "audio_player.h"
#include "browser_app.h"
#include "browser_ui.h"
#include "bmp_display.h"
#include "file_list.h"
#include "font_renderer.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_decoder.h"
#include "image_render.h"
#include "input_keyboard.h"
#include "page_file.h"
#include "text_reader.h"
#include "ui_draw.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 输出程序用法。
 * @param program_name 程序名称。
 */
static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s <fb> <keyboard|-> <root> <font> "
            "[ALSA device] [touch device]\n", program_name);
}

/**
 * @brief 获取单调时钟毫秒值。
 * @return 单调时钟毫秒值。
 */
static uint64_t monotonic_ms(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) < 0) {
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000U +
           (uint64_t)value.tv_nsec / 1000000U;
}

/**
 * @brief 释放当前图片或 GIF 资源。
 * @param app 浏览器上下文。
 */
static void close_image(struct browser_app *app)
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
    return bmp_display_flush(&app->display);
}

/**
 * @brief 在当前目录选择相邻图片并加载资源。
 * @param app 浏览器上下文。
 * @param direction 正数向后，负数向前。
 * @return 找到并加载返回 1，无图片返回 0，失败返回 -1。
 */
static int select_adjacent_image(struct browser_app *app, int direction)
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
 * @brief 关闭媒体资源并返回文件列表。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int close_media_page(struct browser_app *app)
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

/**
 * @brief 按当前音频位置相对跳转。
 * @param app 浏览器上下文。
 * @param delta_percent 百分比增量。
 */
static void seek_relative(struct browser_app *app, int delta_percent)
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
 * @brief 处理媒体页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
static int handle_media_key(struct browser_app *app,
                            enum input_action action)
{
    if (action == INPUT_ACTION_EXIT) {
        return 1;
    }
    if (action == INPUT_ACTION_BACK) {
        return close_media_page(app);
    }
    if (app->page == BROWSER_PAGE_IMAGE) {
        if (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT) {
            int result = select_adjacent_image(
                app, action == INPUT_ACTION_NEXT ? 1 : -1);
            return result < 0 ? -1 : render_image_page(app);
        }
        if (action == INPUT_ACTION_ROTATE) {
            app->rotation = (app->rotation + 90U) % 360U;
            return render_image_page(app);
        }
    } else if (app->page == BROWSER_PAGE_TEXT &&
               (action == INPUT_ACTION_PREVIOUS ||
                action == INPUT_ACTION_NEXT)) {
        if (action == INPUT_ACTION_NEXT) {
            text_reader_next(&app->text);
        } else {
            text_reader_previous(&app->text);
        }
        return render_text_page(app);
    } else if (app->page == BROWSER_PAGE_AUDIO) {
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
    return 0;
}

/**
 * @brief 处理媒体页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
static int handle_media_touch(struct browser_app *app,
                              const struct browser_input *input)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;

    if (input->touch == TOUCH_ACTION_TAP && input->x < UI_BUTTON_SIZE &&
        input->y < UI_BUTTON_SIZE) {
        return close_media_page(app);
    }
    if (app->page == BROWSER_PAGE_IMAGE) {
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
    } else if (app->page == BROWSER_PAGE_TEXT &&
               input->touch == TOUCH_ACTION_SWIPE &&
               abs(input->dx) > abs(input->dy)) {
        if (input->dx < 0) {
            text_reader_next(&app->text);
        } else {
            text_reader_previous(&app->text);
        }
        return render_text_page(app);
    } else if (app->page == BROWSER_PAGE_AUDIO) {
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
    }
    return 0;
}

/**
 * @brief 推进 GIF 并定时刷新音频状态。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
static int update_periodic(struct browser_app *app, uint64_t now_ms)
{
    if (app->page == BROWSER_PAGE_IMAGE && app->gif.frame_count > 0 &&
        gif_animation_advance(&app->gif, now_ms)) {
        return render_image_page(app);
    }
    if (app->page == BROWSER_PAGE_AUDIO &&
        now_ms - app->last_audio_refresh_ms >= UI_AUDIO_REFRESH_MS) {
        return render_audio_page(app);
    }
    return 0;
}

/**
 * @brief 计算事件循环下一次等待时长。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return poll 等待毫秒数。
 */
static int event_timeout(const struct browser_app *app, uint64_t now_ms)
{
    int timeout = UI_AUDIO_REFRESH_MS;

    if (app->page == BROWSER_PAGE_IMAGE && app->gif.frame_count > 0) {
        timeout = gif_animation_timeout(&app->gif, now_ms, timeout);
    }
    return timeout;
}

/**
 * @brief 分发一次键盘或触摸输入。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
static int dispatch_input(struct browser_app *app,
                          const struct browser_input *input)
{
    if (input->action != INPUT_ACTION_NONE) {
        return app->page == BROWSER_PAGE_FILES ?
               handle_file_key(app, input->action) :
               handle_media_key(app, input->action);
    }
    if (input->touch != TOUCH_ACTION_NONE) {
        return app->page == BROWSER_PAGE_FILES ?
               handle_file_touch(app, input) :
               handle_media_touch(app, input);
    }
    return 0;
}

/**
 * @brief 运行浏览器事件循环。
 * @param app 浏览器上下文。
 * @return 成功退出返回 0，失败返回 -1。
 */
static int run_browser(struct browser_app *app)
{
    if (render_file_page(app) < 0) {
        return -1;
    }
    while (1) {
        struct browser_input input;
        uint64_t now = monotonic_ms();
        int wait_result = input_manager_wait(
            &app->input, &input, event_timeout(app, now));
        int result;

        if (wait_result < 0) {
            return -1;
        }
        result = wait_result > 0 ? dispatch_input(app, &input) : 0;
        if (result > 0) {
            return 0;
        }
        if (result < 0) {
            fprintf(stderr, "input action %s failed: %s\n",
                    input_action_name(input.action), strerror(errno));
        }
        if (update_periodic(app, monotonic_ms()) < 0) {
            return -1;
        }
    }
}

/**
 * @brief 多媒体文件浏览器入口。
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    struct browser_app app;
    const char *touch_path;
    int result = -1;

    if (argc < 5 || argc > 7) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    memset(&app, 0, sizeof(app));
    app.display.fd = -1;
    app.input.keyboard_fd = -1;
    app.input.touch_fd = -1;
    app.alsa_device = argc >= 6 ? argv[5] : "default";
    touch_path = argc >= 7 ? argv[6] : NULL;
    if (realpath(argv[3], app.root) == NULL ||
        file_list_scan(app.root, &app.files) < 0) {
        perror(argv[3]);
        return EXIT_FAILURE;
    }
    if (bmp_display_open(&app.display, argv[1]) < 0) {
        return EXIT_FAILURE;
    }
    if (font_renderer_open(&app.font, argv[4], 24) < 0) {
        goto cleanup_display;
    }
    if (input_manager_open(&app.input, argv[2], touch_path,
                           (int)app.display.variable_info.xres,
                           (int)app.display.variable_info.yres) < 0) {
        goto cleanup_font;
    }
    animation_decoder_manager_init(&app.animations);
    if (animation_decoder_register_builtin(&app.animations) < 0) {
        goto cleanup_input;
    }
    if (audio_player_init(&app.audio) < 0) {
        goto cleanup_input;
    }
    result = run_browser(&app);
    close_image(&app);
    text_reader_close(&app.text);
    audio_player_destroy(&app.audio);
cleanup_input:
    input_manager_close(&app.input);
cleanup_font:
    font_renderer_close(&app.font);
cleanup_display:
    bmp_display_close(&app.display);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
