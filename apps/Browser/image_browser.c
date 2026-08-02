#include "audio_player.h"
#include "bmp_display.h"
#include "file_list.h"
#include "font_renderer.h"
#include "image_data.h"
#include "image_decoder.h"
#include "image_render.h"
#include "input_keyboard.h"
#include "text_reader.h"
#include "ui_draw.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UI_BACKGROUND 0x15191dU
#define UI_HEADER 0x20262cU
#define UI_SELECTED 0x2d6a78U
#define UI_TEXT 0xf2f5f7U
#define UI_MUTED 0xaab5bdU
#define UI_ACCENT 0x4fc3a1U
#define UI_MARGIN 20
#define UI_HEADER_HEIGHT 54

/** @brief 浏览器当前页面。 */
enum browser_page {
    BROWSER_PAGE_FILES = 0,
    BROWSER_PAGE_IMAGE,
    BROWSER_PAGE_TEXT,
    BROWSER_PAGE_AUDIO
};

/** @brief 多媒体文件浏览器完整运行上下文。 */
struct browser_app {
    struct bmp_display display;
    struct input_keyboard keyboard;
    struct font_renderer font;
    struct file_list files;
    struct text_reader text;
    struct audio_player audio;
    char root[PATH_MAX];
    char current_path[PATH_MAX];
    const char *alsa_device;
    size_t selected;
    enum browser_page page;
};

/**
 * @brief 输出程序用法。
 *
 * @param program_name 程序名称。
 */
static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s <fb device> <input device> <root dir> <font> "
            "[ALSA device]\n", program_name);
}

/**
 * @brief 判断文件类型是否为图片。
 *
 * @param type 文件类型。
 * @return 是图片返回 1，否则返回 0。
 */
static int is_image(enum file_type type)
{
    return type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG;
}

/**
 * @brief 绘制文件列表页面。
 *
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int render_file_page(struct browser_app *app)
{
    int row_height = (int)app->font.pixel_size + 14;
    int available = (int)app->display.variable_info.yres - UI_HEADER_HEIGHT;
    size_t visible = available > 0 ? (size_t)(available / row_height) : 1;
    size_t first;
    size_t index;

    if (visible == 0) {
        visible = 1;
    }
    first = app->selected / visible * visible;
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    ui_draw_rect(&app->display, 0, 0,
                 (int)app->display.variable_info.xres,
                 UI_HEADER_HEIGHT, UI_HEADER);
    ui_draw_text(&app->display, &app->font, app->files.directory,
                 UI_MARGIN, (int)app->font.pixel_size + 14,
                 (int)app->display.variable_info.xres - UI_MARGIN * 2,
                 UI_TEXT, UI_HEADER);
    for (index = first; index < app->files.count && index < first + visible;
         index++) {
        int y = UI_HEADER_HEIGHT + (int)(index - first) * row_height;
        uint32_t background = index == app->selected ?
                              UI_SELECTED : UI_BACKGROUND;
        char line[FILE_LIST_NAME_SIZE + 16];

        if (index == app->selected) {
            ui_draw_rect(&app->display, 0, y,
                         (int)app->display.variable_info.xres,
                         row_height, background);
        }
        snprintf(line, sizeof(line), "%-3s  %s",
                 file_type_name(app->files.entries[index].type),
                 app->files.entries[index].name);
        ui_draw_text(&app->display, &app->font, line, UI_MARGIN,
                     y + (int)app->font.pixel_size + 5,
                     (int)app->display.variable_info.xres - UI_MARGIN * 2,
                     index == app->selected ? UI_TEXT : UI_MUTED,
                     background);
    }
    if (app->files.count == 0) {
        ui_draw_text(&app->display, &app->font, "Empty directory",
                     UI_MARGIN, UI_HEADER_HEIGHT +
                     (int)app->font.pixel_size + 10,
                     (int)app->display.variable_info.xres - UI_MARGIN * 2,
                     UI_MUTED, UI_BACKGROUND);
    }
    return bmp_display_flush(&app->display);
}

/**
 * @brief 解码并显示当前图片。
 *
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int render_image_page(struct browser_app *app)
{
    struct image_data image = {0};
    enum file_type type = app->files.entries[app->selected].type;
    int result;

    if (file_list_path(&app->files, app->selected,
                       app->current_path, sizeof(app->current_path)) < 0 ||
        image_decode(app->current_path, type, &image) < 0) {
        return -1;
    }
    result = image_render_draw(&app->display, &image);
    image_data_destroy(&image);
    return result;
}

/**
 * @brief 绘制音频播放页面。
 *
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int render_audio_page(struct browser_app *app)
{
    const char *name = app->files.entries[app->selected].name;
    const char *state = audio_player_state_name(
        audio_player_get_state(&app->audio));
    int center_y = (int)app->display.variable_info.yres / 2;

    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, "WAV PLAYER", UI_MARGIN,
                 center_y - (int)app->font.pixel_size,
                 (int)app->display.variable_info.xres - UI_MARGIN * 2,
                 UI_ACCENT, UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, name, UI_MARGIN, center_y + 12,
                 (int)app->display.variable_info.xres - UI_MARGIN * 2,
                 UI_TEXT, UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, state, UI_MARGIN,
                 center_y + (int)app->font.pixel_size + 34,
                 (int)app->display.variable_info.xres - UI_MARGIN * 2,
                 UI_MUTED, UI_BACKGROUND);
    return bmp_display_flush(&app->display);
}

/**
 * @brief 在当前目录中选择上一张或下一张图片。
 *
 * @param app 浏览器上下文。
 * @param direction 正数向后，负数向前。
 * @return 找到图片返回 1，没有其他图片返回 0。
 */
