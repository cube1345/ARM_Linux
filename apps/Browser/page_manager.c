#include "page_manager.h"

#include "browser_ui.h"
#include "page_audio.h"
#include "page_file.h"
#include "page_image.h"
#include "page_text.h"

#include <errno.h>

static struct page_operation file_page_operation;
static struct page_operation image_page_operation;
static struct page_operation text_page_operation;
static struct page_operation audio_page_operation;

/**
 * @brief 执行音频页面周期任务。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
static int audio_page_periodic(struct browser_app *app, uint64_t now_ms)
{
    if (now_ms - app->last_audio_refresh_ms >= UI_AUDIO_REFRESH_MS) {
        return render_audio_page(app);
    }
    return 0;
}

/**
 * @brief 初始化页面管理器。
 * @param manager 页面管理器。
 */
void page_manager_init(struct page_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
    }
}

/**
 * @brief 注册页面 operation。
 * @param manager 页面管理器。
 * @param operation 页面 operation，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_register(struct page_manager *manager,
                          struct page_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->render == NULL) {
        errno = EINVAL;
        return -1;
    }
    operation->next = manager->head;
    manager->head = operation;
    return 0;
}

/**
 * @brief 注册内置文件、图片、文本和音频页面。
 * @param manager 页面管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_register_builtin(struct page_manager *manager)
{
    file_page_operation.page = BROWSER_PAGE_FILES;
    file_page_operation.name = "files";
    file_page_operation.render = render_file_page;
    file_page_operation.handle_key = handle_file_key;
    file_page_operation.handle_touch = handle_file_touch;
    file_page_operation.periodic = NULL;
    file_page_operation.event_timeout = NULL;
    file_page_operation.next = NULL;

    image_page_operation.page = BROWSER_PAGE_IMAGE;
    image_page_operation.name = "image";
    image_page_operation.render = render_image_page;
    image_page_operation.handle_key = handle_image_key;
    image_page_operation.handle_touch = handle_image_touch;
    image_page_operation.periodic = update_image_page;
    image_page_operation.event_timeout = image_page_event_timeout;
    image_page_operation.next = NULL;

    text_page_operation.page = BROWSER_PAGE_TEXT;
    text_page_operation.name = "text";
    text_page_operation.render = render_text_page;
    text_page_operation.handle_key = handle_text_key;
    text_page_operation.handle_touch = handle_text_touch;
    text_page_operation.periodic = NULL;
    text_page_operation.event_timeout = NULL;
    text_page_operation.next = NULL;

    audio_page_operation.page = BROWSER_PAGE_AUDIO;
    audio_page_operation.name = "audio";
    audio_page_operation.render = render_audio_page;
    audio_page_operation.handle_key = handle_audio_key;
    audio_page_operation.handle_touch = handle_audio_touch;
    audio_page_operation.periodic = audio_page_periodic;
    audio_page_operation.event_timeout = NULL;
    audio_page_operation.next = NULL;

    return page_manager_register(manager, &file_page_operation) < 0 ||
           page_manager_register(manager, &image_page_operation) < 0 ||
           page_manager_register(manager, &text_page_operation) < 0 ||
           page_manager_register(manager, &audio_page_operation) < 0 ? -1 : 0;
}

/**
 * @brief 查找指定页面 operation。
 * @param manager 页面管理器。
 * @param page 页面枚举。
 * @return 找到返回页面 operation，否则返回 NULL。
 */
const struct page_operation *page_manager_find(
    const struct page_manager *manager, enum browser_page page)
{
    struct page_operation *operation;

    if (manager == NULL) {
        return NULL;
    }
    for (operation = manager->head; operation != NULL;
         operation = operation->next) {
        if (operation->page == page) {
            return operation;
        }
    }
    return NULL;
}

/**
 * @brief 渲染当前页面。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_render(const struct page_manager *manager,
                        struct browser_app *app)
{
    const struct page_operation *operation = page_manager_find(manager,
                                                              app->page);

    if (operation == NULL || operation->render == NULL) {
        errno = ENOTSUP;
        return -1;
    }
    return operation->render(app);
}

/**
 * @brief 分发当前页面键盘动作。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int page_manager_handle_key(const struct page_manager *manager,
                            struct browser_app *app,
                            enum input_action action)
{
    const struct page_operation *operation = page_manager_find(manager,
                                                              app->page);

    if (operation == NULL || operation->handle_key == NULL) {
        return 0;
    }
    return operation->handle_key(app, action);
}

/**
 * @brief 分发当前页面触摸动作。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int page_manager_handle_touch(const struct page_manager *manager,
                              struct browser_app *app,
                              const struct browser_input *input)
{
    const struct page_operation *operation = page_manager_find(manager,
                                                              app->page);

    if (operation == NULL || operation->handle_touch == NULL) {
        return 0;
    }
    return operation->handle_touch(app, input);
}

/**
 * @brief 执行当前页面周期任务。
 * @param manager 页面管理器。
 * @param app 浏览器上下文。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功返回 0，失败返回 -1。
 */
int page_manager_periodic(const struct page_manager *manager,
                          struct browser_app *app, uint64_t now_ms)
{
    const struct page_operation *operation = page_manager_find(manager,
                                                              app->page);

    if (operation == NULL || operation->periodic == NULL) {
        return 0;
    }
    return operation->periodic(app, now_ms);
}

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
                               uint64_t now_ms, int current_timeout)
{
    const struct page_operation *operation = page_manager_find(manager,
                                                              app->page);

    if (operation == NULL || operation->event_timeout == NULL) {
        return current_timeout;
    }
    return operation->event_timeout(app, now_ms, current_timeout);
}
