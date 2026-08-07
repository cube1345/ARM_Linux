#include "debug_manager.h"

#include "browser_app.h"
#include "browser_ui.h"
#include "display_manager.h"
#include "font_manager.h"
#include "input_keyboard.h"

#include <errno.h>
#include <libavutil/avutil.h>
#include <stdio.h>
#include <unistd.h>

static struct debug_operation display_operation;
static struct debug_operation input_operation;
static struct debug_operation audio_operation;
static struct debug_operation ffmpeg_operation;
static struct debug_operation tools_operation;

/** @brief 生成输入 operation 摘要。 */
static void debug_input_summary(const struct browser_app *app, char *output,
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

/** @brief 生成显示设备诊断摘要。 */
static int debug_display_status(const struct browser_app *app, char *output,
                                size_t output_size)
{
    snprintf(output, output_size, "%ux%u  %u bpp  %s",
             app->display.variable_info.xres,
             app->display.variable_info.yres,
             app->display.variable_info.bits_per_pixel,
             display_manager_active_name(&app->display_devices));
    return 0;
}

/** @brief 生成输入设备诊断摘要。 */
static int debug_input_status(const struct browser_app *app, char *output,
                              size_t output_size)
{
    debug_input_summary(app, output, output_size);
    return 0;
}

/** @brief 生成音频设备诊断摘要。 */
static int debug_audio_status(const struct browser_app *app, char *output,
                              size_t output_size)
{
    snprintf(output, output_size, "%s  /dev/snd: %s", app->alsa_device,
             access("/dev/snd", F_OK) == 0 ? "READY" : "NOT FOUND");
    return 0;
}

/** @brief 生成 FFmpeg/字体诊断摘要。 */
static int debug_ffmpeg_status(const struct browser_app *app, char *output,
                               size_t output_size)
{
    snprintf(output, output_size, "libavutil %s  font %s %upx",
             av_version_info(), font_manager_active_name(&app->fonts),
             app->font.pixel_size);
    return 0;
}

/** @brief 生成工具可用性诊断摘要。 */
static int debug_tools_status(const struct browser_app *app, char *output,
                              size_t output_size)
{
    int tools_ready;

    (void)app;
    tools_ready = access("/usr/bin/evtest", X_OK) == 0 &&
                  access("/usr/bin/fbgrab", X_OK) == 0 &&
                  access("/usr/bin/strace", X_OK) == 0;
    snprintf(output, output_size, "evtest / fbgrab / strace: %s",
             tools_ready ? "READY" : "CHECK ROOTFS");
    return 0;
}

/** @brief 初始化诊断管理器。 */
void debug_manager_init(struct debug_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
        manager->tail = NULL;
        manager->count = 0;
    }
}

/** @brief 注册诊断 operation。 */
int debug_manager_register(struct debug_manager *manager,
                           struct debug_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->status == NULL) {
        errno = EINVAL;
        return -1;
    }
    operation->next = NULL;
    if (manager->tail == NULL) {
        manager->head = operation;
    } else {
        manager->tail->next = operation;
    }
    manager->tail = operation;
    manager->count++;
    return 0;
}

/** @brief 注册内置诊断 operation。 */
int debug_manager_register_builtin(struct debug_manager *manager)
{
    display_operation.name = "DISPLAY";
    display_operation.color = UI_ACCENT;
    display_operation.status = debug_display_status;
    display_operation.next = NULL;
    input_operation.name = "INPUT";
    input_operation.color = UI_ACCENT_2;
    input_operation.status = debug_input_status;
    input_operation.next = NULL;
    audio_operation.name = "AUDIO";
    audio_operation.color = UI_WARNING;
    audio_operation.status = debug_audio_status;
    audio_operation.next = NULL;
    ffmpeg_operation.name = "FFMPEG";
    ffmpeg_operation.color = UI_ACCENT_2;
    ffmpeg_operation.status = debug_ffmpeg_status;
    ffmpeg_operation.next = NULL;
    tools_operation.name = "TOOLS";
    tools_operation.color = UI_SELECTED_BORDER;
    tools_operation.status = debug_tools_status;
    tools_operation.next = NULL;
    return debug_manager_register(manager, &display_operation) < 0 ||
           debug_manager_register(manager, &input_operation) < 0 ||
           debug_manager_register(manager, &audio_operation) < 0 ||
           debug_manager_register(manager, &ffmpeg_operation) < 0 ||
           debug_manager_register(manager, &tools_operation) < 0 ? -1 : 0;
}

/** @brief 按索引获取诊断 operation。 */
const struct debug_operation *debug_manager_at(
    const struct debug_manager *manager, size_t index)
{
    struct debug_operation *operation;

    if (manager == NULL) {
        return NULL;
    }
    for (operation = manager->head; operation != NULL && index > 0;
         operation = operation->next) {
        index--;
    }
    return operation;
}
