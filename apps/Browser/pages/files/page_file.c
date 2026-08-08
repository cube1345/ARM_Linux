#include "page_file.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "desktop_app.h"
#include "file_list.h"
#include "page_audio.h"
#include "page_gallery.h"
#include "page_video.h"
#include "page_image.h"
#include "page_text.h"
#include "ui_draw.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#define FILE_PAGE_LIST_TOP (UI_HEADER_HEIGHT + 12)
#define FILE_PAGE_TAG_WIDTH 64
#define FILE_PAGE_UP_WIDTH 72
#define FILE_PAGE_HOME_WIDTH 104
#define FILE_PAGE_SORT_WIDTH 104
#define FILE_PAGE_SEARCH_WIDTH 104
#define FILE_PAGE_RECENT_WIDTH 104
#define FILE_PAGE_FAVORITE_WIDTH 92
#define FILE_PAGE_BUTTON_GAP 12
#define FILE_PAGE_BUTTON_Y 10
#define FILE_PAGE_BUTTON_HEIGHT 42

/** @brief 文件页顶部按钮布局。 */
struct file_page_button_layout {
    int search_x;
    int sort_x;
    int recent_x;
    int favorite_x;
    int up_x;
    int home_x;
    int show_search_sort;
    int show_recent_favorite;
};

/** @brief 根据屏幕宽度计算文件页顶部按钮位置。 */
static struct file_page_button_layout file_page_button_layout(int width)
{
    struct file_page_button_layout layout;

    layout.home_x = width - UI_MARGIN - FILE_PAGE_HOME_WIDTH;
    layout.up_x = layout.home_x - FILE_PAGE_UP_WIDTH - FILE_PAGE_BUTTON_GAP;
    layout.favorite_x = -1;
    layout.recent_x = -1;
    layout.sort_x = -1;
    layout.search_x = -1;
    layout.show_search_sort = width >= 640;
    layout.show_recent_favorite = width >= 920;
    if (layout.show_recent_favorite) {
        layout.favorite_x = layout.up_x - FILE_PAGE_FAVORITE_WIDTH -
                            FILE_PAGE_BUTTON_GAP;
        layout.recent_x = layout.favorite_x - FILE_PAGE_RECENT_WIDTH -
                          FILE_PAGE_BUTTON_GAP;
        layout.sort_x = layout.recent_x - FILE_PAGE_SORT_WIDTH -
                        FILE_PAGE_BUTTON_GAP;
    } else {
        layout.sort_x = layout.up_x - FILE_PAGE_SORT_WIDTH -
                        FILE_PAGE_BUTTON_GAP;
    }
    layout.search_x = layout.sort_x - FILE_PAGE_SEARCH_WIDTH -
                      FILE_PAGE_BUTTON_GAP;
    return layout;
}

/** @brief 判断文件类型是否匹配当前应用过滤器。 */
static int file_type_allowed_by_app(const struct browser_app *app,
                                    enum file_type type)
{
    if (browser_file_type_is_image(type)) {
        return (app->file_filter & FILE_LIST_FILTER_IMAGES) != 0U;
    }
    if (browser_file_type_is_audio(type)) {
        return (app->file_filter & FILE_LIST_FILTER_AUDIO) != 0U;
    }
    if (browser_file_type_is_video(type)) {
        return (app->file_filter & FILE_LIST_FILTER_VIDEO) != 0U;
    }
    if (type == FILE_TYPE_TEXT) {
        return (app->file_filter & FILE_LIST_FILTER_TEXT) != 0U;
    }
    return 0;
}

/** @brief 获取路径 basename。 */
static const char *file_path_basename(const char *path)
{
    const char *slash = strrchr(path, '/');

    return slash == NULL ? path : slash + 1;
}

/** @brief 判断当前文件页是否为虚拟路径列表。 */
static int file_view_is_virtual(const struct browser_app *app)
{
    return app->file_view == BROWSER_FILE_VIEW_RECENT ||
           app->file_view == BROWSER_FILE_VIEW_FAVORITES;
}

/** @brief 获取文件页视图名称。 */
static const char *file_view_name(enum browser_file_view view)
{
    if (view == BROWSER_FILE_VIEW_RECENT) return "Recent";
    if (view == BROWSER_FILE_VIEW_FAVORITES) return "Favorites";
    return "Files";
}

