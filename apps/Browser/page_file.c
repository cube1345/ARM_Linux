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

#define FILE_PAGE_LIST_TOP (UI_HEADER_HEIGHT + 12)
#define FILE_PAGE_TAG_WIDTH 64

/**
 * @brief 获取文件类型标签颜色。
 * @param type 文件类型。
 * @return RGB888 颜色。
 */
static uint32_t file_type_color(enum file_type type)
{
    if (type == FILE_TYPE_DIRECTORY) {
        return UI_ACCENT;
    }
    if (type == FILE_TYPE_TEXT) {
        return UI_WARNING;
    }
    if (browser_file_type_is_audio(type)) {
        return UI_ACCENT_2;
    }
    if (browser_file_type_is_image(type)) {
        return UI_SELECTED_BORDER;
    }
    return UI_MUTED;
}

/**
 * @brief 绘制文件列表页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_file_page(struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;
    int row_height = (int)app->font.pixel_size + 18;
    int card_x = UI_MARGIN;
    int card_width = width - UI_MARGIN * 2;
    char subtitle[FILE_LIST_NAME_SIZE + 32];
    size_t visible = browser_ui_visible_rows(&app->display, &app->font);
    size_t first = app->selected / visible * visible;
    size_t index;

    snprintf(subtitle, sizeof(subtitle), "%zu items  ·  %.180s",
             app->files.count, app->files.directory);
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_header(&app->display, &app->font,
                           "Media Browser", subtitle);
    for (index = first; index < app->files.count && index < first + visible;
         index++) {
        int y = FILE_PAGE_LIST_TOP + (int)(index - first) * row_height;
        int card_height = row_height - 6;
        uint32_t background = index == app->selected ? UI_SELECTED :
                              (index % 2U == 0U ? UI_SURFACE :
                               UI_SURFACE_ALT);
        uint32_t border = index == app->selected ? UI_SELECTED_BORDER :
                          UI_BORDER;
        uint32_t tag = file_type_color(app->files.entries[index].type);

        browser_ui_draw_panel(&app->display, card_x, y, card_width,
                              card_height, background, border);
        ui_draw_rect(&app->display, card_x + 10, y + 9,
                     FILE_PAGE_TAG_WIDTH, card_height - 18, tag);
        ui_draw_text(&app->display, &app->font,
                     file_type_name(app->files.entries[index].type),
                     card_x + 20, y + (int)app->font.pixel_size + 6,
                     FILE_PAGE_TAG_WIDTH - 18, UI_BACKGROUND, tag);
        ui_draw_text(&app->display, &app->font,
                     app->files.entries[index].name,
                     card_x + FILE_PAGE_TAG_WIDTH + 28,
                     y + (int)app->font.pixel_size + 7,
                     card_width - FILE_PAGE_TAG_WIDTH - 44,
                     index == app->selected ? UI_TEXT : UI_MUTED,
                     background);
    }
    if (app->files.count == 0) {
        int y = FILE_PAGE_LIST_TOP + 20;

        browser_ui_draw_panel(&app->display, UI_MARGIN, y, card_width,
                              row_height + 24, UI_SURFACE, UI_BORDER);
        ui_draw_text(&app->display, &app->font, "Empty directory",
                     UI_MARGIN + 18, y + (int)app->font.pixel_size + 18,
                     card_width - 36, UI_MUTED, UI_SURFACE);
    }
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "↑↓ select  Enter open  Esc back  Q quit");
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

    if (input->touch == TOUCH_ACTION_TAP &&
        input->y >= FILE_PAGE_LIST_TOP &&
        input->y < (int)app->display.variable_info.yres - UI_FOOTER_HEIGHT) {
        int row_height = (int)app->font.pixel_size + 18;
        size_t first = app->selected / visible * visible;
        size_t row = (size_t)(input->y - FILE_PAGE_LIST_TOP) /
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
