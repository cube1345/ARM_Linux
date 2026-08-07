#include "desktop_app.h"

#include "browser_app.h"
#include "file_list.h"
#include "page_diagnostics.h"
#include "page_file.h"
#include "page_settings.h"
#include "page_tools.h"

#include <errno.h>

/**
 * @brief 启动需要文件列表的桌面应用。
 * @param app 浏览器上下文。
 * @param operation 应用 operation。
 * @return 成功返回 0，失败返回 -1。
 */
static int launch_file_application(
    struct browser_app *app,
    const struct desktop_app_operation *operation)
{
    if (file_list_scan_filtered(app->root, &app->files,
                                operation->file_filter) < 0) {
        return -1;
    }
    app->active_app = operation->id;
    app->file_filter = operation->file_filter;
    app->file_sort = FILE_LIST_SORT_NAME;
    file_list_sort(&app->files, app->file_sort);
    app->selected = 0;
    app->page = BROWSER_PAGE_FILES;
    return render_file_page(app);
}

/**
 * @brief 启动诊断应用。
 * @param app 浏览器上下文。
 * @param operation 应用 operation。
 * @return 成功返回 0，失败返回 -1。
 */
static int launch_diagnostics(
    struct browser_app *app,
    const struct desktop_app_operation *operation)
{
    app->active_app = operation->id;
    app->page = BROWSER_PAGE_DIAGNOSTICS;
    return render_diagnostics_page(app);
}

/**
 * @brief 启动设置应用。
 * @param app 浏览器上下文。
 * @param operation 应用 operation。
 * @return 成功返回 0，失败返回 -1。
 */
static int launch_settings(
    struct browser_app *app,
    const struct desktop_app_operation *operation)
{
    app->active_app = operation->id;
    app->page = BROWSER_PAGE_SETTINGS;
    return render_settings_page(app);
}

/**
 * @brief 启动外部工具应用。
 * @param app 浏览器上下文。
 * @param operation 应用 operation。
 * @return 成功返回 0，失败返回 -1。
 */
static int launch_tools(
    struct browser_app *app,
    const struct desktop_app_operation *operation)
{
    app->active_app = operation->id;
    app->tool_selected = 0;
    app->tool_output[0] = '\0';
    app->tool_status[0] = '\0';
    app->page = BROWSER_PAGE_TOOLS;
    return render_tools_page(app);
}

static struct desktop_app_operation builtin_apps[] = {
    {
        DESKTOP_APP_GALLERY, "Gallery", "Photos + GIF",
        "PIC", 0x4fc3a1U, FILE_LIST_FILTER_IMAGES,
        launch_file_application, NULL
    },
    {
        DESKTOP_APP_PLAYER, "Player", "Audio + video",
        "PLAY", 0x7aa2ffU, FILE_LIST_FILTER_AUDIO_VIDEO,
        launch_file_application, NULL
    },
    {
        DESKTOP_APP_FILES, "Files", "All media files",
        "FILE", 0xffc857U, FILE_LIST_FILTER_ALL,
        launch_file_application, NULL
    },
    {
        DESKTOP_APP_READER, "Reader", "UTF-8 documents",
        "TXT", 0xe58ca8U, FILE_LIST_FILTER_TEXT,
        launch_file_application, NULL
    },
    {
        DESKTOP_APP_DIAGNOSTICS, "Diagnostics", "Device tools",
        "SYS", 0x65c7d0U, 0U, launch_diagnostics, NULL
    },
    {
        DESKTOP_APP_TOOLS, "Tools", "Linux commands",
        "CLI", 0x72d572U, 0U, launch_tools, NULL
    },
    {
        DESKTOP_APP_SETTINGS, "Settings", "Volume + options",
        "SET", 0x9aa8b3U, 0U, launch_settings, NULL
    }
};

/**
 * @brief 初始化桌面应用管理器。
 * @param manager 桌面应用管理器。
 */
void desktop_app_manager_init(struct desktop_app_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
        manager->tail = NULL;
        manager->count = 0;
    }
}

/**
 * @brief 注册桌面应用 operation。
 * @param manager 桌面应用管理器。
 * @param operation 应用 operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_register(struct desktop_app_manager *manager,
                         struct desktop_app_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->summary == NULL || operation->badge == NULL ||
        operation->launch == NULL) {
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

/**
 * @brief 注册内置桌面应用。
 * @param manager 桌面应用管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_register_builtin(struct desktop_app_manager *manager)
{
    size_t index;

    for (index = 0; index < sizeof(builtin_apps) / sizeof(builtin_apps[0]);
         index++) {
        if (desktop_app_register(manager, &builtin_apps[index]) < 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief 按应用标识查找 operation。
 * @param manager 桌面应用管理器。
 * @param id 应用标识。
 * @return 找到返回 operation，否则返回 NULL。
 */
const struct desktop_app_operation *desktop_app_find(
    const struct desktop_app_manager *manager, enum desktop_app_id id)
{
    struct desktop_app_operation *operation;

    if (manager == NULL) {
        return NULL;
    }
    for (operation = manager->head; operation != NULL;
         operation = operation->next) {
        if (operation->id == id) {
            return operation;
        }
    }
    return NULL;
}

/**
 * @brief 按桌面顺序获取应用 operation。
 * @param manager 桌面应用管理器。
 * @param index 应用索引。
 * @return 找到返回 operation，否则返回 NULL。
 */
const struct desktop_app_operation *desktop_app_at(
    const struct desktop_app_manager *manager, size_t index)
{
    struct desktop_app_operation *operation;

    if (manager == NULL) {
        return NULL;
    }
    for (operation = manager->head; operation != NULL && index > 0;
         operation = operation->next) {
        index--;
    }
    return operation;
}

/**
 * @brief 启动指定桌面应用。
 * @param manager 桌面应用管理器。
 * @param app 浏览器上下文。
 * @param index 应用索引。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_launch(const struct desktop_app_manager *manager,
                       struct browser_app *app, size_t index)
{
    const struct desktop_app_operation *operation =
        desktop_app_at(manager, index);

    if (app == NULL || operation == NULL) {
        errno = EINVAL;
        return -1;
    }
    return operation->launch(app, operation);
}
