#include "page_file.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "file_list.h"
#include "page_audio.h"
#include "page_image.h"
#include "page_text.h"
#include "ui_draw.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 绘制文件列表页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_file_page(struct browser_app *app)
{
    int row_height = (int)app->font.pixel_size + 14;
    size_t visible = browser_ui_visible_rows(&app->display, &app->font);
    size_t first = app->selected / visible * visible;
    size_t index;

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
 * @brief 打开当前选择的目录或媒体。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int open_selected(struct browser_app *app)
{
    enum file_type type;

    if (app->files.count == 0 || app->selected >= app->files.count ||
        file_list_path(&app->files, app->selected,
                       app->current_path, sizeof(app->current_path)) < 0) {
        errno = EINVAL;
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
    if (browser_file_type_is_image(type)) {
        if (load_selected_image(app) < 0) {
            return -1;
        }
        app->page = BROWSER_PAGE_IMAGE;
        return render_image_page(app);
    }
    if (type == FILE_TYPE_TEXT) {
        if (text_reader_open(&app->text, app->current_path) < 0) {
            return -1;
        }
        app->page = BROWSER_PAGE_TEXT;
        return render_text_page(app);
    }
    if (browser_file_type_is_audio(type)) {
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
 * @brief 返回父目录但不越过启动根目录。
 * @param app 浏览器上下文。
 * @return 已进入父目录返回 1，已在根目录返回 0，失败返回 -1。
 */
int enter_parent(struct browser_app *app)
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
 * @brief 处理文件列表键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int handle_file_key(struct browser_app *app, enum input_action action)
{
    if (action == INPUT_ACTION_UP && app->files.count > 0) {
        app->selected = (app->selected + app->files.count - 1U) %
                        app->files.count;
    } else if (action == INPUT_ACTION_DOWN && app->files.count > 0) {
        app->selected = (app->selected + 1U) % app->files.count;
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
 * @brief 处理文件列表触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int handle_file_touch(struct browser_app *app,
                      const struct browser_input *input)
{
    size_t visible = browser_ui_visible_rows(&app->display, &app->font);

    if (input->touch == TOUCH_ACTION_TAP && input->y >= UI_HEADER_HEIGHT) {
        int row_height = (int)app->font.pixel_size + 14;
        size_t first = app->selected / visible * visible;
        size_t row = (size_t)(input->y - UI_HEADER_HEIGHT) /
                     (size_t)row_height;

        if (first + row < app->files.count && row < visible) {
            app->selected = first + row;
            return open_selected(app);
        }
    } else if (input->touch == TOUCH_ACTION_SWIPE &&
               abs(input->dy) > abs(input->dx) && app->files.count > 0) {
        if (input->dy < 0) {
            app->selected += visible;
            if (app->selected >= app->files.count) {
                app->selected = app->files.count - 1U;
            }
        } else if (app->selected > visible) {
            app->selected -= visible;
        } else {
            app->selected = 0;
        }
        return render_file_page(app);
    }
    return 0;
}
