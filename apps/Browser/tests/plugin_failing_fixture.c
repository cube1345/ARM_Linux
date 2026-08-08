#include "plugin_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 判断测试图片后缀是否由失败插件接管。 */
static int supports_failing_image(const char *path, enum file_type type)
{
    return path != NULL && strstr(path, ".failimg") != NULL &&
           type == FILE_TYPE_PLUGIN_IMAGE;
}

/** @brief 提供不会被实际执行的测试图片解码入口。 */
static int decode_failing_image(const char *path, struct image_data *image)
{
    (void)path;
    (void)image;
    return 0;
}

static struct image_decoder image_decoder = {
    "failing-image", supports_failing_image, decode_failing_image, NULL
};

/** @brief 返回测试插件 ABI 版本。 */
uint32_t browser_plugin_abi_version(void)
{
    return BROWSER_PLUGIN_ABI_VERSION;
}

/** @brief 写入 marker，证明初始化失败的插件仍被安全关闭。 */
static void plugin_shutdown(void)
{
    const char *path = getenv("BROWSER_PLUGIN_SHUTDOWN_MARKER");
    FILE *file;

    if (path == NULL || path[0] == '\0') return;
    file = fopen(path, "w");
    if (file == NULL) return;
    fputs("failed-plugin-shutdown\n", file);
    fclose(file);
}

/** @brief 注册 operation 后主动失败，用于验证句柄保留语义。 */
int browser_plugin_init(const struct browser_plugin_host *host,
                        struct browser_plugin *plugin)
{
    if (host == NULL || plugin == NULL || host->abi_version !=
        BROWSER_PLUGIN_ABI_VERSION) {
        return -1;
    }
    plugin->abi_version = BROWSER_PLUGIN_ABI_VERSION;
    plugin->name = "failing-fixture";
    plugin->shutdown = plugin_shutdown;
    if (host->register_image_decoder(host->context, &image_decoder) < 0 ||
        host->register_file_extension(host->context, ".failimg",
                                      FILE_TYPE_PLUGIN_IMAGE) < 0) {
        return -1;
    }
    return -1;
}
