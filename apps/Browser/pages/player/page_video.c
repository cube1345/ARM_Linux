#include "page_video.h"

#include "browser_ui.h"
#include "image_render.h"
#include "media_player.h"
#include "ui_draw.h"

#include <stdint.h>
#include <stdio.h>

#define VIDEO_PROGRESS_Y 0
#define VIDEO_VOLUME_Y 0
#define VIDEO_BAR_HEIGHT 12
#define VIDEO_BAR_INSET 28
#define VIDEO_PANEL_HEIGHT 220
#define VIDEO_PLAY_BUTTON_WIDTH 92
#define VIDEO_SCALE_BUTTON_WIDTH 72
#define VIDEO_FULLSCREEN_BUTTON_WIDTH 72
#define VIDEO_HEADER_BUTTON_HEIGHT 42
#define VIDEO_HEADER_BUTTON_GAP 4

/** @brief 获取视频缩放模式的短标签。 */
static const char *video_render_mode_name(enum image_render_mode mode)
{
    if (mode == IMAGE_RENDER_FILL) return "FILL";
    if (mode == IMAGE_RENDER_ORIGINAL) return "1:1";
    return "FIT";
}

/** @brief 循环切换 FIT、FILL 和原始大小缩放模式。 */
static void cycle_video_render_mode(struct browser_app *app)
{
    app->video_render_mode =
        (enum image_render_mode)(((int)app->video_render_mode + 1) % 3);
}

/** @brief 获取播放暂停按钮左坐标。 */
static int video_play_button_x(const struct browser_app *app)
{
    return (int)app->display.variable_info.xres - UI_MARGIN -
           VIDEO_PLAY_BUTTON_WIDTH;
}

/** @brief 获取全屏按钮左坐标。 */
static int video_fullscreen_button_x(const struct browser_app *app)
{
    return video_play_button_x(app) - VIDEO_HEADER_BUTTON_GAP -
           VIDEO_FULLSCREEN_BUTTON_WIDTH;
}

/** @brief 获取缩放模式按钮左坐标。 */
static int video_scale_button_x(const struct browser_app *app)
{
    return video_fullscreen_button_x(app) - VIDEO_HEADER_BUTTON_GAP -
           VIDEO_SCALE_BUTTON_WIDTH;
}

static int video_bar_x(void)
{
    return UI_MARGIN + VIDEO_BAR_INSET;
}

static int video_bar_width(const struct browser_app *app)
{
    int width = (int)app->display.variable_info.xres - UI_MARGIN * 2 -
                VIDEO_BAR_INSET * 2;

    return width > 1 ? width : 1;
}

static int video_panel_y(const struct browser_app *app)
{
    int height = (int)app->display.variable_info.yres;
    int y = (height - VIDEO_PANEL_HEIGHT) / 2;

    if (y < UI_HEADER_HEIGHT) y = UI_HEADER_HEIGHT;
    if (y + VIDEO_PANEL_HEIGHT > height - UI_FOOTER_HEIGHT) {
        y = height - UI_FOOTER_HEIGHT - VIDEO_PANEL_HEIGHT;
    }
    return y > UI_MARGIN ? y : UI_MARGIN;
}

static int status_progress(const struct media_player_status *status)
{
    if (status->duration_ms == 0) return 0;
    return (int)((status->position_ms * 100U) / status->duration_ms);
}

/** @brief 判断条目是否属于 FFmpeg 播放列表。 */
static int media_entry_supported(enum file_type type)
{
    return browser_file_type_is_audio(type) || browser_file_type_is_video(type);
}

/** @brief 查找播放列表中相邻的 FFmpeg 媒体条目。 */
static int find_adjacent_media(const struct browser_app *app, int direction)
{
    size_t count = app->files.count;
    size_t offset;
    size_t index;

    if (count == 0 || app->selected >= count) return -1;
    if (app->playback_mode == BROWSER_PLAYBACK_SHUFFLE && direction > 0) {
        size_t candidate = (size_t)(monotonic_ms() % count);

        for (offset = 0; offset < count; offset++) {
            index = (candidate + offset) % count;
            if (index != app->selected &&
                media_entry_supported(app->files.entries[index].type)) {
                return (int)index;
            }
        }
        return -1;
    }
    for (offset = 1; offset <= count; offset++) {
        if (direction > 0) {
            if (app->playback_mode == BROWSER_PLAYBACK_ONCE &&
                app->selected + offset >= count) return -1;
            index = (app->selected + offset) % count;
        } else {
            if (app->playback_mode == BROWSER_PLAYBACK_ONCE &&
                offset > app->selected) return -1;
            index = (app->selected + count - offset) % count;
        }
        if (media_entry_supported(app->files.entries[index].type)) {
            return (int)index;
        }
    }
    return -1;
}

