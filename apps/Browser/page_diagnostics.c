#include "page_diagnostics.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "input_keyboard.h"
#include "ui_draw.h"

#include <libavutil/avutil.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DIAGNOSTICS_ROW_TOP (UI_HEADER_HEIGHT + 22)
#define DIAGNOSTICS_ROW_HEIGHT 58
#define DIAGNOSTICS_ROW_GAP 4

/**
 * @brief 绘制一条诊断状态。
 * @param app 浏览器上下文。
 * @param row 行索引。
 * @param label 状态名称。
 * @param value 状态内容。
 * @param color 标签颜色。
 */
static void draw_diagnostics_row(struct browser_app *app, int row,
                                 const char *label, const char *value,
                                 uint32_t color)
{
    int width = (int)app->display.variable_info.xres - UI_MARGIN * 2;
    int y = DIAGNOSTICS_ROW_TOP +
            row * (DIAGNOSTICS_ROW_HEIGHT + DIAGNOSTICS_ROW_GAP);

    browser_ui_draw_panel(&app->display, UI_MARGIN, y, width,
                          DIAGNOSTICS_ROW_HEIGHT, UI_SURFACE, UI_BORDER);
    ui_draw_rect(&app->display, UI_MARGIN + 16, y + 18,
                 116, DIAGNOSTICS_ROW_HEIGHT - 36, color);
    ui_draw_text(&app->display, &app->font, label, UI_MARGIN + 28,
                 y + 18 + (int)app->font.pixel_size + 6,
                 92, UI_BACKGROUND, color);
    ui_draw_text(&app->display, &app->font, value, UI_MARGIN + 154,
                 y + (DIAGNOSTICS_ROW_HEIGHT +
                      (int)app->font.pixel_size) / 2 - 4,
                 width - 174, UI_TEXT, UI_SURFACE);
}

/**
 * @brief 生成输入 operation 摘要。
 * @param app 浏览器上下文。
 * @param output 输出字符串。
 * @param output_size 输出缓冲区大小。
 */
static void input_summary(const struct browser_app *app, char *output,
                          size_t output_size)
{
    struct input_operation *operation;
    size_t used = 0;

    output[0] = '\0';
    for (operation = app->input.operations; operation != NULL;
         operation = operation->next) {
        int written = snprintf(output + used, output_size - used,
                               used == 0 ? "%s" : " + %s",
                               operation->name);

        if (written < 0 || (size_t)written >= output_size - used) {
            break;
        }
        used += (size_t)written;
    }
    if (used == 0) {
        snprintf(output, output_size, "No input operation");
    }
}

/**
 * @brief 绘制设备与运行环境诊断页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_diagnostics_page(struct browser_app *app)
{
    char display[96];
    char input[96];
    char audio[128];
    char ffmpeg[96];
    char tools[160];
    int tools_ready;

    snprintf(display, sizeof(display), "%ux%u  %u bpp  framebuffer",
             app->display.variable_info.xres,
             app->display.variable_info.yres,
             app->display.variable_info.bits_per_pixel);
    input_summary(app, input, sizeof(input));
    snprintf(audio, sizeof(audio), "%s  /dev/snd: %s",
             app->alsa_device,
             access("/dev/snd", F_OK) == 0 ? "READY" : "NOT FOUND");
    snprintf(ffmpeg, sizeof(ffmpeg), "libavutil %s  software decode",
             av_version_info());
    tools_ready = access("/usr/bin/evtest", X_OK) == 0 &&
                  access("/usr/bin/fbgrab", X_OK) == 0 &&
                  access("/usr/bin/strace", X_OK) == 0;
    snprintf(tools, sizeof(tools), "evtest / fbgrab / strace: %s",
             tools_ready ? "READY" : "CHECK ROOTFS");
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_navigation_header(&app->display, &app->font,
                                      "Diagnostics",
                                      "Runtime devices and support tools");
    draw_diagnostics_row(app, 0, "DISPLAY", display, UI_ACCENT);
    draw_diagnostics_row(app, 1, "INPUT", input, UI_ACCENT_2);
    draw_diagnostics_row(app, 2, "AUDIO", audio, UI_WARNING);
    draw_diagnostics_row(app, 3, "FFMPEG", ffmpeg, UI_ACCENT_2);
    draw_diagnostics_row(app, 4, "TOOLS", tools, UI_SELECTED_BORDER);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Esc or top-left button returns to desktop");
    return bmp_display_flush(&app->display);
}

/**
 * @brief 处理诊断页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0。
 */
int handle_diagnostics_key(struct browser_app *app,
                           enum input_action action)
{
    (void)app;
    (void)action;
    return 0;
}

/**
 * @brief 处理诊断页面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0。
 */
int handle_diagnostics_touch(struct browser_app *app,
                             const struct browser_input *input)
{
    (void)app;
    (void)input;
    return 0;
}
