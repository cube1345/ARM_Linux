#include "animation_decoder.h"
#include "audio_player.h"
#include "browser_app.h"
#include "browser_config.h"
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
#include "page_gallery.h"
#include "page_file.h"
#include "page_image.h"
#include "page_video.h"
#include "plugin_manager.h"
#include "text_reader.h"
#include "ui_draw.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t browser_shutdown_requested;

#define BROWSER_SCREEN_IDLE_DEFAULT_SECONDS 300U
#define BROWSER_WATCHDOG_DEFAULT_SECONDS 15U

/**
 * @brief 记录终止信号并交给主循环执行资源清理。
 * @param signal_number 收到的信号编号。
 */
static void handle_shutdown_signal(int signal_number)
{
    (void)signal_number;
    browser_shutdown_requested = 1;
}

/**
 * @brief 安装可安全退出的终止信号处理器。
 * @return 成功返回 0，失败返回 -1。
 */
static int install_shutdown_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0 ||
        sigaction(SIGHUP, &action, NULL) < 0) {
        return -1;
    }
    return 0;
}

/**
 * @brief 输出程序用法。
 * @param program_name 程序名称。
 */
static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s <fb> <keyboard|stdin|auto|-> <root> <font> "
            "[ALSA device] [touch|mouse|auto device]\n", program_name);
}

/**
 * @brief 将运行参数复制到可持久化配置字段。
 * @param output 输出缓冲区。
 * @param output_size 输出缓冲区大小。
 * @param value 参数值，NULL 记录为空字符串。
 * @return 成功返回 0，参数过长返回 -1。
 */