/** @brief 启动指定 FFmpeg 播放列表条目。 */
static int start_media_index(struct browser_app *app, size_t index)
{
    char path[PATH_MAX];
    size_t previous = app->selected;

    if (file_list_path(&app->files, index, path, sizeof(path)) < 0) {
        return -1;
    }
    media_player_stop(&app->media);
    image_data_destroy(&app->media_frame);
    app->media_frame_serial = 0;
    if (media_player_start(&app->media, path, app->alsa_device) < 0) {
        app->selected = previous;
        return -1;
    }
    snprintf(app->current_path, sizeof(app->current_path), "%s", path);
    browser_app_restore_playback(app, path, BROWSER_PAGE_VIDEO);
    (void)subtitle_track_load_for_media(&app->subtitles, path);
    app->selected = index;
    app->page = BROWSER_PAGE_VIDEO;
    return 0;
}

/** @brief 打开相邻 FFmpeg 媒体条目。 */
static int play_adjacent_media(struct browser_app *app, int direction)
{
    int index = find_adjacent_media(app, direction);

    if (index < 0 || start_media_index(app, (size_t)index) < 0) return -1;
    return render_video_page(app);
}

static void draw_video_controls(struct browser_app *app,
                                const struct media_player_status *status)
{
    int width = video_bar_width(app);
    int x = video_bar_x();
    int height = (int)app->display.variable_info.yres;
    int progress_y = height - UI_FOOTER_HEIGHT - 58;
    int volume_y = height - UI_FOOTER_HEIGHT - 28;
    char elapsed[16];
    char duration[16];
    char timing[40];
    char volume[24];

    browser_ui_format_time(status->position_ms, elapsed, sizeof(elapsed));
    browser_ui_format_time(status->duration_ms, duration, sizeof(duration));
    snprintf(timing, sizeof(timing), "%s / %s", elapsed, duration);
    snprintf(volume, sizeof(volume), "Volume %d%%", status->volume);
    browser_ui_draw_progress_bar(&app->display, x, progress_y, width,
                                 VIDEO_BAR_HEIGHT, status_progress(status),
                                 UI_ACCENT);
    browser_ui_draw_progress_bar(&app->display, x, volume_y, width,
                                 VIDEO_BAR_HEIGHT, status->volume,
                                 UI_SELECTED);
    ui_draw_text(&app->display, &app->font, timing, x, progress_y - 8,
                 width, UI_TEXT, UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, volume, x, volume_y - 8,
                 width, UI_MUTED, UI_BACKGROUND);
}

/** @brief 在视频底部绘制当前 SRT 字幕。 */
static void draw_video_subtitle(struct browser_app *app,
                                const struct media_player_status *status)
{
    const char *text = subtitle_track_text_at(&app->subtitles,
                                               status->position_ms);
    int width;
    int baseline;

    if (text == NULL || status->width == 0 || status->height == 0) return;
    width = (int)app->display.variable_info.xres - UI_MARGIN * 2;
    baseline = app->video_fullscreen ?
               (int)app->display.variable_info.yres - UI_MARGIN - 8 :
               (int)app->display.variable_info.yres - UI_FOOTER_HEIGHT - 92;
    ui_draw_rect(&app->display, UI_MARGIN - 6,
                 baseline - (int)app->font.pixel_size - 6,
                 width + 12, (int)app->font.pixel_size + 14,
                 UI_BACKGROUND);
    ui_draw_text(&app->display, &app->font, text, UI_MARGIN, baseline,
                 width, UI_TEXT, UI_BACKGROUND);
}

