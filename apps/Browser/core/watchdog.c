#include "watchdog.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/watchdog.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define WATCHDOG_DEFAULT_DEVICE "/dev/watchdog"
#define WATCHDOG_DEFAULT_TIMEOUT_SECONDS 15U

/** @brief 将 watchdog 文件描述符编码为 operation context。 */
static void *watchdog_context(int fd)
{
    return (void *)(intptr_t)(fd + 1);
}

/** @brief 从 operation context 还原 watchdog 文件描述符。 */
static int watchdog_fd(void *context)
{
    return (int)(intptr_t)context - 1;
}

/** @brief 通过 WDIOC_KEEPALIVE 喂狗。 */
static int watchdog_keepalive(void *context)
{
    int dummy = 0;

    return ioctl(watchdog_fd(context), WDIOC_KEEPALIVE, &dummy);
}

/** @brief 写入 magic close 并关闭 watchdog。 */
static int watchdog_disarm(void *context)
{
    int fd = watchdog_fd(context);
    const char magic = 'V';
    ssize_t written = write(fd, &magic, sizeof(magic));
    int result = written == (ssize_t)sizeof(magic) ? 0 : -1;

    if (close(fd) < 0) result = -1;
    return result;
}

/** @brief 使用指定 operation 初始化 watchdog。 */
void watchdog_manager_init(struct watchdog_manager *manager,
                           const struct watchdog_operation *operation,
                           unsigned int timeout_seconds, uint64_t now_ms)
{
    if (manager == NULL) return;
    memset(manager, 0, sizeof(*manager));
    if (operation != NULL) manager->operation = *operation;
    manager->keepalive_interval_ms = timeout_seconds > 1U ?
        (uint64_t)timeout_seconds * 500U : 500U;
    manager->next_keepalive_ms = now_ms + manager->keepalive_interval_ms;
    manager->enabled = operation != NULL && operation->keepalive != NULL &&
                       operation->disarm != NULL && timeout_seconds > 0U;
}

/** @brief 打开 Linux watchdog 设备并设置超时。 */
int watchdog_manager_open(struct watchdog_manager *manager,
                          const char *device, unsigned int timeout_seconds,
                          uint64_t now_ms)
{
    struct watchdog_operation operation;
    const char *path = device;
    int fd;
    int requested;

    if (manager == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (path == NULL || path[0] == '\0') path = WATCHDOG_DEFAULT_DEVICE;
    if (strcmp(path, "-") == 0 || timeout_seconds == 0U) {
        watchdog_manager_init(manager, NULL, 0, now_ms);
        return 0;
    }
    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) {
        watchdog_manager_init(manager, NULL, 0, now_ms);
        return 0;
    }
    if (fd < 0) return -1;
    requested = (int)timeout_seconds;
    if (ioctl(fd, WDIOC_SETTIMEOUT, &requested) < 0) {
        (void)close(fd);
        return -1;
    }
    operation.keepalive = watchdog_keepalive;
    operation.disarm = watchdog_disarm;
    operation.context = watchdog_context(fd);
    watchdog_manager_init(manager, &operation, (unsigned int)requested,
                           now_ms);
    return 0;
}

/** @brief 按计划发送 keepalive。 */
int watchdog_manager_update(struct watchdog_manager *manager,
                            uint64_t now_ms)
{
    if (manager == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!manager->enabled || now_ms < manager->next_keepalive_ms) return 0;
    if (manager->operation.keepalive(manager->operation.context) < 0) {
        manager->enabled = 0;
        (void)manager->operation.disarm(manager->operation.context);
        return -1;
    }
    manager->next_keepalive_ms = now_ms + manager->keepalive_interval_ms;
    return 0;
}

/** @brief 正常退出时 disarm 并释放 watchdog。 */
void watchdog_manager_destroy(struct watchdog_manager *manager)
{
    if (manager == NULL) return;
    if (manager->enabled) (void)manager->operation.disarm(manager->operation.context);
    memset(manager, 0, sizeof(*manager));
}
