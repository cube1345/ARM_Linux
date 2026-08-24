#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "desktop_app.h"
#include "display_manager.h"
#include "file_list.h"
#include "image_decoder.h"
#include "page_manager.h"
#include "audio_player.h"

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define BROWSER_PLUGIN_ABI_VERSION 1U

struct browser_plugin_host {
    uint32_t abi_version;
    void *context;
    struct image_decoder_manager *images;
    struct audio_backend_manager *audio;
    struct page_manager *pages;
    struct desktop_app_manager *desktop_apps;
    struct display_manager *displays;
    int (*register_image_decoder)(void *context,
                                  struct image_decoder *decoder);
    int (*register_audio_backend)(void *context,
                                  struct audio_backend_operation *backend);
    int (*register_page)(void *context, struct page_operation *page);
    int (*register_desktop_app)(void *context,
                                struct desktop_app_operation *application);
    int (*register_display)(void *context,
                            struct display_operation *display);
    int (*register_file_extension)(void *context, const char *extension,
                                   enum file_type type);
};

/** @brief 已加载插件的描述信息。 */
struct browser_plugin {
    uint32_t abi_version;
    const char *name;
    void (*shutdown)(void);
};

/** @brief 动态插件句柄记录。 */
struct browser_plugin_handle {
    void *handle;
    struct browser_plugin descriptor;
    char path[PATH_MAX];
};

/** @brief 动态插件 manager。 */
struct browser_plugin_manager {
    struct browser_plugin_handle *items;
    size_t count;
    size_t capacity;
};

/** @brief 初始化插件 host。 */
void browser_plugin_host_init(struct browser_plugin_host *host,
                              struct image_decoder_manager *images,
                              struct audio_backend_manager *audio,
                              struct page_manager *pages,
                              struct desktop_app_manager *desktop_apps,
                              struct display_manager *displays);

/** @brief 初始化动态插件 manager。 */
void browser_plugin_manager_init(struct browser_plugin_manager *manager);

/**
 * @brief 加载指定目录中的所有 `.so` 插件。
 * @param manager 动态插件 manager。
 * @param directory 插件目录，不存在时视为无插件。
 * @param host 插件 host ABI。
 * @return 成功返回 0，参数、内存或插件初始化错误返回 -1。
 */
int browser_plugin_manager_load(struct browser_plugin_manager *manager,
                                const char *directory,
                                const struct browser_plugin_host *host);

/**
 * @brief 关闭所有插件并释放句柄。
 * @warning 必须在所有插件 operation 不再被调用后执行。
 */
void browser_plugin_manager_destroy(struct browser_plugin_manager *manager);

#endif