/** @brief 判断两个路径列表是否一致。 */
static int path_lists_equal(const struct browser_path_list *left,
                            const struct browser_path_list *right)
{
    size_t index;

    if (left->count != right->count) return 0;
    for (index = 0; index < left->count; index++) {
        if (strcmp(left->paths[index], right->paths[index]) != 0) return 0;
    }
    return 1;
}

/** @brief 从配置路径列表构建可打开的虚拟文件列表。 */
static int build_path_list_view(struct browser_app *app,
                                enum browser_file_view view)
{
    struct browser_path_list *source =
        view == BROWSER_FILE_VIEW_RECENT ? &app->config.recent_files :
        &app->config.favorite_files;
    struct browser_path_list valid = {0};
    struct file_entry entries[FILE_LIST_MAX_COUNT];
    size_t count = 0;
    size_t index;

    for (index = 0; index < source->count && count < FILE_LIST_MAX_COUNT;
         index++) {
        const char *path = source->paths[index];
        struct stat status;
        enum file_type type;
        int written;

        if (stat(path, &status) < 0 || !S_ISREG(status.st_mode)) continue;
        type = file_list_detect_type(path);
        if (type == FILE_TYPE_UNKNOWN || !file_type_allowed_by_app(app, type)) {
            continue;
        }
        written = snprintf(entries[count].name, sizeof(entries[count].name),
                           "%s", path);
        if (written < 0 || (size_t)written >= sizeof(entries[count].name)) {
            continue;
        }
        entries[count].type = type;
        entries[count].size_bytes = (uint64_t)status.st_size;
        entries[count].modified_time = status.st_mtime;
        browser_path_list_append(&valid, path);
        count++;
    }
    memcpy(app->files.entries, entries, sizeof(entries[0]) * count);
    app->files.count = count;
    if (!path_lists_equal(source, &valid)) {
        *source = valid;
        (void)browser_app_save_config(app);
    }
    if (app->selected >= app->files.count) {
        app->selected = app->files.count == 0 ? 0 : app->files.count - 1U;
    }
    return 0;
}

/** @brief 获取文件列表行高。 */
static int file_page_row_height(const struct browser_app *app)
{
    return (int)app->font.pixel_size * 2 + 28;
}

/** @brief 获取文件列表页面可显示的行数。 */
static size_t file_page_visible_rows(const struct browser_app *app)
{
    int available = (int)app->display.variable_info.yres -
                    FILE_PAGE_LIST_TOP - UI_FOOTER_HEIGHT - UI_MARGIN;
    int row_height = file_page_row_height(app);

    if (available <= 0 || row_height <= 0) return 1;
    return (size_t)(available / row_height) > 0 ?
           (size_t)(available / row_height) : 1;
}

/** @brief 返回排序方式的显示名称。 */
static const char *file_sort_name(enum file_list_sort sort)
{
    switch (sort) {
    case FILE_LIST_SORT_TYPE: return "Type";
    case FILE_LIST_SORT_TIME: return "Time";
    case FILE_LIST_SORT_SIZE: return "Size";
    case FILE_LIST_SORT_NAME:
    default: return "Name";
    }
}

/** @brief 获取下一个文件列表排序方式。 */
static enum file_list_sort file_sort_next(enum file_list_sort sort)
{
    return sort >= FILE_LIST_SORT_SIZE ? FILE_LIST_SORT_NAME :
           (enum file_list_sort)((int)sort + 1);
}