static int select_adjacent_image(struct browser_app *app, int direction)
{
    size_t checked;
    size_t index = app->selected;

    for (checked = 0; checked < app->files.count; checked++) {
        index = direction > 0 ? (index + 1) % app->files.count :
                (index + app->files.count - 1) % app->files.count;
        if (is_image(app->files.entries[index].type)) {
            app->selected = index;
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 打开当前选中的文件或目录。
 *
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int open_selected(struct browser_app *app)
{
    enum file_type type;

    if (app->files.count == 0 || app->selected >= app->files.count ||
        file_list_path(&app->files, app->selected,
                       app->current_path, sizeof(app->current_path)) < 0) {
        return -1;
    }
    type = app->files.entries[app->selected].type;
    if (type == FILE_TYPE_DIRECTORY) {
        if (file_list_scan(app->current_path, &app->files) < 0) {
            return -1;
        }
        app->selected = 0;
        return render_file_page(app);
    }
    if (is_image(type)) {
        app->page = BROWSER_PAGE_IMAGE;
        return render_image_page(app);
    }
    if (type == FILE_TYPE_TEXT) {
        if (text_reader_open(&app->text, app->current_path) < 0) {
            return -1;
        }
        app->page = BROWSER_PAGE_TEXT;
        return text_reader_render(&app->text, &app->display, &app->font);
    }
    if (type == FILE_TYPE_WAV) {
        if (audio_player_start(&app->audio, app->current_path,
                               app->alsa_device) < 0) {
            return -1;
        }
        app->page = BROWSER_PAGE_AUDIO;
        return render_audio_page(app);
    }
    errno = ENOTSUP;
    return -1;
}

/**
 * @brief 返回父目录，但不越过启动根目录。
 *
 * @param app 浏览器上下文。
 * @return 已进入父目录返回 1，已在根目录返回 0，失败返回 -1。
 */
static int enter_parent(struct browser_app *app)
{
    char parent[PATH_MAX];
    char *slash;

    if (strcmp(app->files.directory, app->root) == 0) {
        return 0;
    }
    snprintf(parent, sizeof(parent), "%s", app->files.directory);
    slash = strrchr(parent, '/');
    if (slash == NULL || slash == parent) {
        snprintf(parent, sizeof(parent), "/");
    } else {
        *slash = '\0';
    }
    if (file_list_scan(parent, &app->files) < 0) {
        return -1;
    }
    app->selected = 0;
    return 1;
}

/**
 * @brief 处理文件列表页面动作。
 *
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续运行返回 0，退出返回 1，错误返回 -1。
 */
static int handle_file_action(struct browser_app *app,
                              enum input_action action)
{
    if (action == INPUT_ACTION_UP && app->files.count > 0) {
        app->selected = (app->selected + app->files.count - 1) %
                        app->files.count;
    } else if (action == INPUT_ACTION_DOWN && app->files.count > 0) {
        app->selected = (app->selected + 1) % app->files.count;
    } else if (action == INPUT_ACTION_OPEN) {
        return open_selected(app);
    } else if (action == INPUT_ACTION_BACK) {
        int result = enter_parent(app);

        if (result <= 0) {
            return result == 0 ? 1 : -1;
        }
    } else if (action == INPUT_ACTION_EXIT) {
        return 1;
    } else {
        return 0;
    }
    return render_file_page(app);
}

/**
 * @brief 关闭媒体页面并返回文件列表。
 *
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
static int close_media_page(struct browser_app *app)
{
    if (app->page == BROWSER_PAGE_TEXT) {
        text_reader_close(&app->text);
    } else if (app->page == BROWSER_PAGE_AUDIO) {
        audio_player_stop(&app->audio);
    }
    app->page = BROWSER_PAGE_FILES;
    return render_file_page(app);
}

/**
 * @brief 处理媒体页面动作。
 *
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续运行返回 0，退出返回 1，错误返回 -1。
 */
static int handle_media_action(struct browser_app *app,
                               enum input_action action)
{
    if (action == INPUT_ACTION_EXIT) {
        return 1;
    }
    if (action == INPUT_ACTION_BACK) {
        return close_media_page(app);
    }
    if (app->page == BROWSER_PAGE_IMAGE &&
        (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT)) {
        select_adjacent_image(app, action == INPUT_ACTION_NEXT ? 1 : -1);
        return render_image_page(app);
    }
    if (app->page == BROWSER_PAGE_TEXT &&
        (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT)) {
        if (action == INPUT_ACTION_NEXT) {
            text_reader_next(&app->text);
        } else {
            text_reader_previous(&app->text);
        }
        return text_reader_render(&app->text, &app->display, &app->font);
    }
    if (app->page == BROWSER_PAGE_AUDIO && action == INPUT_ACTION_TOGGLE) {
        audio_player_toggle_pause(&app->audio);
        return render_audio_page(app);
    }
    return 0;
}

/**
 * @brief 运行浏览器事件循环。
 *
 * @param app 浏览器上下文。
 * @return 成功退出返回 0，失败返回 -1。
 */
static int run_browser(struct browser_app *app)
{
    if (render_file_page(app) < 0) {
        return -1;
    }
    while (1) {
        enum input_action action;
        int result;

        if (input_keyboard_wait(&app->keyboard, &action) < 0) {
            return -1;
        }
        result = app->page == BROWSER_PAGE_FILES ?
                 handle_file_action(app, action) :
                 handle_media_action(app, action);
        if (result > 0) {
            return 0;
        }
        if (result < 0) {
            fprintf(stderr, "action %s failed: %s\n",
                    input_action_name(action), strerror(errno));
        }
    }
}

/**
 * @brief 多媒体文件浏览器入口。
 *
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    struct browser_app app;
    int result = -1;

    if (argc != 5 && argc != 6) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    memset(&app, 0, sizeof(app));
    app.display.fd = -1;
    app.keyboard.fd = -1;
    app.alsa_device = argc == 6 ? argv[5] : "default";
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
    if (input_keyboard_open(&app.keyboard, argv[2]) < 0) {
        goto cleanup_font;
    }
    if (audio_player_init(&app.audio) < 0) {
        goto cleanup_input;
    }
    result = run_browser(&app);
    text_reader_close(&app.text);
    audio_player_destroy(&app.audio);
cleanup_input:
    input_keyboard_close(&app.keyboard);
cleanup_font:
    font_renderer_close(&app.font);
cleanup_display:
    bmp_display_close(&app.display);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
