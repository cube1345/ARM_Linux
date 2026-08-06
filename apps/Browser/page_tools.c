#include "page_tools.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "ui_draw.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#define TOOLS_LIST_TOP (UI_HEADER_HEIGHT + 16)
#define TOOLS_ROW_HEIGHT 66
#define TOOLS_ROW_GAP 6
#define TOOLS_OUTPUT_HEIGHT 202
#define TOOLS_OUTPUT_LINES 5
#define TOOLS_BUTTON_WIDTH 118
#define TOOLS_BUTTON_HEIGHT 42
#define TOOLS_OUTPUT_MAX 1800

/** @brief 可从桌面启动的外部工具。 */
struct tool_command {
    const char *name;
    const char *summary;
    const char *command;
    uint32_t color;
};

static const struct tool_command tool_commands[] = {
    {
        "Audio devices",
        "List ALSA playback devices",
        "/usr/bin/aplay -l 2>&1",
        UI_ACCENT_2
    },
    {
        "Mixer state",
        "Read current ALSA mixer",
        "/usr/bin/amixer 2>&1",
        UI_ACCENT
    },
    {
        "MP3 decoder",
        "Show mpg123 version",
        "/usr/bin/mpg123 --version 2>&1",
        UI_WARNING
    },
    {
        "Trace tool",
        "Show strace version",
        "/usr/bin/strace -V 2>&1",
        UI_SELECTED_BORDER
    },
    {
        "Framebuffer shot",
        "Capture /dev/fb0 to /tmp",
        "/usr/bin/fbgrab /tmp/browser-tools-shot.png 2>&1",
        0xe58ca8U
    },
    {
        "Input query",
        "Query keyboard ENTER key",
        "/usr/bin/evtest --query /dev/input/event0 EV_KEY KEY_ENTER 2>&1",
        0x72d572U
    }
};

/** @brief 获取工具数量。 */
static size_t tools_count(void)
{
    return sizeof(tool_commands) / sizeof(tool_commands[0]);
}

/** @brief 返回页面内容宽度。 */
static int tools_content_width(const struct browser_app *app)
{
    return (int)app->display.variable_info.xres - UI_MARGIN * 2;
}

/** @brief 返回输出面板 Y 坐标。 */
static int tools_output_y(const struct browser_app *app)
{
    int height = (int)app->display.variable_info.yres;

    return height - UI_FOOTER_HEIGHT - TOOLS_OUTPUT_HEIGHT - UI_MARGIN;
}

/** @brief 绘制一个外部工具条目。 */
static void draw_tool_row(struct browser_app *app, size_t index)
{
    int width = tools_content_width(app);
    int y = TOOLS_LIST_TOP + (int)index * (TOOLS_ROW_HEIGHT + TOOLS_ROW_GAP);
    uint32_t background = index == app->tool_selected ? UI_SELECTED :
                          (index % 2U == 0U ? UI_SURFACE :
                           UI_SURFACE_ALT);
    uint32_t border = index == app->tool_selected ? UI_SELECTED_BORDER :
                      UI_BORDER;
    const struct tool_command *tool = &tool_commands[index];

    browser_ui_draw_panel(&app->display, UI_MARGIN, y, width,
                          TOOLS_ROW_HEIGHT, background, border);
    ui_draw_rect(&app->display, UI_MARGIN + 14, y + 15, 96,
                 TOOLS_ROW_HEIGHT - 30, tool->color);
    ui_draw_text(&app->display, &app->font, "RUN", UI_MARGIN + 28,
                 y + 15 + (int)app->font.pixel_size + 4, 68,
                 UI_BACKGROUND, tool->color);
    ui_draw_text(&app->display, &app->font, tool->name, UI_MARGIN + 128,
                 y + (int)app->font.pixel_size + 8, width - 256,
                 UI_TEXT, background);
    ui_draw_text(&app->display, &app->font, tool->summary, UI_MARGIN + 128,
                 y + TOOLS_ROW_HEIGHT - 10, width - 256,
                 UI_MUTED, background);
    if (index == app->tool_selected) {
        browser_ui_draw_button(&app->display, &app->font,
                               UI_MARGIN + width - TOOLS_BUTTON_WIDTH - 14,
                               y + 12, TOOLS_BUTTON_WIDTH,
                               TOOLS_BUTTON_HEIGHT, "OPEN", UI_HEADER);
    }
}

/** @brief 绘制输出文本的一行。 */
static void draw_output_line(struct browser_app *app, const char *line,
                             int y, int width)
{
    ui_draw_text(&app->display, &app->font, line, UI_MARGIN + 24,
                 y, width - 48, UI_TEXT, UI_SURFACE);
}

