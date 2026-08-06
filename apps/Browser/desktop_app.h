#ifndef DESKTOP_APP_H
#define DESKTOP_APP_H

#include <stddef.h>
#include <stdint.h>

struct browser_app;

/** @brief 桌面内置应用标识。 */
enum desktop_app_id {
    DESKTOP_APP_NONE = 0,
    DESKTOP_APP_GALLERY,
    DESKTOP_APP_PLAYER,
    DESKTOP_APP_FILES,
    DESKTOP_APP_READER,
    DESKTOP_APP_DIAGNOSTICS,
    DESKTOP_APP_SETTINGS
};

/** @brief 桌面应用 operation。 */
struct desktop_app_operation {
    enum desktop_app_id id;
    const char *name;
    const char *summary;
    const char *badge;
    uint32_t color;
    unsigned int file_filter;
    int (*launch)(struct browser_app *app,
                  const struct desktop_app_operation *operation);
    struct desktop_app_operation *next;
};

/** @brief 桌面应用 operation 链表管理器。 */
struct desktop_app_manager {
    struct desktop_app_operation *head;
    struct desktop_app_operation *tail;
    size_t count;
};

/**
 * @brief 初始化桌面应用管理器。
 * @param manager 桌面应用管理器。
 */
void desktop_app_manager_init(struct desktop_app_manager *manager);

/**
 * @brief 注册桌面应用 operation。
 * @param manager 桌面应用管理器。
 * @param operation 应用 operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_register(struct desktop_app_manager *manager,
                         struct desktop_app_operation *operation);

/**
 * @brief 注册内置桌面应用。
 * @param manager 桌面应用管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_register_builtin(struct desktop_app_manager *manager);

/**
 * @brief 按应用标识查找 operation。
 * @param manager 桌面应用管理器。
 * @param id 应用标识。
 * @return 找到返回 operation，否则返回 NULL。
 */
const struct desktop_app_operation *desktop_app_find(
    const struct desktop_app_manager *manager, enum desktop_app_id id);

/**
 * @brief 按桌面顺序获取应用 operation。
 * @param manager 桌面应用管理器。
 * @param index 应用索引。
 * @return 找到返回 operation，否则返回 NULL。
 */
const struct desktop_app_operation *desktop_app_at(
    const struct desktop_app_manager *manager, size_t index);

/**
 * @brief 启动指定桌面应用。
 * @param manager 桌面应用管理器。
 * @param app 浏览器上下文。
 * @param index 应用索引。
 * @return 成功返回 0，失败返回 -1。
 */
int desktop_app_launch(const struct desktop_app_manager *manager,
                       struct browser_app *app, size_t index);

#endif
