#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "browser_app.h"
#include "input_keyboard.h"

#include <stdint.h>

/** @brief 页面 operation 回调集合。 */
struct page_operation {
    enum browser_page page;
    const char *name;
    int (*render)(struct browser_app *app);
    int (*handle_key)(struct browser_app *app, enum input_action action);
    int (*handle_touch)(struct browser_app *app,
                        const struct browser_input *input);
    int (*periodic)(struct browser_app *app, uint64_t now_ms);
    int (*event_timeout)(const struct browser_app *app, uint64_t now_ms,
                         int current_timeout);
    struct page_operation *next;
};

/** @brief 页面 operation 链表管理器。 */
struct page_manager {
    struct page_operation *head;
};

/**
 * @brief 初始化页面管理器。
 * @param manager 页面管理器。
 */
void page_manager_init(struct page_manager *manager);

/**
 * @brief 注册页面 operation。
 * @param manager 页面管理器。
 * @param operation 页面 operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_register(struct page_manager *manager,
                          struct page_operation *operation);

/**
 * @brief 注册内置文件、图片、文本和音频页面。
 * @param manager 页面管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_register_builtin(struct page_manager *manager);

/**
 * @brief 查找指定页面 operation。
 * @param manager 页面管理器。
 * @param page 页面枚举。
 * @return 找到返回页面 operation，否则返回 NULL。
 */
const struct page_operation *page_manager_find(
    const struct page_manager *manager, enum browser_page page);

/**
 * @brief 渲染当前页面。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_render(const struct page_manager *manager,
                        struct browser_app *app);

/**
 * @brief 分发当前页面键盘动作。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int page_manager_handle_key(const struct page_manager *manager,
                            struct browser_app *app,
                            enum input_action action);

/**
 * @brief 分发当前页面触摸动作。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int page_manager_handle_touch(const struct page_manager *manager,
                              struct browser_app *app,
                              const struct browser_input *input);

/**
 * @brief 执行当前页面周期任务。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_periodic(const struct page_manager *manager,
                          struct browser_app *app, uint64_t now_ms);

/**
 * @brief 由当前页面调整事件等待时间。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @param current_timeout 当前等待时间。
 * @return 调整后的等待时间。
 */
int page_manager_event_timeout(const struct page_manager *manager,
                               const struct browser_app *app,
                               uint64_t now_ms, int current_timeout);

#endif