/** @brief 将文件大小格式化为紧凑可读文本。 */
static void format_file_size(uint64_t bytes, char *output, size_t output_size)
{
    if (bytes >= 1024U * 1024U * 1024U) {
        snprintf(output, output_size, "%.1f GB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024U * 1024U) {
        snprintf(output, output_size, "%.1f MB",
                 (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024U) {
        snprintf(output, output_size, "%.1f KB",
                 (double)bytes / 1024.0);
    } else {
        snprintf(output, output_size, "%" PRIu64 " B", bytes);
    }
}

/** @brief 将修改时间格式化为月日和时分。 */
static void format_file_time(time_t modified_time, char *output,
                             size_t output_size)
{
    struct tm local_time;

    if (localtime_r(&modified_time, &local_time) == NULL ||
        strftime(output, output_size, "%m/%d %H:%M", &local_time) == 0) {
        snprintf(output, output_size, "--/-- --:--");
    }
}

/** @brief 按当前排序方式更新列表。 */
static void apply_file_sort(struct browser_app *app)
{
    file_list_sort(&app->files, app->file_sort);
    if (app->files.count == 0) {
        app->selected = 0;
    } else if (app->selected >= app->files.count) {
        app->selected = app->files.count - 1U;
    }
}

/** @brief 判断文件名是否包含忽略大小写的搜索词。 */
static int file_name_matches(const char *name, const char *query)
{
    size_t name_length = strlen(name);
    size_t query_length = strlen(query);
    size_t offset;

    if (query_length == 0) return 1;
    if (query_length > name_length) return 0;
    for (offset = 0; offset + query_length <= name_length; offset++) {
        if (strncasecmp(name + offset, query, query_length) == 0) return 1;
    }
    return 0;
}

/** @brief 重新递归扫描并应用当前搜索词。 */
static int refresh_file_search(struct browser_app *app)
{
    char directory[PATH_MAX];
    size_t read_index;
    size_t write_index = 0;

    snprintf(directory, sizeof(directory), "%s", app->files.directory);
    if (file_list_scan_recursive_filtered(directory, &app->files,
                                          app->file_filter) < 0) {
        return -1;
    }
    for (read_index = 0; read_index < app->files.count; read_index++) {
        if (!file_name_matches(app->files.entries[read_index].name,
                               app->search_query)) {
            continue;
        }
        if (write_index != read_index) {
            app->files.entries[write_index] = app->files.entries[read_index];
        }
        write_index++;
    }
    app->files.count = write_index;
    apply_file_sort(app);
    app->selected = 0;
    return 0;
}

/** @brief 开启当前目录的递归文件名搜索。 */
static int begin_file_search(struct browser_app *app)
{
    app->search_active = 1;
    app->search_query[0] = '\0';
    if (refresh_file_search(app) < 0) {
        app->search_active = 0;
        return -1;
    }
    return 0;
}

/** @brief 退出搜索并恢复当前目录的普通列表。 */
static int end_file_search(struct browser_app *app)
{
    char directory[PATH_MAX];

    snprintf(directory, sizeof(directory), "%s", app->files.directory);
    app->search_active = 0;
    app->search_query[0] = '\0';
    if (file_list_scan_filtered(directory, &app->files,
                                app->file_filter) < 0) {
        return -1;
    }
    apply_file_sort(app);
    app->selected = 0;
    return 0;
}

/** @brief 刷新当前文件页视图。 */
static int refresh_current_file_view(struct browser_app *app)
{
    if (app->file_view == BROWSER_FILE_VIEW_DIRECTORY) {
        if (file_list_scan_filtered(app->files.directory, &app->files,
                                    app->file_filter) < 0) {
            return -1;
        }
        apply_file_sort(app);
        return 0;
    }
    return build_path_list_view(app, app->file_view);
}

/** @brief 切换文件页视图并刷新列表。 */
static int switch_file_view(struct browser_app *app,
                            enum browser_file_view view)
{
    app->search_active = 0;
    app->search_query[0] = '\0';
    app->file_view = view;
    app->selected = 0;
    return refresh_current_file_view(app);
}

/** @brief 循环切换普通目录、最近和收藏视图。 */
static int cycle_file_view(struct browser_app *app, int direction)
{
    int next = (int)app->file_view + direction;

    if (next < (int)BROWSER_FILE_VIEW_DIRECTORY) {
        next = (int)BROWSER_FILE_VIEW_FAVORITES;
    } else if (next > (int)BROWSER_FILE_VIEW_FAVORITES) {
        next = (int)BROWSER_FILE_VIEW_DIRECTORY;
    }
    return switch_file_view(app, (enum browser_file_view)next);
}

/** @brief 获取当前选中条目的完整路径。 */
static int selected_file_path(const struct browser_app *app, char *output,
                              size_t output_size)
{
    if (app->files.count == 0 || app->selected >= app->files.count) {
        errno = EINVAL;
        return -1;
    }
    return file_list_path(&app->files, app->selected, output, output_size);
}

/** @brief 将成功打开的非目录文件记录到最近列表。 */
static void remember_opened_file(struct browser_app *app)
{
    if (app->current_path[0] == '\0') return;
    browser_path_list_add_front(&app->config.recent_files,
                                app->current_path);
    (void)browser_app_save_config(app);
}

/** @brief 收藏或取消收藏当前选中的普通文件。 */
static int toggle_selected_favorite(struct browser_app *app)
{
    char path[PATH_MAX];

    if (app->files.count == 0 || app->selected >= app->files.count ||
        app->files.entries[app->selected].type == FILE_TYPE_DIRECTORY ||
        selected_file_path(app, path, sizeof(path)) < 0) {
        return 0;
    }
    if (browser_path_list_remove(&app->config.favorite_files, path) == 0) {
        browser_path_list_add_front(&app->config.favorite_files, path);
    }
    (void)browser_app_save_config(app);
    if (app->file_view == BROWSER_FILE_VIEW_FAVORITES &&
        refresh_current_file_view(app) < 0) {
        return -1;
    }
    return render_file_page(app);
}

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
    if (browser_file_type_is_video(type)) {
        return UI_ACCENT_2;
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
    const struct desktop_app_operation *application =
        desktop_app_find(&app->desktop_apps, app->active_app);

    if (app->active_app == DESKTOP_APP_GALLERY) {
        return render_gallery_page(app);
    }
    int width = (int)app->display.variable_info.xres;
    int row_height = file_page_row_height(app);
    int card_x = UI_MARGIN;
    int card_width = width - UI_MARGIN * 2;
    struct file_page_button_layout buttons = file_page_button_layout(width);
    char subtitle[FILE_LIST_NAME_SIZE + 64];
    size_t visible = file_page_visible_rows(app);
    size_t first = app->selected / visible * visible;
    size_t index;

    if (app->search_active) {
        snprintf(subtitle, sizeof(subtitle), "Search: %s  %zu files  %.110s",
                 app->search_query[0] == '\0' ? "*" : app->search_query,
                 app->files.count, app->files.directory);
    } else if (file_view_is_virtual(app)) {
        snprintf(subtitle, sizeof(subtitle), "%s  %zu items",
                 file_view_name(app->file_view), app->files.count);
    } else {
        snprintf(subtitle, sizeof(subtitle), "%s  %zu items  sort:%s  %.140s",
                 file_view_name(app->file_view), app->files.count,
                 file_sort_name(app->file_sort), app->files.directory);
    }
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_header(&app->display, &app->font,
                           application == NULL ? "Files" : application->name,
                           subtitle);
    if (buttons.show_search_sort) {
        browser_ui_draw_button(&app->display, &app->font,
                               buttons.search_x, FILE_PAGE_BUTTON_Y,
                               FILE_PAGE_SEARCH_WIDTH,
                               FILE_PAGE_BUTTON_HEIGHT,
                               file_view_is_virtual(app) ? "FILES" :
                               app->search_active ? "CLOSE" : "SEARCH",
                               UI_HEADER);
        browser_ui_draw_button(&app->display, &app->font,
                               buttons.sort_x, FILE_PAGE_BUTTON_Y,
                               FILE_PAGE_SORT_WIDTH,
                               FILE_PAGE_BUTTON_HEIGHT,
                               file_view_is_virtual(app) ? "VIEW" :
                               file_sort_name(app->file_sort), UI_HEADER);
    }
    if (buttons.show_recent_favorite) {
        browser_ui_draw_button(&app->display, &app->font,
                               buttons.recent_x, FILE_PAGE_BUTTON_Y,
                               FILE_PAGE_RECENT_WIDTH,
                               FILE_PAGE_BUTTON_HEIGHT, "RECENT",
                               app->file_view == BROWSER_FILE_VIEW_RECENT ?
                               UI_SELECTED : UI_HEADER);
        browser_ui_draw_button(&app->display, &app->font,
                               buttons.favorite_x, FILE_PAGE_BUTTON_Y,
                               FILE_PAGE_FAVORITE_WIDTH,
                               FILE_PAGE_BUTTON_HEIGHT, "FAV",
                               app->file_view == BROWSER_FILE_VIEW_FAVORITES ?
                               UI_SELECTED : UI_HEADER);
    }
    browser_ui_draw_button(&app->display, &app->font,
                           buttons.up_x, FILE_PAGE_BUTTON_Y,
                           FILE_PAGE_UP_WIDTH, FILE_PAGE_BUTTON_HEIGHT,
                           file_view_is_virtual(app) ? "FILES" : "UP",
                           UI_HEADER);
    browser_ui_draw_button(&app->display, &app->font,
                           buttons.home_x, FILE_PAGE_BUTTON_Y,
                           FILE_PAGE_HOME_WIDTH, FILE_PAGE_BUTTON_HEIGHT,
                           "HOME", UI_HEADER);
    for (index = first; index < app->files.count && index < first + visible;
         index++) {
        int y = FILE_PAGE_LIST_TOP + (int)(index - first) * row_height;
        int card_height = row_height - 6;
        const char *display_name =
            file_view_is_virtual(app) ?
            file_path_basename(app->files.entries[index].name) :
            app->files.entries[index].name;
        char metadata[192];
        char size_text[24];
        char time_text[24];
        uint32_t background = index == app->selected ? UI_SELECTED :
                              (index % 2U == 0U ? UI_SURFACE :
                               UI_SURFACE_ALT);
        uint32_t border = index == app->selected ? UI_SELECTED_BORDER :
                          UI_BORDER;
        uint32_t tag = file_type_color(app->files.entries[index].type);

        if (app->files.entries[index].type == FILE_TYPE_DIRECTORY) {
            snprintf(size_text, sizeof(size_text), "Folder");
        } else {
            format_file_size(app->files.entries[index].size_bytes,
                             size_text, sizeof(size_text));
        }
        format_file_time(app->files.entries[index].modified_time,
                         time_text, sizeof(time_text));
        if (file_view_is_virtual(app)) {
            snprintf(metadata, sizeof(metadata), "%s  %s  %.120s",
                     size_text, time_text, app->files.entries[index].name);
        } else {
            snprintf(metadata, sizeof(metadata), "%s  %s", size_text,
                     time_text);
        }
        browser_ui_draw_panel(&app->display, card_x, y, card_width,
                              card_height, background, border);
        ui_draw_rect(&app->display, card_x + 10, y + 9,
                     FILE_PAGE_TAG_WIDTH, card_height - 18, tag);
        ui_draw_text(&app->display, &app->font,
                     file_type_name(app->files.entries[index].type),
                     card_x + 20, y + (int)app->font.pixel_size + 6,
                     FILE_PAGE_TAG_WIDTH - 18, UI_BACKGROUND, tag);
        ui_draw_text(&app->display, &app->font,
                     display_name,
                     card_x + FILE_PAGE_TAG_WIDTH + 28,
                     y + (int)app->font.pixel_size + 5,
                     card_width - FILE_PAGE_TAG_WIDTH - 44,
                     index == app->selected ? UI_TEXT : UI_MUTED,
                     background);
        ui_draw_text(&app->display, &app->font, metadata,
                     card_x + FILE_PAGE_TAG_WIDTH + 28,
                     y + (int)app->font.pixel_size * 2 + 7,
                     card_width - FILE_PAGE_TAG_WIDTH - 44,
                     UI_MUTED, background);
    }
    if (app->files.count == 0) {
        int y = FILE_PAGE_LIST_TOP + 20;
        const char *empty_text = app->search_active ? "No matching files" :
                                 app->file_view ==
                                 BROWSER_FILE_VIEW_RECENT ?
                                 "No recent files" :
                                 app->file_view ==
                                 BROWSER_FILE_VIEW_FAVORITES ?
                                 "No favorites yet" : "Empty directory";

        browser_ui_draw_panel(&app->display, UI_MARGIN, y, card_width,
                              row_height + 24, UI_SURFACE, UI_BORDER);
        ui_draw_text(&app->display, &app->font, empty_text,
                     UI_MARGIN + 18, y + (int)app->font.pixel_size + 18,
                     card_width - 36, UI_MUTED, UI_SURFACE);
    }
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                app->search_active ?
                                "Type to search  Backspace edit  Esc close" :
                                file_view_is_virtual(app) ?
                                "↑↓ select  Enter open  R favorite  Tab view  Back files" :
                                "↑↓ select  Enter open  O sort  R favorite  Tab view  / search");
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
        if (file_list_scan_filtered(app->current_path, &app->files,
                                    app->file_filter) < 0) {
            return -1;
        }
        app->file_view = BROWSER_FILE_VIEW_DIRECTORY;
        file_list_sort(&app->files, app->file_sort);
        app->selected = 0;
        return render_file_page(app);
    }
    if (browser_file_type_is_image(type)) {
        if (load_selected_image(app) < 0) {
            return -1;
        }
        remember_opened_file(app);
        app->page = BROWSER_PAGE_IMAGE;
        return render_image_page(app);
    }
    if (type == FILE_TYPE_TEXT) {
        if (text_reader_open(&app->text, app->current_path) < 0) {
            return -1;
        }
        remember_opened_file(app);
        app->page = BROWSER_PAGE_TEXT;
        return render_text_page(app);
    }
    if (browser_file_type_is_audio(type)) {
        if (type != FILE_TYPE_WAV && type != FILE_TYPE_MP3) {
            if (media_player_start(&app->media, app->current_path,
                                   app->alsa_device) < 0) {
                return -1;
            }
            browser_app_restore_playback(app, app->current_path,
                                         BROWSER_PAGE_VIDEO);
            (void)subtitle_track_load_for_media(&app->subtitles,
                                                app->current_path);
            remember_opened_file(app);
            app->page = BROWSER_PAGE_VIDEO;
            return render_video_page(app);
        }
        if (audio_player_start(&app->audio, app->current_path,
                               app->alsa_device) < 0) {
            return -1;
        }
        browser_app_restore_playback(app, app->current_path,
                                     BROWSER_PAGE_AUDIO);
        (void)audio_metadata_read(app->current_path, &app->audio_metadata);
        remember_opened_file(app);
        app->page = BROWSER_PAGE_AUDIO;
        return render_audio_page(app);
    }
    if (browser_file_type_is_video(type)) {
        if (media_player_start(&app->media, app->current_path,
                               app->alsa_device) < 0) {
            return -1;
        }
        browser_app_restore_playback(app, app->current_path,
                                     BROWSER_PAGE_VIDEO);
        (void)subtitle_track_load_for_media(&app->subtitles,
                                            app->current_path);
        remember_opened_file(app);
        app->page = BROWSER_PAGE_VIDEO;
        return render_video_page(app);
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
    if (file_list_scan_filtered(parent, &app->files,
                                app->file_filter) < 0) {
        return -1;
    }
    file_list_sort(&app->files, app->file_sort);
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
    if (app->active_app == DESKTOP_APP_GALLERY) {
        return handle_gallery_key(app, action);
    }
    if (action == INPUT_ACTION_SEARCH) {
        if (file_view_is_virtual(app) &&
            switch_file_view(app, BROWSER_FILE_VIEW_DIRECTORY) < 0) {
            return -1;
        }
        if (app->search_active) {
            if (end_file_search(app) < 0) return -1;
        } else if (begin_file_search(app) < 0) {
            return -1;
        }
        return render_file_page(app);
    }
    if (app->search_active && action == INPUT_ACTION_BACK) {
        size_t length = strlen(app->search_query);

        if (length > 0) {
            app->search_query[length - 1U] = '\0';
            if (refresh_file_search(app) < 0) return -1;
            return render_file_page(app);
        }
        if (end_file_search(app) < 0) return -1;
        return render_file_page(app);
    }
    if (action == INPUT_ACTION_UP && app->files.count > 0) {
        app->selected = (app->selected + app->files.count - 1U) %
                        app->files.count;
    } else if (action == INPUT_ACTION_DOWN && app->files.count > 0) {
        app->selected = (app->selected + 1U) % app->files.count;
    } else if (action == INPUT_ACTION_OPEN) {
        return open_selected(app);
    } else if (action == INPUT_ACTION_ROTATE) {
        return toggle_selected_favorite(app);
    } else if (action == INPUT_ACTION_VIEW) {
        if (cycle_file_view(app, 1) < 0) return -1;
    } else if (action == INPUT_ACTION_SORT) {
        if (file_view_is_virtual(app)) {
            if (cycle_file_view(app, 1) < 0) return -1;
        } else {
            app->file_sort = file_sort_next(app->file_sort);
            apply_file_sort(app);
            app->selected = 0;
            (void)browser_app_save_config(app);
        }
    } else if (action == INPUT_ACTION_BACK) {
        if (file_view_is_virtual(app)) {
            if (switch_file_view(app, BROWSER_FILE_VIEW_DIRECTORY) < 0) {
                return -1;
            }
            return render_file_page(app);
        }
        int result = enter_parent(app);

        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            return browser_app_return_to_desktop(app);
        }
    } else if (action == INPUT_ACTION_EXIT) {
        return 1;
    } else {
        return 0;
    }
    return render_file_page(app);
}

/**
 * @brief 处理文件搜索模式下输入的字符。
 * @param app 浏览器上下文。
 * @param text 输入字符。
 * @param text_length 字符数量。
 * @return 成功返回 0，失败返回 -1。
 */
int handle_file_text(struct browser_app *app, const char *text,
                     size_t text_length)
{
    size_t current_length;
    size_t copy_length;

    if (app == NULL || text == NULL || text_length == 0 ||
        !app->search_active) {
        return 0;
    }
    current_length = strlen(app->search_query);
    if (current_length >= BROWSER_SEARCH_QUERY_SIZE - 1U) return 0;
    copy_length = text_length;
    if (copy_length > BROWSER_SEARCH_QUERY_SIZE - 1U - current_length) {
        copy_length = BROWSER_SEARCH_QUERY_SIZE - 1U - current_length;
    }
    memcpy(app->search_query + current_length, text, copy_length);
    app->search_query[current_length + copy_length] = '\0';
    if (refresh_file_search(app) < 0) return -1;
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
    if (app->active_app == DESKTOP_APP_GALLERY) {
        return handle_gallery_touch(app, input);
    }
    int width = (int)app->display.variable_info.xres;
    size_t visible = file_page_visible_rows(app);
    struct file_page_button_layout buttons = file_page_button_layout(width);

    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT &&
        buttons.show_search_sort && input->x >= buttons.search_x &&
        input->x < buttons.search_x + FILE_PAGE_SEARCH_WIDTH) {
        if (file_view_is_virtual(app)) {
            if (switch_file_view(app, BROWSER_FILE_VIEW_DIRECTORY) < 0) {
                return -1;
            }
            return render_file_page(app);
        }
        return handle_file_key(app, INPUT_ACTION_SEARCH);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT &&
        buttons.show_search_sort && input->x >= buttons.sort_x &&
        input->x < buttons.sort_x + FILE_PAGE_SORT_WIDTH) {
        return handle_file_key(app, INPUT_ACTION_SORT);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT && buttons.show_recent_favorite &&
        input->x >= buttons.recent_x &&
        input->x < buttons.recent_x + FILE_PAGE_RECENT_WIDTH) {
        if (switch_file_view(app, BROWSER_FILE_VIEW_RECENT) < 0) return -1;
        return render_file_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT && buttons.show_recent_favorite &&
        input->x >= buttons.favorite_x &&
        input->x < buttons.favorite_x + FILE_PAGE_FAVORITE_WIDTH) {
        if (switch_file_view(app, BROWSER_FILE_VIEW_FAVORITES) < 0) {
            return -1;
        }
        return render_file_page(app);
    }

    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT &&
        input->x >= buttons.up_x &&
        input->x < buttons.up_x + FILE_PAGE_UP_WIDTH) {
        if (file_view_is_virtual(app)) {
            if (switch_file_view(app, BROWSER_FILE_VIEW_DIRECTORY) < 0) {
                return -1;
            }
            return render_file_page(app);
        }
        int result = enter_parent(app);

        if (result < 0) {
            return -1;
        }
        return result == 0 ? browser_app_return_to_desktop(app) :
               render_file_page(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->y < UI_HEADER_HEIGHT &&
        input->x >= buttons.home_x &&
        input->x < buttons.home_x + FILE_PAGE_HOME_WIDTH) {
        return browser_app_return_to_desktop(app);
    }
    if (input->touch == TOUCH_ACTION_TAP &&
        input->y >= FILE_PAGE_LIST_TOP &&
        input->y < (int)app->display.variable_info.yres - UI_FOOTER_HEIGHT) {
        int row_height = file_page_row_height(app);
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
