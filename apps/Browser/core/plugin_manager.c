#include "plugin_manager.h"

#include "browser_log.h"

#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_NAME_MAX_COUNT 64U
#define PLUGIN_SUFFIX ".so"

typedef uint32_t (*plugin_abi_version_fn)(void);
typedef int (*plugin_init_fn)(const struct browser_plugin_host *host,
                              struct browser_plugin *plugin);

static int plugin_register_image(void *context,
                                 struct image_decoder *decoder)
{
    struct browser_plugin_host *host = context;

    return host == NULL ? -1 : image_decoder_register(host->images, decoder);
}

static int plugin_register_audio(void *context,
                                 struct audio_backend_operation *backend)
{
    struct browser_plugin_host *host = context;

    return host == NULL ? -1 : audio_backend_register(host->audio, backend);
}

static int plugin_register_page(void *context, struct page_operation *page)
{
    struct browser_plugin_host *host = context;

    return host == NULL ? -1 : page_manager_register(host->pages, page);
}

static int plugin_register_desktop(void *context,
                                   struct desktop_app_operation *application)
{
    struct browser_plugin_host *host = context;

    return host == NULL ? -1 : desktop_app_register(host->desktop_apps,
                                                     application);
}

static int plugin_register_display(void *context,
                                   struct display_operation *display)
{
    struct browser_plugin_host *host = context;

    return host == NULL ? -1 : display_manager_register(host->displays,
                                                         display);
}

static int plugin_register_extension(void *context, const char *extension,
                                     enum file_type type)
{
    (void)context;
    return file_list_register_extension(extension, type);
}

/** @brief 初始化动态插件 host ABI。 */
void browser_plugin_host_init(struct browser_plugin_host *host,
                              struct image_decoder_manager *images,
                              struct audio_backend_manager *audio,
                              struct page_manager *pages,
                              struct desktop_app_manager *desktop_apps,
                              struct display_manager *displays)
{
    if (host == NULL) return;
    memset(host, 0, sizeof(*host));
    host->abi_version = BROWSER_PLUGIN_ABI_VERSION;
    host->context = host;
    host->images = images;
    host->audio = audio;
    host->pages = pages;
    host->desktop_apps = desktop_apps;
    host->displays = displays;
    host->register_image_decoder = plugin_register_image;
    host->register_audio_backend = plugin_register_audio;
    host->register_page = plugin_register_page;
    host->register_desktop_app = plugin_register_desktop;
    host->register_display = plugin_register_display;
    host->register_file_extension = plugin_register_extension;
}

/** @brief 初始化动态插件 manager。 */
void browser_plugin_manager_init(struct browser_plugin_manager *manager)
{
    if (manager == NULL) return;
    memset(manager, 0, sizeof(*manager));
}

/** @brief 判断目录项是否为动态插件文件。 */
static int is_plugin_name(const char *name)
{
    size_t name_length;
    size_t suffix_length = sizeof(PLUGIN_SUFFIX) - 1U;

    if (name == NULL) return 0;
    name_length = strlen(name);
    return name_length > suffix_length &&
           strcmp(name + name_length - suffix_length, PLUGIN_SUFFIX) == 0;
}

/** @brief 对插件文件名排序，保证加载顺序稳定。 */
static int compare_plugin_names(const void *left, const void *right)
{
    const char *const *left_name = left;
    const char *const *right_name = right;

    return strcmp(*left_name, *right_name);
}

/** @brief 将动态符号安全转换为函数指针。 */
static void *resolve_symbol(void *handle, const char *name)
{
    return dlsym(handle, name);
}

/** @brief 为下一个插件句柄预留空间。 */
static int reserve_plugin(struct browser_plugin_manager *manager)
{
    struct browser_plugin_handle *items;

    if (manager->count < manager->capacity) return 0;
    {
        size_t capacity = manager->capacity == 0 ? 4U :
            manager->capacity * 2U;

        items = realloc(manager->items, capacity * sizeof(*items));
        if (items == NULL) {
            errno = ENOMEM;
            return -1;
        }
        manager->items = items;
        manager->capacity = capacity;
    }
    return 0;
}

/** @brief 保存一个已初始化或待安全清理的插件句柄。 */
static void append_plugin(struct browser_plugin_manager *manager,
                          void *handle,
                          const struct browser_plugin *plugin,
                          const char *path)
{
    manager->items[manager->count].handle = handle;
    manager->items[manager->count].descriptor = *plugin;
    snprintf(manager->items[manager->count].path,
             sizeof(manager->items[manager->count].path), "%s", path);
    manager->count++;
}

