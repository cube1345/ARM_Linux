#include "screen_power.h"

#include <errno.h>
#include <linux/fb.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

/** @brief 将 framebuffer 文件描述符转换为 operation context。 */
static void *framebuffer_context(int fd)
{
    return (void *)(intptr_t)(fd + 1);
}

/** @brief 从 operation context 还原 framebuffer 文件描述符。 */
static int framebuffer_fd(void *context)
{
    return (int)(intptr_t)context - 1;
}

/** @brief 通过 FBIOBLANK 设置 framebuffer 电源状态。 */
static int framebuffer_set_blank(void *context, int blank)
{
    int mode = blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK;

    return ioctl(framebuffer_fd(context), FBIOBLANK, mode);
}

/** @brief 使用指定 operation 初始化屏幕电源管理器。 */
void screen_power_manager_init(struct screen_power_manager *manager,
                               const struct screen_power_operation *operation,
                               uint64_t idle_timeout_ms, uint64_t now_ms)
{
    if (manager == NULL) return;
    memset(manager, 0, sizeof(*manager));
    if (operation != NULL) manager->operation = *operation;
    manager->idle_timeout_ms = idle_timeout_ms;
    manager->last_activity_ms = now_ms;
    manager->enabled = operation != NULL && operation->set_blank != NULL &&
                       idle_timeout_ms > 0;
}

/** @brief 使用 framebuffer FBIOBLANK 初始化屏幕电源管理器。 */
void screen_power_manager_init_framebuffer(
    struct screen_power_manager *manager, int framebuffer_fd_value,
    uint64_t idle_timeout_ms, uint64_t now_ms)
{
    struct screen_power_operation operation;

    operation.set_blank = framebuffer_set_blank;
    operation.context = framebuffer_context(framebuffer_fd_value);
    screen_power_manager_init(manager, &operation, idle_timeout_ms, now_ms);
    if (framebuffer_fd_value < 0 && manager != NULL) manager->enabled = 0;
}

/** @brief 禁用发生错误的 backend 并返回失败。 */
static int disable_screen_power(struct screen_power_manager *manager)
{
    manager->enabled = 0;
    manager->sleeping = 0;
    return -1;
}

/** @brief 记录用户活动并在需要时唤醒屏幕。 */
int screen_power_manager_activity(struct screen_power_manager *manager,
                                  uint64_t now_ms)
{
    if (manager == NULL) {
        errno = EINVAL;
        return -1;
    }
    manager->last_activity_ms = now_ms;
    if (!manager->enabled || !manager->sleeping) return 0;
    if (manager->operation.set_blank(manager->operation.context, 0) < 0) {
        return disable_screen_power(manager);
    }
    manager->sleeping = 0;
    return 1;
}

/** @brief 在达到空闲时限后关闭屏幕。 */
int screen_power_manager_update(struct screen_power_manager *manager,
                                uint64_t now_ms)
{
    if (manager == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!manager->enabled || manager->sleeping ||
        now_ms - manager->last_activity_ms < manager->idle_timeout_ms) {
        return 0;
    }
    if (manager->operation.set_blank(manager->operation.context, 1) < 0) {
        return disable_screen_power(manager);
    }
    manager->sleeping = 1;
    return 1;
}

/** @brief 销毁管理器，休眠时先尝试恢复显示。 */
void screen_power_manager_destroy(struct screen_power_manager *manager)
{
    if (manager == NULL) return;
    if (manager->enabled && manager->sleeping) {
        (void)manager->operation.set_blank(manager->operation.context, 0);
    }
    memset(manager, 0, sizeof(*manager));
}
