#include "font_manager.h"

#include "browser_log.h"

#include <errno.h>

static struct font_operation freetype_operation;

/** @brief 初始化字体管理器。 */
void font_manager_init(struct font_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
        manager->tail = NULL;
        manager->active = NULL;
    }
}

/** @brief 注册字体 operation。 */
int font_manager_register(struct font_manager *manager,
                          struct font_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->open == NULL || operation->set_size == NULL ||
        operation->close == NULL) {
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

/** @brief 注册内置 FreeType 字体 operation。 */
int font_manager_register_builtin(struct font_manager *manager)
{
    freetype_operation.name = "freetype-utf8";
    freetype_operation.open = font_renderer_open;
    freetype_operation.set_size = font_renderer_set_size;
    freetype_operation.close = font_renderer_close;
    freetype_operation.next = NULL;
    return font_manager_register(manager, &freetype_operation);
}

/** @brief 使用已注册 operation 打开字体。 */
int font_manager_open(struct font_manager *manager,
                      struct font_renderer *renderer, const char *path,
                      uint32_t pixel_size)
{
    struct font_operation *operation;

    if (manager == NULL || renderer == NULL || path == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (operation = manager->head; operation != NULL;
         operation = operation->next) {
        if (operation->open(renderer, path, pixel_size) == 0) {
            manager->active = operation;
            browser_log(BROWSER_LOG_INFO, "font operation: %s",
                        operation->name);
            return 0;
        }
    }
    errno = ENODEV;
    return -1;
}

/** @brief 修改当前字体大小。 */
int font_manager_set_size(const struct font_manager *manager,
                          struct font_renderer *renderer,
                          uint32_t pixel_size)
{
    if (manager == NULL || manager->active == NULL || renderer == NULL) {
        errno = EINVAL;
        return -1;
    }
    return manager->active->set_size(renderer, pixel_size);
}

/** @brief 关闭当前字体。 */
void font_manager_close(struct font_manager *manager,
                        struct font_renderer *renderer)
{
    if (manager != NULL && manager->active != NULL && renderer != NULL) {
        manager->active->close(renderer);
        manager->active = NULL;
    } else if (renderer != NULL) {
        font_renderer_close(renderer);
    }
}

/** @brief 获取当前字体 operation 名称。 */
const char *font_manager_active_name(const struct font_manager *manager)
{
    if (manager == NULL || manager->active == NULL) {
        return "none";
    }
    return manager->active->name;
}