/** @brief 绘制命令输出面板。 */
static void draw_tools_output(struct browser_app *app)
{
    int y = tools_output_y(app);
    int width = tools_content_width(app);
    const char *cursor = app->tool_output;
    int line;

    browser_ui_draw_panel(&app->display, UI_MARGIN, y, width,
                          TOOLS_OUTPUT_HEIGHT, UI_SURFACE, UI_BORDER);
    ui_draw_text(&app->display, &app->font, app->tool_status,
                 UI_MARGIN + 24, y + (int)app->font.pixel_size + 18,
                 width - 48, UI_SELECTED_BORDER, UI_SURFACE);
    if (cursor[0] == '\0') {
        cursor = "Select a tool and press Enter.";
    }
    for (line = 0; line < TOOLS_OUTPUT_LINES && cursor[0] != '\0'; line++) {
        char buffer[160];
        size_t length = 0;

        while (cursor[length] != '\0' && cursor[length] != '\n' &&
               length < sizeof(buffer) - 1U) {
            buffer[length] = cursor[length];
            length++;
        }
        buffer[length] = '\0';
        draw_output_line(app, buffer,
                         y + 58 + line * ((int)app->font.pixel_size + 8),
                         width);
        cursor += length;
        if (cursor[0] == '\n') {
            cursor++;
        }
    }
}

/** @brief 将命令退出状态格式化为用户可读文本。 */
static void format_tool_status(struct browser_app *app,
                               const struct tool_command *tool, int status)
{
    if (status < 0) {
        snprintf(app->tool_status, sizeof(app->tool_status),
                 "%s: command failed", tool->name);
    } else if (WIFEXITED(status)) {
        snprintf(app->tool_status, sizeof(app->tool_status),
                 "%s: exit %d", tool->name, WEXITSTATUS(status));
    } else {
        snprintf(app->tool_status, sizeof(app->tool_status),
                 "%s: stopped", tool->name);
    }
}

/** @brief 执行当前选中的外部工具并采集输出。 */
static int run_selected_tool(struct browser_app *app)
{
    const struct tool_command *tool;
    FILE *pipe;
    size_t used = 0;
    int status;

    if (app->tool_selected >= tools_count()) {
        errno = EINVAL;
        return -1;
    }
    tool = &tool_commands[app->tool_selected];
    snprintf(app->tool_status, sizeof(app->tool_status),
             "%s: running ...", tool->name);
    app->tool_output[0] = '\0';
    if (render_tools_page(app) < 0) {
        return -1;
    }
    pipe = popen(tool->command, "r");
    if (pipe == NULL) {
        snprintf(app->tool_output, sizeof(app->tool_output),
                 "popen failed: %s", strerror(errno));
        format_tool_status(app, tool, -1);
        return render_tools_page(app);
    }
    while (used < TOOLS_OUTPUT_MAX) {
        size_t remaining = sizeof(app->tool_output) - used - 1U;
        size_t got;

        if (remaining == 0U) {
            break;
        }
        got = fread(app->tool_output + used, 1, remaining, pipe);
        used += got;
        if (got == 0U) {
            break;
        }
    }
    app->tool_output[used] = '\0';
    status = pclose(pipe);
    if (used == 0U) {
        snprintf(app->tool_output, sizeof(app->tool_output),
                 "(no output)");
    }
    format_tool_status(app, tool, status);
    return render_tools_page(app);
}

/** @brief 绘制外部 Linux 工具启动页。 */
int render_tools_page(struct browser_app *app)
{
    size_t index;
    size_t count = tools_count();

    if (app->tool_selected >= count) {
        app->tool_selected = 0;
    }
    if (app->tool_status[0] == '\0') {
        snprintf(app->tool_status, sizeof(app->tool_status),
                 "External command launcher");
    }
    bmp_display_clear(&app->display, (uint8_t)(UI_BACKGROUND >> 16),
                      (uint8_t)(UI_BACKGROUND >> 8),
                      (uint8_t)UI_BACKGROUND);
    browser_ui_draw_navigation_header(&app->display, &app->font,
                                      "Tools",
                                      "Run trusted Linux ARM commands");
    for (index = 0; index < count; index++) {
        draw_tool_row(app, index);
    }
    draw_tools_output(app);
    browser_ui_draw_footer_hint(&app->display, &app->font,
                                "Up/Down select  Enter run  Esc desktop");
    return bmp_display_flush(&app->display);
}

/** @brief 处理外部 Linux 工具页键盘动作。 */
int handle_tools_key(struct browser_app *app, enum input_action action)
{
    size_t count = tools_count();

    if (action == INPUT_ACTION_UP && count > 0U) {
        app->tool_selected = (app->tool_selected + count - 1U) % count;
    } else if (action == INPUT_ACTION_DOWN && count > 0U) {
        app->tool_selected = (app->tool_selected + 1U) % count;
    } else if (action == INPUT_ACTION_OPEN) {
        return run_selected_tool(app);
    } else if (action == INPUT_ACTION_BACK) {
        return browser_app_return_to_desktop(app);
    } else {
        return 0;
    }
    return render_tools_page(app);
}

/** @brief 处理外部 Linux 工具页触摸动作。 */
int handle_tools_touch(struct browser_app *app,
                       const struct browser_input *input)
{
    size_t count = tools_count();

    if (input->touch != TOUCH_ACTION_TAP) {
        return 0;
    }
    if (input->x < UI_BUTTON_SIZE && input->y < UI_BUTTON_SIZE) {
        return browser_app_return_to_desktop(app);
    }
    if (input->y >= TOOLS_LIST_TOP &&
        input->y < tools_output_y(app)) {
        int row_height = TOOLS_ROW_HEIGHT + TOOLS_ROW_GAP;
        size_t row = (size_t)(input->y - TOOLS_LIST_TOP) /
                     (size_t)row_height;

        if (row < count) {
            app->tool_selected = row;
            return run_selected_tool(app);
        }
    }
    return 0;
}