int render_video_page(struct browser_app *app)
{
    struct media_player_status status;
    uint64_t serial = 0;
    int has_frame;
    char title[FILE_LIST_NAME_SIZE + 64];
    int title_x = UI_BUTTON_SIZE + 16;
    int scale_x = video_scale_button_x(app);
    int fullscreen_x = video_fullscreen_button_x(app);
    int play_x = video_play_button_x(app);
    int title_width;

    media_player_get_status(&app->media, &status);
    if (status.frame_serial != app->media_frame_serial) {
        image_data_destroy(&app->media_frame);
        has_frame = media_player_copy_frame(&app->media, &app->media_frame,
                                            &serial);
        if (has_frame) app->media_frame_serial = serial;
    }
    if (app->media_frame.pixels != NULL) {
        if (image_render_draw_mode(&app->display, &app->media_frame, 0,
                                   app->video_render_mode) < 0) {
            return -1;
        }
    } else {
        bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                          (uint8_t)(UI_BACKGROUND >> 8),
                          (uint8_t)UI_BACKGROUND);
        browser_ui_draw_panel(&app->display, UI_MARGIN, video_panel_y(app),
                              (int)app->display.variable_info.xres - UI_MARGIN * 2,
                              VIDEO_PANEL_HEIGHT, UI_SURFACE, UI_BORDER);
        ui_draw_text(&app->display, &app->font, "MEDIA", UI_MARGIN + 28,
                     video_panel_y(app) + 58, 160, UI_ACCENT_2, UI_SURFACE);
        ui_draw_text(&app->display, &app->font, "Audio stream",
                     UI_MARGIN + 28, video_panel_y(app) + 104, 300,
                     UI_TEXT, UI_SURFACE);
    }
    draw_video_subtitle(app, &status);
    if (app->video_fullscreen && app->media_frame.pixels != NULL) {
        app->last_media_refresh_ms = monotonic_ms();
        return bmp_display_flush(&app->display);
    }
    if (status.width > 0 && status.height > 0) {
        title_width = scale_x - title_x - VIDEO_HEADER_BUTTON_GAP;
        snprintf(title, sizeof(title), "PLAYER  %s  %ux%u  %.1ffps",
                 app->files.entries[app->selected].name,
                 status.width, status.height, status.frame_rate);
    } else {
        title_width = play_x - title_x - VIDEO_HEADER_BUTTON_GAP;
        snprintf(title, sizeof(title), "PLAYER  %s",
                 app->files.entries[app->selected].name);
    }
    browser_ui_draw_back_button(&app->display, &app->font);
    if (title_width > 0) {
        ui_draw_text(&app->display, &app->font, title, title_x, 40,
                     title_width, UI_TEXT, UI_BACKGROUND);
    }
    if (status.width > 0 && status.height > 0) {
        browser_ui_draw_button(&app->display, &app->font, scale_x, 10,
                               VIDEO_SCALE_BUTTON_WIDTH,
                               VIDEO_HEADER_BUTTON_HEIGHT,
                               video_render_mode_name(app->video_render_mode),
                               UI_HEADER);
        browser_ui_draw_button(&app->display, &app->font, fullscreen_x, 10,
                               VIDEO_FULLSCREEN_BUTTON_WIDTH,
                               VIDEO_HEADER_BUTTON_HEIGHT, "FULL", UI_HEADER);
    }
    browser_ui_draw_button(&app->display, &app->font,
                           play_x, 10, VIDEO_PLAY_BUTTON_WIDTH,
                           VIDEO_HEADER_BUTTON_HEIGHT,
                           status.state == MEDIA_PLAYER_PAUSED ? "PLAY" :
                           "PAUSE", UI_HEADER);
    draw_video_controls(app, &status);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Space pause  R scale  Enter full  ←/→ 10s");
    app->last_media_refresh_ms = monotonic_ms();
    return bmp_display_flush(&app->display);
}

int update_video_page(struct browser_app *app, uint64_t now_ms)
{
    struct media_player_status status;

    media_player_get_status(&app->media, &status);
    if (status.state == MEDIA_PLAYER_ENDED) {
        if (media_handle_completion(app) == 0) return 0;
    }
    if (now_ms - app->last_media_refresh_ms < UI_AUDIO_REFRESH_MS) return 0;
    return render_video_page(app);
}

