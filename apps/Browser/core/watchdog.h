#ifndef BROWSER_WATCHDOG_H
#define BROWSER_WATCHDOG_H

#include <stdint.h>

/** @brief watchdog keepalive/disarm operation。 */
struct watchdog_operation {
    int (*keepalive)(void *context);
    int (*disarm)(void *context);
    void *context;
};

/** @brief 可选硬件 watchdog 管理器。 */
struct watchdog_manager {
    struct watchdog_operation operation;
    uint64_t keepalive_interval_ms;
    uint64_t next_keepalive_ms;
    int enabled;
};

/**
 * @brief 使用指定 operation 初始化 watchdog。
 * @param manager 输出管理器。
 * @param operation keepalive/disarm operation。
 * @param timeout_seconds watchdog 超时秒数。
 * @param now_ms 当前单调时钟毫秒值。
 */
void watchdog_manager_init(struct watchdog_manager *manager,
                           const struct watchdog_operation *operation,
                           unsigned int timeout_seconds, uint64_t now_ms);

/**
 * @brief 打开 Linux watchdog 设备并设置超时。
 * @param manager 输出管理器。
 * @param device 设备路径，NULL/空字符串使用 `/dev/watchdog`，`-` 禁用。
 * @param timeout_seconds 请求超时秒数。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 成功或设备不存在并降级返回 0，打开/配置失败返回 -1。
 */
int watchdog_manager_open(struct watchdog_manager *manager,
                          const char *device, unsigned int timeout_seconds,
                          uint64_t now_ms);

/**
 * @brief 按计划发送 keepalive。
 * @return 发送成功或无需发送返回 0，backend 失败返回 -1。
 */
int watchdog_manager_update(struct watchdog_manager *manager,
                            uint64_t now_ms);

/** @brief 正常退出时 disarm 并释放 watchdog。 */
void watchdog_manager_destroy(struct watchdog_manager *manager);

#endif
