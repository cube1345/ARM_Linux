#include "display_manager.h"

#include "browser_log.h"

#include <errno.h>

static struct display_operation framebuffer_operation;

/** @brief 初始化显示设备管理器。 */
void display_manager_init(struct display_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
        manager->tail = NULL;
        manager->active = NULL;
    }
}

/** @brief 注册显示设备 operation。 */
int display_manager_register(struct display_manager *manager,
                             struct display_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->open == NULL || operation->close == NULL) {
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
    return 0;
}

/** @brief 注册内置 framebuffer 显示设备 operation。 */
int display_manager_register_builtin(struct display_manager *manager)
{
    framebuffer_operation.name = "framebuffer";
    framebuffer_operation.open = bmp_display_open;
    framebuffer_operation.close = bmp_display_close;
    framebuffer_operation.next = NULL;
    return display_manager_register(manager, &framebuffer_operation);
}

/** @brief 使用已注册 operation 打开显示设备。 */
int display_manager_open(struct display_manager *manager,
                         struct bmp_display *display, const char *path)
{
    struct display_operation *operation;

    if (manager == NULL || display == NULL || path == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (operation = manager->head; operation != NULL;
         operation = operation->next) {
        if (operation->open(display, path) == 0) {
            manager->active = operation;
            browser_log(BROWSER_LOG_INFO, "display operation: %s",
                        operation->name);
            return 0;
        }
    }
    errno = ENODEV;
    return -1;
}

/** @brief 关闭当前显示设备。 */
void display_manager_close(struct display_manager *manager,
                           struct bmp_display *display)
{
    if (manager != NULL && manager->active != NULL && display != NULL) {
        manager->active->close(display);
        manager->active = NULL;
    } else if (display != NULL) {
        bmp_display_close(display);
    }
}

/** @brief 获取当前显示 operation 名称。 */
const char *display_manager_active_name(
    const struct display_manager *manager)
{
    if (manager == NULL || manager->active == NULL) {
        return "none";
    }
    return manager->active->name;
}
