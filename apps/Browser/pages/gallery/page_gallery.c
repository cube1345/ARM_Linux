#include "page_gallery.h"

#include "animation_decoder.h"
#include "browser_ui.h"
#include "file_list.h"
#include "gif_animation.h"
#include "image_decoder.h"
#include "image_render.h"
#include "page_file.h"
#include "ui_draw.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define GALLERY_GAP 14
#define GALLERY_TOP (UI_HEADER_HEIGHT + 14)
#define GALLERY_UP_WIDTH 72
#define GALLERY_HOME_WIDTH 104

/** @brief 根据屏幕宽度返回 Gallery 列数。 */
static size_t gallery_columns(const struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;

    if (width >= 900) return 3;
    return width >= 560 ? 2 : 1;
}

/** @brief 根据屏幕高度返回 Gallery 行数。 */
static size_t gallery_rows(const struct browser_app *app)
{
    int available = (int)app->display.variable_info.yres - GALLERY_TOP -
                    UI_FOOTER_HEIGHT - UI_MARGIN;

    return available >= 420 ? 2 : 1;
}

/** @brief 释放 Gallery 全部缩略图缓存。 */
void gallery_cache_clear(struct browser_app *app)
{
    size_t index;

    if (app == NULL) return;
    for (index = 0; index < BROWSER_GALLERY_CACHE_COUNT; index++) {
        image_data_destroy(&app->gallery_cache[index].image);
        app->gallery_cache[index].path[0] = '\0';
        app->gallery_cache[index].last_used = 0;
    }
    app->gallery_cache_serial = 0;
}

/** @brief 查找空闲或最久未使用的缓存槽。 */
static size_t gallery_cache_slot(struct browser_app *app, const char *path)
{
    size_t oldest = 0;
    size_t index;

    for (index = 0; index < BROWSER_GALLERY_CACHE_COUNT; index++) {
        if (strcmp(app->gallery_cache[index].path, path) == 0) return index;
        if (app->gallery_cache[index].path[0] == '\0') return index;
        if (app->gallery_cache[index].last_used <
            app->gallery_cache[oldest].last_used) {
            oldest = index;
        }
    }
    return oldest;
}

/** @brief 解码并缓存指定文件的缩略图。 */
static const struct image_data *gallery_thumbnail(
    struct browser_app *app, size_t file_index,
    uint32_t maximum_width, uint32_t maximum_height)
{
    struct image_data source = {0};
    struct gif_animation animation = {0};
    const struct image_data *frame = &source;
    enum file_type type = app->files.entries[file_index].type;
    char path[PATH_MAX];
    size_t slot;
    int decoded = 0;

    if (file_list_path(&app->files, file_index, path, sizeof(path)) < 0) {
        return NULL;
    }
    slot = gallery_cache_slot(app, path);
    if (strcmp(app->gallery_cache[slot].path, path) == 0 &&
        app->gallery_cache[slot].image.pixels != NULL) {
        app->gallery_cache[slot].last_used = ++app->gallery_cache_serial;
        return &app->gallery_cache[slot].image;
    }
    image_data_destroy(&app->gallery_cache[slot].image);
    app->gallery_cache[slot].path[0] = '\0';
    if (type == FILE_TYPE_GIF) {
        if (animation_decoder_manager_open(&app->animations, path, type,
                                           &animation) == 0) {
            frame = gif_animation_current(&animation);
            decoded = frame != NULL;
        }
    } else {
        decoded = image_decode(path, type, &source) == 0;
    }
    if (decoded && image_render_scale_fit(frame, maximum_width,
                                          maximum_height,
                                          &app->gallery_cache[slot].image) ==
                   0) {
        snprintf(app->gallery_cache[slot].path,
                 sizeof(app->gallery_cache[slot].path), "%s", path);
        app->gallery_cache[slot].last_used = ++app->gallery_cache_serial;
    }
    gif_animation_close(&animation);
    image_data_destroy(&source);
    return app->gallery_cache[slot].image.pixels == NULL ? NULL :
           &app->gallery_cache[slot].image;
}

