#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <stddef.h>
#include <stdint.h>

struct browser_app;

/** @brief 诊断状态 operation 回调集合。 */
struct debug_operation {
    const char *name;
    uint32_t color;
    int (*status)(const struct browser_app *app, char *output,
                  size_t output_size);
    struct debug_operation *next;
};

/** @brief 诊断状态 operation 管理器。 */
struct debug_manager {
    struct debug_operation *head;
    struct debug_operation *tail;
    size_t count;
};

/** @brief 初始化诊断管理器。 */
void debug_manager_init(struct debug_manager *manager);

/** @brief 注册诊断 operation。 */
int debug_manager_register(struct debug_manager *manager,
                           struct debug_operation *operation);

/** @brief 注册内置诊断 operation。 */
int debug_manager_register_builtin(struct debug_manager *manager);

/** @brief 按索引获取诊断 operation。 */
const struct debug_operation *debug_manager_at(
    const struct debug_manager *manager, size_t index);

#endif
