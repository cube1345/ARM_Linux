#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#include <stdint.h>

/** @brief 屏幕 blank/unblank operation。 */
struct screen_power_operation {
    int (*set_blank)(void *context, int blank);
    void *context;
};

/** @brief 屏幕空闲休眠状态管理器。 */
struct screen_power_manager {
    struct screen_power_operation operation;
    uint64_t idle_timeout_ms;
    uint64_t last_activity_ms;
    int sleeping;
    int enabled;
};

/**
 * @brief 使用指定 operation 初始化屏幕电源管理器。
 * @param manager 输出管理器。
 * @param operation blank/unblank operation。
 * @param idle_timeout_ms 空闲休眠毫秒数，0 禁用。
 * @param now_ms 当前单调时钟毫秒值。
 */
void screen_power_manager_init(struct screen_power_manager *manager,
                               const struct screen_power_operation *operation,
                               uint64_t idle_timeout_ms, uint64_t now_ms);

/** @brief 使用 framebuffer FBIOBLANK 初始化屏幕电源管理器。 */
void screen_power_manager_init_framebuffer(
    struct screen_power_manager *manager, int framebuffer_fd,
    uint64_t idle_timeout_ms, uint64_t now_ms);

/**
 * @brief 记录用户活动并在需要时唤醒屏幕。
 * @return 已唤醒返回 1，无需唤醒返回 0，backend 失败返回 -1。
 */
int screen_power_manager_activity(struct screen_power_manager *manager,
                                  uint64_t now_ms);

/**
 * @brief 在达到空闲时限后关闭屏幕。
 * @return 已进入休眠返回 1，无变化返回 0，backend 失败返回 -1。
 */
int screen_power_manager_update(struct screen_power_manager *manager,
                                uint64_t now_ms);

/** @brief 销毁管理器，休眠时先尝试恢复显示。 */
void screen_power_manager_destroy(struct screen_power_manager *manager);

#endif
