#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "font_renderer.h"

#include <stdint.h>

/** @brief 字体渲染 operation 回调集合。 */
struct font_operation {
    const char *name;
    int (*open)(struct font_renderer *renderer, const char *path,
                uint32_t pixel_size);
    int (*set_size)(struct font_renderer *renderer, uint32_t pixel_size);
    void (*close)(struct font_renderer *renderer);
    struct font_operation *next;
};

/** @brief 字体渲染 operation 管理器。 */
struct font_manager {
    struct font_operation *head;
    struct font_operation *tail;
    const struct font_operation *active;
};

/** @brief 初始化字体管理器。 */
void font_manager_init(struct font_manager *manager);

/** @brief 注册字体 operation。 */
int font_manager_register(struct font_manager *manager,
                          struct font_operation *operation);

/** @brief 注册内置 FreeType 字体 operation。 */
int font_manager_register_builtin(struct font_manager *manager);

/** @brief 使用已注册 operation 打开字体。 */
int font_manager_open(struct font_manager *manager,
                      struct font_renderer *renderer, const char *path,
                      uint32_t pixel_size);

/** @brief 修改当前字体大小。 */
int font_manager_set_size(const struct font_manager *manager,
                          struct font_renderer *renderer,
                          uint32_t pixel_size);

/** @brief 关闭当前字体。 */
void font_manager_close(struct font_manager *manager,
                        struct font_renderer *renderer);

/** @brief 获取当前字体 operation 名称。 */
const char *font_manager_active_name(const struct font_manager *manager);

#endif
