#include "watchdog.h"

#include <stdio.h>
#include <stdlib.h>

/** @brief fake watchdog 状态。 */
struct fake_watchdog {
    int keepalive_calls;
    int disarm_calls;
    int fail_keepalive;
};

/** @brief 记录 fake keepalive 调用并支持失败注入。 */
static int fake_keepalive(void *context)
{
    struct fake_watchdog *watchdog = context;

    watchdog->keepalive_calls++;
    return watchdog->fail_keepalive ? -1 : 0;
}

/** @brief 记录 fake disarm 调用。 */
static int fake_disarm(void *context)
{
    struct fake_watchdog *watchdog = context;

    watchdog->disarm_calls++;
    return 0;
}

/** @brief 验证 keepalive 周期、失败降级、禁用和正常 disarm。 */
int main(void)
{
    struct fake_watchdog fake = {0};
    struct watchdog_operation operation = {
        fake_keepalive, fake_disarm, &fake
    };
    struct watchdog_manager manager;

    watchdog_manager_init(&manager, &operation, 10U, 100U);
    if (watchdog_manager_update(&manager, 5099U) != 0 ||
        watchdog_manager_update(&manager, 5100U) != 0 ||
        fake.keepalive_calls != 1 ||
        watchdog_manager_update(&manager, 10099U) != 0 ||
        watchdog_manager_update(&manager, 10100U) != 0 ||
        fake.keepalive_calls != 2) {
        fprintf(stderr, "FAIL watchdog schedule\n");
        return EXIT_FAILURE;
    }
    watchdog_manager_destroy(&manager);
    if (fake.disarm_calls != 1) {
        fprintf(stderr, "FAIL watchdog disarm\n");
        return EXIT_FAILURE;
    }

    fake.fail_keepalive = 1;
    watchdog_manager_init(&manager, &operation, 2U, 0U);
    if (watchdog_manager_update(&manager, 1000U) != -1 || manager.enabled ||
        fake.keepalive_calls != 3 || fake.disarm_calls != 2 ||
        watchdog_manager_update(&manager, 2000U) != 0) {
        fprintf(stderr, "FAIL watchdog fallback\n");
        return EXIT_FAILURE;
    }
    printf("PASS watchdog\n");
    return EXIT_SUCCESS;
}
