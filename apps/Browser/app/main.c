#include "animation_decoder.h"
#include "audio_player.h"
#include "browser_app.h"
#include "browser_log.h"
#include "browser_ui.h"
#include "bmp_display.h"
#include "desktop_app.h"
#include "file_list.h"
#include "font_renderer.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_decoder.h"
#include "image_render.h"
#include "input_keyboard.h"
#include "page_manager.h"
#include "page_image.h"
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
            "Usage: %s <fb> <keyboard|stdin|-> <root> <font> "
            "[ALSA device] [touch|mouse device]\n", program_name);
}

/**
 * @brief 获取单调时钟毫秒值。
 * @return 单调时钟毫秒值。
 */
uint64_t monotonic_ms(void)
{
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) < 0) {
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000U +
           (uint64_t)value.tv_nsec / 1000000U;
}

/**
 * @brief 判断页面是否为媒体详情页面。
 * @param page 页面枚举。
 * @return 媒体详情页面返回 1，否则返回 0。
 */
static int browser_page_is_media(enum browser_page page)
{
    return page == BROWSER_PAGE_IMAGE ||
           page == BROWSER_PAGE_TEXT ||
           page == BROWSER_PAGE_AUDIO ||
           page == BROWSER_PAGE_VIDEO;
}

/**
 * @brief 执行当前页面的周期任务。
 * @param pages 页面管理器。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
static int update_periodic(const struct page_manager *pages,
                           struct browser_app *app, uint64_t now_ms)
{
    return page_manager_periodic(pages, app, now_ms);
}

/**
 * @brief 计算事件循环下一次等待时长。
 * @param pages 页面管理器。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return poll 等待毫秒数。
 */
static int event_timeout(const struct page_manager *pages,
                         const struct browser_app *app, uint64_t now_ms)
{
    int timeout = UI_AUDIO_REFRESH_MS;

    return page_manager_event_timeout(pages, app, now_ms, timeout);
}

/**
 * @brief 分发一次键盘或触摸输入。
 * @param pages 页面管理器。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
static int dispatch_input(const struct page_manager *pages,
                          struct browser_app *app,
                          const struct browser_input *input)
{
    if (input->action != INPUT_ACTION_NONE) {
        if (input->action == INPUT_ACTION_EXIT) {
            return 1;
        }
        if (input->action == INPUT_ACTION_BACK) {
            if (browser_page_is_media(app->page)) {
                return browser_app_close_media_page(app);
            }
            if (app->page == BROWSER_PAGE_DIAGNOSTICS ||
                app->page == BROWSER_PAGE_TOOLS ||
                app->page == BROWSER_PAGE_SETTINGS) {
                return browser_app_return_to_desktop(app);
            }
            if (app->page == BROWSER_PAGE_DESKTOP) {
                return 0;
            }
        }
        return page_manager_handle_key(pages, app, input->action);
    }
    if (input->touch != TOUCH_ACTION_NONE) {
        if (input->touch == TOUCH_ACTION_TAP &&
            input->x < UI_BUTTON_SIZE && input->y < UI_BUTTON_SIZE) {
            if (browser_page_is_media(app->page)) {
                return browser_app_close_media_page(app);
            }
            if (app->page == BROWSER_PAGE_DIAGNOSTICS ||
                app->page == BROWSER_PAGE_TOOLS ||
                app->page == BROWSER_PAGE_SETTINGS) {
                return browser_app_return_to_desktop(app);
            }
        }
        return page_manager_handle_touch(pages, app, input);
    }
    return 0;
}

/**
 * @brief 运行浏览器事件循环。
 * @param pages 页面管理器。
 * @param app 浏览器上下文。
 * @return 成功退出返回 0，失败返回 -1。
 */
static int run_browser(const struct page_manager *pages,
                       struct browser_app *app)
{
    if (page_manager_render(pages, app) < 0) {
        return -1;
    }
    while (1) {
        struct browser_input input;
        uint64_t now = monotonic_ms();
        int wait_result = input_manager_wait(
            &app->input, &input, event_timeout(pages, app, now));
        int result;

        if (wait_result < 0) {
            return -1;
        }
        result = wait_result > 0 ? dispatch_input(pages, app, &input) : 0;
        if (result > 0) {
            return 0;
        }
        if (result < 0) {
            browser_log(BROWSER_LOG_WARN, "input action %s failed: %s",
                        input_action_name(input.action), strerror(errno));
        }
        if (update_periodic(pages, app, monotonic_ms()) < 0) {
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
    struct page_manager pages;
    const char *touch_path;
    int result = -1;

    browser_log_init_from_env();
    if (argc < 5 || argc > 7) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    memset(&app, 0, sizeof(app));
    app.display.fd = -1;
    app.file_filter = FILE_LIST_FILTER_ALL;
    app.alsa_device = argc >= 6 ? argv[5] : "default";
    touch_path = argc >= 7 ? argv[6] : NULL;
    if (realpath(argv[3], app.root) == NULL ||
        file_list_scan(app.root, &app.files) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, argv[3]);
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
    if (image_decoder_prepare() < 0) {
        goto cleanup_input;
    }
    if (audio_player_init(&app.audio) < 0) {
        goto cleanup_input;
    }
    if (media_player_init(&app.media) < 0) {
        goto cleanup_audio_player;
    }
    desktop_app_manager_init(&app.desktop_apps);
    if (desktop_app_register_builtin(&app.desktop_apps) < 0) {
        goto cleanup_media_player;
    }
    page_manager_init(&pages);
    if (page_manager_register_builtin(&pages) < 0) {
        goto cleanup_media_player;
    }
    result = run_browser(&pages, &app);
cleanup_media_player:
    close_image(&app);
    text_reader_close(&app.text);
    image_data_destroy(&app.media_frame);
    media_player_destroy(&app.media);
cleanup_audio_player:
    audio_player_destroy(&app.audio);
cleanup_input:
    input_manager_close(&app.input);
cleanup_font:
    font_renderer_close(&app.font);
cleanup_display:
    bmp_display_close(&app.display);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
