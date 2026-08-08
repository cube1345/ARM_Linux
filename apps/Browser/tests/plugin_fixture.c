#include "plugin_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 判断 fixture 图片 decoder 是否支持指定路径。 */
static int supports_plugin_image(const char *path, enum file_type type)
{
    return path != NULL && strstr(path, ".plugimg") != NULL &&
           type == FILE_TYPE_PLUGIN_IMAGE;
}

/** @brief 提供 fixture 图片解码入口。 */
static int decode_plugin_image(const char *path, struct image_data *image)
{
    (void)path;
    (void)image;
    return 0;
}

/** @brief 判断 fixture 音频 backend 是否支持指定路径。 */
static int supports_plugin_audio(const char *path)
{
    return path != NULL && strstr(path, ".plugaud") != NULL;
}

/** @brief 提供 fixture 音频播放入口。 */
static int play_plugin_audio(struct audio_player *player)
{
    (void)player;
    return 0;
}

/** @brief 提供 fixture 页面渲染入口。 */
static int render_plugin_page(struct browser_app *app)
{
    (void)app;
    return 0;
}

/** @brief 提供 fixture 桌面应用启动入口。 */
static int launch_plugin_app(
    struct browser_app *app,
    const struct desktop_app_operation *operation)
{
    (void)app;
    (void)operation;
    return 0;
}

/** @brief 提供 fixture display 打开入口。 */
static int open_plugin_display(struct bmp_display *display, const char *path)
{
    (void)display;
    (void)path;
    return 0;
}

/** @brief 提供 fixture display 关闭入口。 */
static void close_plugin_display(struct bmp_display *display)
{
    (void)display;
}

static struct image_decoder image_decoder = {
    "fixture-image", supports_plugin_image, decode_plugin_image, NULL
};
static struct audio_backend_operation audio_backend = {
    "fixture-audio", supports_plugin_audio, play_plugin_audio, NULL
};
static struct page_operation page = {
    BROWSER_PAGE_PLUGIN_BASE, "fixture-page", render_plugin_page,
    NULL, NULL, NULL, NULL, NULL
};
static struct desktop_app_operation desktop_app = {
    DESKTOP_APP_PLUGIN_BASE, "Fixture", "Dynamic plugin", "SO",
    0x55aa77U, FILE_LIST_FILTER_ALL, launch_plugin_app, NULL
};
static struct display_operation display = {
    "fixture-display", open_plugin_display, close_plugin_display, NULL
};

/** @brief 返回 fixture 插件 ABI 版本。 */
uint32_t browser_plugin_abi_version(void)
{
    return BROWSER_PLUGIN_ABI_VERSION;
}

/** @brief 写入 marker，证明插件 shutdown 在卸载前执行。 */
static void plugin_shutdown(void)
{
    const char *path = getenv("BROWSER_PLUGIN_SHUTDOWN_MARKER");
    FILE *file;

    if (path == NULL || path[0] == '\0') return;
    file = fopen(path, "w");
    if (file == NULL) return;
    fputs("shutdown\n", file);
    fclose(file);
}

/** @brief 注册 fixture 提供的全部 operation。 */
int browser_plugin_init(const struct browser_plugin_host *host,
                        struct browser_plugin *plugin)
{
    if (host == NULL || plugin == NULL || host->abi_version !=
        BROWSER_PLUGIN_ABI_VERSION ||
        host->register_image_decoder(host->context, &image_decoder) < 0 ||
        host->register_audio_backend(host->context, &audio_backend) < 0 ||
        host->register_page(host->context, &page) < 0 ||
        host->register_desktop_app(host->context, &desktop_app) < 0 ||
        host->register_display(host->context, &display) < 0 ||
        host->register_file_extension(host->context, ".plugimg",
                                      FILE_TYPE_PLUGIN_IMAGE) < 0 ||
        host->register_file_extension(host->context, ".plugaud",
                                      FILE_TYPE_PLUGIN_AUDIO) < 0) {
        return -1;
    }
    plugin->abi_version = BROWSER_PLUGIN_ABI_VERSION;
    plugin->name = "fixture";
    plugin->shutdown = plugin_shutdown;
    return 0;
}