/** @brief 绘制 Gallery 缩略图网格。 */
int render_gallery_page(struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    size_t columns = gallery_columns(app);
    size_t rows = gallery_rows(app);
    size_t capacity = columns * rows;
    size_t first = app->selected / capacity * capacity;
    int available_height = height - GALLERY_TOP - UI_FOOTER_HEIGHT - UI_MARGIN;
    int card_width = (width - UI_MARGIN * 2 -
                      (int)(columns - 1U) * GALLERY_GAP) / (int)columns;
    int card_height = (available_height -
                       (int)(rows - 1U) * GALLERY_GAP) / (int)rows;
    int image_height = card_height - (int)app->font.pixel_size - 40;
    char subtitle[PATH_MAX + 64];
    size_t index;

    snprintf(subtitle, sizeof(subtitle), "%zu items  page %zu/%zu  %.180s",
             app->files.count, app->files.count == 0 ? 0 : first / capacity + 1,
             app->files.count == 0 ? 0 :
             (app->files.count + capacity - 1U) / capacity,
             app->files.directory);
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_header(&app->display, &app->font, "Gallery", subtitle);
    browser_ui_draw_button(&app->display, &app->font,
                           width - UI_MARGIN - GALLERY_HOME_WIDTH -
                           GALLERY_UP_WIDTH - 12, 10,
                           GALLERY_UP_WIDTH, 42, "UP", UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font,
                           width - UI_MARGIN - GALLERY_HOME_WIDTH, 10,
                           GALLERY_HOME_WIDTH, 42, "HOME", UI_HEADER);
    for (index = first; index < app->files.count && index < first + capacity;
         index++) {
        size_t local = index - first;
        int column = (int)(local % columns);
        int row = (int)(local / columns);
        int x = UI_MARGIN + column * (card_width + GALLERY_GAP);
        int y = GALLERY_TOP + row * (card_height + GALLERY_GAP);
        uint32_t background = index == app->selected ? UI_SELECTED : UI_SURFACE;
        uint32_t border = index == app->selected ? UI_SELECTED_BORDER : UI_BORDER;

        browser_ui_draw_panel(&app->display, x, y, card_width, card_height,
                              background, border);
        if (app->files.entries[index].type == FILE_TYPE_DIRECTORY) {
            ui_draw_rect(&app->display, x + 12, y + 12, card_width - 24,
                         image_height, UI_WARNING);
            ui_draw_text(&app->display, &app->font, "FOLDER", x + 28,
                         y + image_height / 2, card_width - 56,
                         UI_BACKGROUND, UI_WARNING);
        } else {
            const struct image_data *thumbnail = gallery_thumbnail(
                app, index, (uint32_t)(card_width - 24),
                (uint32_t)image_height);

            if (thumbnail != NULL) {
                (void)image_render_draw_region(&app->display, thumbnail,
                                               x + 12, y + 12,
                                               card_width - 24, image_height,
                                               0x080d12U);
            }
        }
        ui_draw_text(&app->display, &app->font,
                     app->files.entries[index].name,
                     x + 12, y + card_height - 12,
                     card_width - 24,
                     index == app->selected ? UI_TEXT : UI_MUTED,
                     background);
    }
    if (app->files.count == 0) {
        ui_draw_text(&app->display, &app->font, "No photos in this folder",
                     UI_MARGIN + 16, GALLERY_TOP + 54, width - UI_MARGIN * 2,
                     UI_MUTED, UI_BACKGROUND);
    }
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Arrows browse  Enter open  Swipe page");
    return bmp_display_flush(&app->display);
}

/** @brief 处理 Gallery 键盘动作。 */
int handle_gallery_key(struct browser_app *app, enum input_action action)
{
    size_t columns = gallery_columns(app);
    size_t count = app->files.count;

    if (action == INPUT_ACTION_BACK) {
        int result = enter_parent(app);

        if (result < 0) return -1;
        return result == 0 ? browser_app_return_to_desktop(app) :
               render_gallery_page(app);
    }
    if (count == 0) return 0;
    if (action == INPUT_ACTION_PREVIOUS) {
        app->selected = (app->selected + count - 1U) % count;
    } else if (action == INPUT_ACTION_NEXT) {
        app->selected = (app->selected + 1U) % count;
    } else if (action == INPUT_ACTION_UP) {
        app->selected = app->selected >= columns ?
                        app->selected - columns : 0;
    } else if (action == INPUT_ACTION_DOWN) {
        app->selected += columns;
        if (app->selected >= count) app->selected = count - 1U;
    } else if (action == INPUT_ACTION_OPEN) {
        return open_selected(app);
    } else {
        return 0;
    }
    return render_gallery_page(app);
}

/** @brief 处理 Gallery 触摸动作。 */
int handle_gallery_touch(struct browser_app *app,
                         const struct browser_input *input)
{
    int width = (int)app->display.variable_info.xres;
    int height = (int)app->display.variable_info.yres;
    size_t columns = gallery_columns(app);
    size_t rows = gallery_rows(app);
    size_t capacity = columns * rows;
    size_t first = app->selected / capacity * capacity;
    int available_height = height - GALLERY_TOP - UI_FOOTER_HEIGHT - UI_MARGIN;
    int card_width = (width - UI_MARGIN * 2 -
                      (int)(columns - 1U) * GALLERY_GAP) / (int)columns;
    int card_height = (available_height -
                       (int)(rows - 1U) * GALLERY_GAP) / (int)rows;

    if (input->touch == TOUCH_ACTION_TAP && input->y < UI_HEADER_HEIGHT &&
        input->x >= width - UI_MARGIN - GALLERY_HOME_WIDTH) {
        return browser_app_return_to_desktop(app);
    }
    if (input->touch == TOUCH_ACTION_TAP && input->y < UI_HEADER_HEIGHT &&
        input->x >= width - UI_MARGIN - GALLERY_HOME_WIDTH -
                    GALLERY_UP_WIDTH - 12) {
        int result = enter_parent(app);

        if (result < 0) return -1;
        return result == 0 ? browser_app_return_to_desktop(app) :
               render_gallery_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP && input->y >= GALLERY_TOP &&
        input->y < height - UI_FOOTER_HEIGHT) {
        int column = (input->x - UI_MARGIN) / (card_width + GALLERY_GAP);
        int row = (input->y - GALLERY_TOP) / (card_height + GALLERY_GAP);
        size_t index;

        if (column < 0 || column >= (int)columns || row < 0 ||
            row >= (int)rows) return 0;
        index = first + (size_t)row * columns + (size_t)column;
        if (index < app->files.count) {
            app->selected = index;
            return open_selected(app);
        }
    }
    if (input->touch == TOUCH_ACTION_SWIPE &&
        input->dx != 0 && app->files.count > 0) {
        if (input->dx < 0) {
            app->selected += capacity;
            if (app->selected >= app->files.count) {
                app->selected = app->files.count - 1U;
            }
        } else if (app->selected >= capacity) {
            app->selected -= capacity;
        } else {
            app->selected = 0;
        }
        return render_gallery_page(app);
    }
    return 0;
}