static int copy_runtime_setting(char *output, size_t output_size,
                                const char *value)
{
    int written = snprintf(output, output_size, "%s",
                           value == NULL ? "" : value);

    if (written < 0 || (size_t)written >= output_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

/**
 * @brief 从环境变量读取屏幕空闲休眠时长。
 * @return 空闲毫秒数，0 表示禁用。
 */
static uint64_t screen_idle_timeout_ms(void)
{
    const char *text = getenv("BROWSER_SCREEN_IDLE_SECONDS");
    char *end = NULL;
    unsigned long long seconds;

    if (text == NULL || text[0] == '\0') {
        return BROWSER_SCREEN_IDLE_DEFAULT_SECONDS * 1000U;
    }
    errno = 0;
    seconds = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        seconds > UINT64_MAX / 1000U) {
        browser_log(BROWSER_LOG_WARN,
                    "invalid BROWSER_SCREEN_IDLE_SECONDS: %s", text);
        return BROWSER_SCREEN_IDLE_DEFAULT_SECONDS * 1000U;
    }
    return (uint64_t)seconds * 1000U;
}

/**
 * @brief 从环境变量读取 watchdog 超时秒数。
 * @return 超时秒数，0 表示禁用。
 */
static unsigned int watchdog_timeout_seconds(void)
{
    const char *text = getenv("BROWSER_WATCHDOG_SECONDS");
    char *end = NULL;
    unsigned long value;

    if (text == NULL || text[0] == '\0') {
        return BROWSER_WATCHDOG_DEFAULT_SECONDS;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > UINT_MAX) {
        browser_log(BROWSER_LOG_WARN, "invalid BROWSER_WATCHDOG_SECONDS: %s",
                    text);
        return BROWSER_WATCHDOG_DEFAULT_SECONDS;
    }
    return (unsigned int)value;
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
 * @brief 处理当前文件目录的 inotify 变化。
 * @param app 浏览器上下文。
 * @return 成功返回 0，文件页刷新失败返回 -1。
 */
static int update_file_watcher(struct browser_app *app)
{
    int changed;

    if (app->watcher.fd < 0 || app->page != BROWSER_PAGE_FILES ||
        app->file_view != BROWSER_FILE_VIEW_DIRECTORY ||
        app->active_app == DESKTOP_APP_GALLERY) {
        return 0;
    }
    if (file_watcher_update(&app->watcher, app->files.directory) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "watch media directory");
        file_watcher_destroy(&app->watcher);
        return 0;
    }
    changed = file_watcher_consume(&app->watcher);
    if (changed < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "read media directory changes");
        file_watcher_destroy(&app->watcher);
        return 0;
    }
    return changed > 0 ? refresh_file_page_after_change(app) : 0;
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
    int power_result;
    int watchdog_result = watchdog_manager_update(&app->watchdog, now_ms);

    if (watchdog_result < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "watchdog keepalive");
    }

    if (app->page == BROWSER_PAGE_VIDEO ||
        (app->page == BROWSER_PAGE_IMAGE && app->slideshow_enabled)) {
        power_result = screen_power_manager_activity(&app->screen_power,
                                                     now_ms);
    } else {
        power_result = screen_power_manager_update(&app->screen_power,
                                                   now_ms);
    }
    if (power_result < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "screen power control");
    }
    if (update_file_watcher(app) < 0) return -1;
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
    int power_result = screen_power_manager_activity(&app->screen_power,
                                                     monotonic_ms());

    if (power_result > 0) return 0;
    if (power_result < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "wake screen");
    }
    if (input->text_length > 0 && app->page == BROWSER_PAGE_FILES &&
        app->search_active) {
        return handle_file_text(app, input->text, input->text_length);
    }
    if (input->action != INPUT_ACTION_NONE) {
        if (input->action == INPUT_ACTION_EXIT) {
            return 1;
        }
        if (input->action == INPUT_ACTION_BACK) {
            if (app->page == BROWSER_PAGE_VIDEO && app->video_fullscreen) {
                app->video_fullscreen = 0;
                return render_video_page(app);
            }
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
            input->x < UI_BUTTON_SIZE && input->y < UI_BUTTON_SIZE &&
            !(app->page == BROWSER_PAGE_VIDEO && app->video_fullscreen)) {
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
    browser_shutdown_requested = 0;
    if (page_manager_render(pages, app) < 0) {
        return -1;
    }
    while (!browser_shutdown_requested) {
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
    browser_log(BROWSER_LOG_INFO, "shutdown signal received; cleaning up");
    return 0;
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
    struct browser_plugin_manager plugins;
    struct browser_plugin_host plugin_host;
    const char *touch_path;
    const char *plugin_directory;
    int plugins_ready = 0;
    int display_opened = 0;
    int font_opened = 0;
    int input_opened = 0;
    int runtime_started = 0;
    int result = -1;

    browser_log_init_from_env();
    if (browser_log_install_crash_handler(
            getenv("BROWSER_CRASH_LOG_PATH")) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "install crash logger");
    }
    if (install_shutdown_handlers() < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "install signal handlers");
    }
    if (argc < 5 || argc > 7) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    memset(&app, 0, sizeof(app));
    app.display.fd = -1;
    app.watcher.fd = -1;
    app.watcher.watch_descriptor = -1;
    app.file_filter = FILE_LIST_FILTER_ALL;
    browser_config_defaults(&app.config);
    if (browser_config_path(app.config_path, sizeof(app.config_path)) < 0 ||
        browser_config_load(app.config_path, &app.config) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "load browser config");
        browser_config_defaults(&app.config);
    }
    app.file_sort = app.config.file_sort;
    app.playback_mode = app.config.playback_mode;
    browser_ui_set_theme(app.config.ui_theme);
    app.alsa_device = argc >= 6 ? argv[5] : "default";
    touch_path = argc >= 7 ? argv[6] : NULL;
    if (copy_runtime_setting(app.config.keyboard_path,
                             sizeof(app.config.keyboard_path), argv[2]) < 0 ||
        copy_runtime_setting(app.config.touch_path,
                             sizeof(app.config.touch_path), touch_path) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "input device path");
        return EXIT_FAILURE;
    }
    display_manager_init(&app.display_devices);
    font_manager_init(&app.fonts);
    debug_manager_init(&app.debug);
    if (display_manager_register_builtin(&app.display_devices) < 0 ||
        font_manager_register_builtin(&app.fonts) < 0 ||
        debug_manager_register_builtin(&app.debug) < 0) {
        return EXIT_FAILURE;
    }
    animation_decoder_manager_init(&app.animations);
    if (animation_decoder_register_builtin(&app.animations) < 0 ||
        image_decoder_prepare() < 0) {
        return EXIT_FAILURE;
    }
    if (audio_player_init(&app.audio) < 0) {
        return EXIT_FAILURE;
    }
    if (media_player_init(&app.media) < 0) {
        goto cleanup_audio_player;
    }
    browser_app_set_volume(&app, app.config.volume);
    desktop_app_manager_init(&app.desktop_apps);
    if (desktop_app_register_builtin(&app.desktop_apps) < 0) {
        goto cleanup_media_player;
    }
    page_manager_init(&pages);
    if (page_manager_register_builtin(&pages) < 0 ||
        image_decoder_default_manager() == NULL) {
        goto cleanup_media_player;
    }
    browser_plugin_manager_init(&plugins);
    plugins_ready = 1;
    browser_plugin_host_init(&plugin_host,
                             image_decoder_default_manager(),
                             &app.audio.backends, &pages, &app.desktop_apps,
                             &app.display_devices);
    plugin_directory = getenv("BROWSER_PLUGIN_DIR");
    if (plugin_directory == NULL || plugin_directory[0] == '\0') {
        plugin_directory = "/usr/lib/media-browser/plugins";
    }
    if (strcmp(plugin_directory, "-") != 0 &&
        browser_plugin_manager_load(&plugins, plugin_directory,
                                    &plugin_host) < 0) {
        goto cleanup_media_player;
    }
    if (realpath(argv[3], app.root) == NULL ||
        file_list_scan(app.root, &app.files) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, argv[3]);
        goto cleanup_media_player;
    }
    if (file_watcher_init(&app.watcher) < 0 ||
        file_watcher_update(&app.watcher, app.files.directory) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "initialize media directory watch");
        file_watcher_destroy(&app.watcher);
    }
    if (copy_runtime_setting(app.config.media_root,
                             sizeof(app.config.media_root), app.root) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "media root");
        goto cleanup_media_player;
    }
    if (display_manager_open(&app.display_devices, &app.display,
                             argv[1]) < 0) {
        goto cleanup_media_player;
    }
    display_opened = 1;
    screen_power_manager_init_framebuffer(&app.screen_power, app.display.fd,
                                          screen_idle_timeout_ms(),
                                          monotonic_ms());
    if (font_manager_open(&app.fonts, &app.font, argv[4],
                          app.config.font_size) < 0) {
        goto cleanup_media_player;
    }
    font_opened = 1;
    if (input_manager_open(&app.input, argv[2], touch_path,
                           (int)app.display.variable_info.xres,
                           (int)app.display.variable_info.yres) < 0) {
        goto cleanup_media_player;
    }
    input_opened = 1;
    if (watchdog_manager_open(&app.watchdog,
                              getenv("BROWSER_WATCHDOG_DEVICE"),
                              watchdog_timeout_seconds(), monotonic_ms()) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "open watchdog");
    }
    runtime_started = 1;
    result = run_browser(&pages, &app);
cleanup_media_player:
    if (runtime_started) {
        browser_app_remember_playback(&app);
        (void)browser_app_save_config(&app);
    }
    gallery_cache_clear(&app);
    close_image(&app);
    text_reader_close(&app.text);
    subtitle_track_close(&app.subtitles);
    image_data_destroy(&app.media_frame);
    media_player_destroy(&app.media);
cleanup_audio_player:
    audio_player_destroy(&app.audio);
    if (input_opened) {
        input_manager_close(&app.input);
    }
    file_watcher_destroy(&app.watcher);
    if (font_opened) {
        font_manager_close(&app.fonts, &app.font);
    }
    if (display_opened) {
        screen_power_manager_destroy(&app.screen_power);
        display_manager_close(&app.display_devices, &app.display);
    }
    watchdog_manager_destroy(&app.watchdog);
    if (plugins_ready) {
        browser_plugin_manager_destroy(&plugins);
    }
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