/** @brief 加载指定目录中的所有 `.so` 插件。 */
int browser_plugin_manager_load(struct browser_plugin_manager *manager,
                                const char *directory,
                                const struct browser_plugin_host *host)
{
    DIR *stream;
    struct dirent *entry;
    char *names[PLUGIN_NAME_MAX_COUNT];
    size_t name_count = 0;
    size_t index;

    if (manager == NULL || directory == NULL || host == NULL ||
        host->abi_version != BROWSER_PLUGIN_ABI_VERSION) {
        errno = EINVAL;
        return -1;
    }
    stream = opendir(directory);
    if (stream == NULL) {
        if (errno == ENOENT) return 0;
        browser_log_errno(BROWSER_LOG_WARN, directory);
        return -1;
    }
    while ((entry = readdir(stream)) != NULL) {
        if (!is_plugin_name(entry->d_name)) continue;
        if (name_count >= PLUGIN_NAME_MAX_COUNT) {
            browser_log(BROWSER_LOG_WARN, "too many plugins in %s", directory);
            break;
        }
        names[name_count] = strdup(entry->d_name);
        if (names[name_count] == NULL) {
            closedir(stream);
            errno = ENOMEM;
            goto cleanup_names;
        }
        name_count++;
    }
    if (closedir(stream) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "closedir plugin directory");
    }
    qsort(names, name_count, sizeof(names[0]), compare_plugin_names);
    for (index = 0; index < name_count; index++) {
        char path[PATH_MAX];
        void *handle;
        plugin_abi_version_fn get_abi;
        plugin_init_fn init;
        struct browser_plugin plugin;
        void *symbol;

        if (snprintf(path, sizeof(path), "%s/%s", directory,
                     names[index]) >= (int)sizeof(path)) {
            browser_log(BROWSER_LOG_WARN, "plugin path too long: %s",
                        names[index]);
            continue;
        }
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (handle == NULL) {
            browser_log(BROWSER_LOG_WARN, "load plugin %s: %s", path,
                        dlerror());
            continue;
        }
        symbol = resolve_symbol(handle, "browser_plugin_abi_version");
        memset(&get_abi, 0, sizeof(get_abi));
        if (symbol != NULL) {
            memcpy(&get_abi, &symbol, sizeof(get_abi));
        }
        symbol = resolve_symbol(handle, "browser_plugin_init");
        memset(&init, 0, sizeof(init));
        if (symbol != NULL) {
            memcpy(&init, &symbol, sizeof(init));
        }
        if (get_abi == NULL || init == NULL || get_abi() !=
            BROWSER_PLUGIN_ABI_VERSION) {
            browser_log(BROWSER_LOG_WARN, "skip incompatible plugin: %s",
                        path);
            dlclose(handle);
            continue;
        }
        if (reserve_plugin(manager) < 0) {
            dlclose(handle);
            goto cleanup_names;
        }
        memset(&plugin, 0, sizeof(plugin));
        plugin.abi_version = BROWSER_PLUGIN_ABI_VERSION;
        if (init(host, &plugin) < 0 || plugin.abi_version !=
            BROWSER_PLUGIN_ABI_VERSION || plugin.name == NULL) {
            browser_log(BROWSER_LOG_WARN, "plugin initialization failed: %s",
                        path);
            append_plugin(manager, handle, &plugin, path);
            errno = EPROTO;
            goto cleanup_names;
        }
        append_plugin(manager, handle, &plugin, path);
        browser_log(BROWSER_LOG_INFO, "plugin loaded: %s", plugin.name);
    }
    for (index = 0; index < name_count; index++) free(names[index]);
    return 0;

cleanup_names:
    while (name_count > 0) free(names[--name_count]);
    return -1;
}

/** @brief 关闭所有插件并释放句柄。 */
void browser_plugin_manager_destroy(struct browser_plugin_manager *manager)
{
    size_t index;

    if (manager == NULL) return;
    file_list_clear_registered_extensions();
    for (index = manager->count; index > 0; index--) {
        struct browser_plugin_handle *plugin = &manager->items[index - 1U];

        if (plugin->descriptor.shutdown != NULL) {
            plugin->descriptor.shutdown();
        }
        if (plugin->handle != NULL) dlclose(plugin->handle);
    }
    free(manager->items);
    memset(manager, 0, sizeof(*manager));
}