int handle_video_key(struct browser_app *app, enum input_action action)
{
    struct media_player_status status;

    media_player_get_status(&app->media, &status);
    if (action == INPUT_ACTION_TOGGLE) {
        media_player_toggle_pause(&app->media);
    } else if (action == INPUT_ACTION_OPEN && status.width > 0 &&
               status.height > 0) {
        app->video_fullscreen = !app->video_fullscreen;
    } else if (action == INPUT_ACTION_ROTATE && status.width > 0 &&
               status.height > 0) {
        cycle_video_render_mode(app);
    } else if (action == INPUT_ACTION_UP) {
        return media_play_previous(app);
    } else if (action == INPUT_ACTION_DOWN) {
        return media_play_next(app);
    } else if (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT) {
        int64_t target = (int64_t)status.position_ms +
                         (action == INPUT_ACTION_NEXT ? 10000 : -10000);

        media_player_seek_ms(&app->media, target);
    } else if (action == INPUT_ACTION_VOLUME_UP) {
        browser_app_set_volume(app, status.volume + 5);
    } else if (action == INPUT_ACTION_VOLUME_DOWN) {
        browser_app_set_volume(app, status.volume - 5);
    } else {
        return 0;
    }
    return render_video_page(app);
}

/** @brief 打开当前媒体列表中的下一项。 */
int media_play_next(struct browser_app *app)
{
    return play_adjacent_media(app, 1);
}

/** @brief 打开当前媒体列表中的上一项。 */
int media_play_previous(struct browser_app *app)
{
    return play_adjacent_media(app, -1);
}

/** @brief 根据播放模式处理当前媒体自然结束。 */
int media_handle_completion(struct browser_app *app)
{
    if (app->playback_mode == BROWSER_PLAYBACK_ONCE) return -1;
    if (app->playback_mode == BROWSER_PLAYBACK_REPEAT_ONE) {
        if (start_media_index(app, app->selected) < 0) return -1;
        return render_video_page(app);
    }
    return media_play_next(app);
}

int handle_video_touch(struct browser_app *app,
                       const struct browser_input *input)
{
    struct media_player_status status;
    int height = (int)app->display.variable_info.yres;
    int progress_y = height - UI_FOOTER_HEIGHT - 58;
    int volume_y = height - UI_FOOTER_HEIGHT - 28;
    int bar_x = video_bar_x();
    int bar_width = video_bar_width(app);
    int play_x = (int)app->display.variable_info.xres - UI_MARGIN -
                 VIDEO_PLAY_BUTTON_WIDTH;
    int fullscreen_x = video_fullscreen_button_x(app);
    int scale_x = video_scale_button_x(app);
    int percent;

    if (input->touch != TOUCH_ACTION_TAP && input->touch != TOUCH_ACTION_MOVE) {
        return 0;
    }
    if (app->video_fullscreen) {
        if (input->touch == TOUCH_ACTION_TAP) {
            app->video_fullscreen = 0;
            return render_video_page(app);
        }
        return 0;
    }
    percent = browser_ui_bar_percent_at(input->x, bar_x, bar_width);
    if (input->x >= bar_x && input->x <= bar_x + bar_width &&
        browser_ui_touches_bar(input, progress_y)) {
        media_player_seek_percent(&app->media, percent);
    } else if (input->x >= bar_x && input->x <= bar_x + bar_width &&
               browser_ui_touches_bar(input, volume_y)) {
        browser_app_set_volume(app, percent);
    } else if (input->touch == TOUCH_ACTION_TAP && input->y < UI_HEADER_HEIGHT &&
               input->x >= play_x && input->x < play_x + VIDEO_PLAY_BUTTON_WIDTH) {
        media_player_toggle_pause(&app->media);
    } else if (input->touch == TOUCH_ACTION_TAP &&
               input->y < UI_HEADER_HEIGHT && input->x >= fullscreen_x &&
               input->x < fullscreen_x + VIDEO_FULLSCREEN_BUTTON_WIDTH) {
        media_player_get_status(&app->media, &status);
        if (status.width == 0 || status.height == 0) return 0;
        app->video_fullscreen = 1;
    } else if (input->touch == TOUCH_ACTION_TAP &&
               input->y < UI_HEADER_HEIGHT && input->x >= scale_x &&
               input->x < scale_x + VIDEO_SCALE_BUTTON_WIDTH) {
        media_player_get_status(&app->media, &status);
        if (status.width == 0 || status.height == 0) return 0;
        cycle_video_render_mode(app);
    } else {
        return 0;
    }
    return render_video_page(app);
}
