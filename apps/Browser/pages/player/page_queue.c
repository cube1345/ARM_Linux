#include "page_queue.h"

#include "browser_ui.h"
#include "file_list.h"
#include "ui_draw.h"

#include <stdio.h>

#define QUEUE_HEADER_HEIGHT 34
#define QUEUE_ROW_PADDING 10
#define QUEUE_MIN_HEIGHT 82
#define QUEUE_TYPE_WIDTH 42

/** @brief 计算队列中匹配条目的数量和当前项序号。 */
static size_t queue_count(const struct browser_app *app,
                          page_queue_match_fn match, size_t *current)
{
    size_t count = 0;
    size_t index;

    if (current != NULL) *current = 0;
    if (app == NULL || match == NULL) return 0;
    for (index = 0; index < app->files.count; index++) {
        if (!match(app->files.entries[index].type)) continue;
        count++;
        if (index == app->selected && current != NULL) *current = count;
    }
    return count;
}

/** @brief 计算队列绘制时第一行对应的匹配序号。 */
static size_t queue_first_visible(size_t current, size_t total,
                                  size_t visible_rows)
{
    size_t preferred;

    if (current == 0 || visible_rows == 0 || total <= visible_rows) return 1;
    preferred = visible_rows / 2U;
    if (current > preferred + 1U) {
        size_t first = current - preferred;

        if (first + visible_rows - 1U > total) {
            first = total - visible_rows + 1U;
        }
        return first;
    }
    return 1;
}

/** @brief 绘制当前目录中的播放队列摘要。 */
void page_queue_draw(struct browser_app *app, page_queue_match_fn match,
                     const char *title, int x, int y, int width,
                     int height)
{
    size_t current;
    size_t total;
    size_t visible_rows;
    size_t first;
    size_t ordinal = 0;
    size_t drawn = 0;
    size_t index;
    int row_height;
    char heading[80];

    if (app == NULL || match == NULL || title == NULL || width <= 0 ||
        height < QUEUE_MIN_HEIGHT) {
        return;
    }
    total = queue_count(app, match, &current);
    if (total == 0) return;
    row_height = (int)app->font.pixel_size + QUEUE_ROW_PADDING;
    visible_rows = (size_t)((height - QUEUE_HEADER_HEIGHT - 10) /
                            row_height);
    if (visible_rows == 0) return;
    first = queue_first_visible(current, total, visible_rows);
    browser_ui_draw_panel(&app->display, x, y, width, height,
                          UI_SURFACE, UI_BORDER);
    snprintf(heading, sizeof(heading), "%s  %zu/%zu", title,
             current > 0 ? current : 1U, total);
    ui_draw_text(&app->display, &app->font, heading, x + 12,
                 y + (int)app->font.pixel_size + 9, width - 24,
                 UI_ACCENT, UI_SURFACE);
    for (index = 0; index < app->files.count; index++) {
        const struct file_entry *entry = &app->files.entries[index];
        int row_y;
        uint32_t background;
        char label[FILE_LIST_NAME_SIZE + 8];

        if (!match(entry->type)) continue;
        ordinal++;
        if (ordinal < first) continue;
        if (drawn >= visible_rows) break;
        row_y = y + QUEUE_HEADER_HEIGHT + (int)drawn * row_height;
        background = index == app->selected ? UI_SELECTED : UI_SURFACE;
        ui_draw_rect(&app->display, x + 8, row_y, width - 16,
                     row_height - 2, background);
        ui_draw_text(&app->display, &app->font, file_type_name(entry->type),
                     x + 16, row_y + (int)app->font.pixel_size + 5,
                     QUEUE_TYPE_WIDTH, UI_MUTED, background);
        snprintf(label, sizeof(label), "%s%s",
                 index == app->selected ? "> " : "  ", entry->name);
        ui_draw_text(&app->display, &app->font, label,
                     x + 16 + QUEUE_TYPE_WIDTH,
                     row_y + (int)app->font.pixel_size + 5,
                     width - QUEUE_TYPE_WIDTH - 32,
                     index == app->selected ? UI_TEXT : UI_MUTED,
                     background);
        drawn++;
    }
}
