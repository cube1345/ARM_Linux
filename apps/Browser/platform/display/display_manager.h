#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "bmp_display.h"

/** @brief 显示设备 operation 回调集合。 */
struct display_operation {
    const char *name;
    int (*open)(struct bmp_display *display, const char *path);
    void (*close)(struct bmp_display *display);
    struct display_operation *next;
};

/** @brief 显示设备 operation 管理器。 */
struct display_manager {
    struct display_operation *head;
    struct display_operation *tail;
    const struct display_operation *active;
};

/**
 * @brief 初始化显示设备管理器。
 * @param manager 显示设备管理器。
 */
void display_manager_init(struct display_manager *manager);

/**
 * @brief 注册显示设备 operation。
 * @param manager 显示设备管理器。
 * @param operation 显示设备 operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int display_manager_register(struct display_manager *manager,
                             struct display_operation *operation);

/**
 * @brief 注册内置 framebuffer 显示设备 operation。
 * @param manager 显示设备管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int display_manager_register_builtin(struct display_manager *manager);

/**
 * @brief 使用已注册 operation 打开显示设备。
 * @param manager 显示设备管理器。
 * @param display framebuffer 显示上下文。
 * @param path 显示设备路径。
 * @return 成功返回 0，失败返回 -1。
 */
int display_manager_open(struct display_manager *manager,
                         struct bmp_display *display, const char *path);

/**
 * @brief 关闭当前显示设备。
 * @param manager 显示设备管理器。
 * @param display framebuffer 显示上下文。
 */
void display_manager_close(struct display_manager *manager,
                           struct bmp_display *display);

/**
 * @brief 获取当前显示 operation 名称。
 * @param manager 显示设备管理器。
 * @return 当前 operation 名称，未打开时返回 "none"。
 */
const char *display_manager_active_name(
    const struct display_manager *manager);

#endif
