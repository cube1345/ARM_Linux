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

int render_video_page(struct browser_app *app)
{
    struct media_player_status status;
    uint64_t serial = 0;
    int has_frame;
    char title[FILE_LIST_NAME_SIZE + 16];

    media_player_get_status(&app->media, &status);
    if (status.frame_serial != app->media_frame_serial) {
        image_data_destroy(&app->media_frame);
        has_frame = media_player_copy_frame(&app->media, &app->media_frame,
                                            &serial);
        if (has_frame) app->media_frame_serial = serial;
    }
    if (app->media_frame.pixels != NULL) {
        if (image_render_draw(&app->display, &app->media_frame, 0) < 0) {
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
    snprintf(title, sizeof(title), "PLAYER  %s",
             app->files.entries[app->selected].name);
    browser_ui_draw_back_button(&app->display, &app->font);
    ui_draw_text(&app->display, &app->font, title, UI_BUTTON_SIZE + 16,
                 40, (int)app->display.variable_info.xres - UI_BUTTON_SIZE - 32,
                 UI_TEXT, UI_BACKGROUND);
    draw_video_controls(app, &status);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Space play/pause  ←/→ seek  +/- volume");
    app->last_media_refresh_ms = monotonic_ms();
    return bmp_display_flush(&app->display);
}

int update_video_page(struct browser_app *app, uint64_t now_ms)
{
    if (now_ms - app->last_media_refresh_ms < UI_AUDIO_REFRESH_MS) return 0;
    return render_video_page(app);
}

int handle_video_key(struct browser_app *app, enum input_action action)
{
    struct media_player_status status;
    int percent = 0;

    media_player_get_status(&app->media, &status);
    if (action == INPUT_ACTION_TOGGLE) {
        media_player_toggle_pause(&app->media);
    } else if (action == INPUT_ACTION_PREVIOUS || action == INPUT_ACTION_NEXT) {
        if (status.duration_ms > 0) {
            percent = (int)(status.position_ms * 100U / status.duration_ms);
        }
        percent += action == INPUT_ACTION_NEXT ? 5 : -5;
        media_player_seek_percent(&app->media, percent);
    } else if (action == INPUT_ACTION_VOLUME_UP) {
        browser_app_set_volume(app, status.volume + 5);
    } else if (action == INPUT_ACTION_VOLUME_DOWN) {
        browser_app_set_volume(app, status.volume - 5);
    } else {
        return 0;
    }
    return render_video_page(app);
}

int handle_video_touch(struct browser_app *app,
                       const struct browser_input *input)
{
    struct media_player_status status;
    int height = (int)app->display.variable_info.yres;
    int progress_y = height - UI_FOOTER_HEIGHT - 58;
    int volume_y = height - UI_FOOTER_HEIGHT - 28;
    int percent;

    if (input->touch != TOUCH_ACTION_TAP && input->touch != TOUCH_ACTION_MOVE) {
        return 0;
    }
    percent = browser_ui_bar_percent(&app->display, input->x);
    if (browser_ui_touches_bar(input, progress_y)) {
        media_player_seek_percent(&app->media, percent);
    } else if (browser_ui_touches_bar(input, volume_y)) {
        browser_app_set_volume(app, percent);
    } else {
        return 0;
    }
    media_player_get_status(&app->media, &status);
    (void)status;
    return render_video_page(app);
}
