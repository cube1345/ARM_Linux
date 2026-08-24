#include "screen_power.h"

#include <stdio.h>
#include <stdlib.h>

/** @brief fake blank backend 状态。 */
struct fake_screen {
    int calls;
    int last_blank;
    int fail;
};

/** @brief 记录 blank/unblank 调用并支持注入失败。 */
static int fake_set_blank(void *context, int blank)
{
    struct fake_screen *screen = context;

    screen->calls++;
    screen->last_blank = blank;
    return screen->fail ? -1 : 0;
}

/** @brief 验证空闲休眠、输入唤醒、销毁恢复与 backend 降级。 */
int main(void)
{
    struct fake_screen screen = {0};
    struct screen_power_operation operation = {fake_set_blank, &screen};
    struct screen_power_manager manager;

    screen_power_manager_init(&manager, &operation, 100U, 1000U);
    if (screen_power_manager_update(&manager, 1099U) != 0 ||
        screen_power_manager_update(&manager, 1100U) != 1 ||
        screen.calls != 1 || screen.last_blank != 1 || !manager.sleeping ||
        screen_power_manager_update(&manager, 1200U) != 0 ||
        screen_power_manager_activity(&manager, 1200U) != 1 ||
        screen.calls != 2 || screen.last_blank != 0 || manager.sleeping ||
        screen_power_manager_activity(&manager, 1250U) != 0) {
        fprintf(stderr, "FAIL screen sleep/wake state\n");
        return EXIT_FAILURE;
    }
    if (screen_power_manager_update(&manager, 1350U) != 1) {
        fprintf(stderr, "FAIL second screen sleep\n");
        return EXIT_FAILURE;
    }
    screen_power_manager_destroy(&manager);
    if (screen.calls != 4 || screen.last_blank != 0) {
        fprintf(stderr, "FAIL destroy screen wake\n");
        return EXIT_FAILURE;
    }

    screen.calls = 0;
    screen.fail = 1;
    screen_power_manager_init(&manager, &operation, 10U, 0U);
    if (screen_power_manager_update(&manager, 10U) != -1 ||
        manager.enabled || manager.sleeping ||
        screen_power_manager_update(&manager, 20U) != 0) {
        fprintf(stderr, "FAIL screen backend fallback\n");
        return EXIT_FAILURE;
    }
    printf("PASS screen power\n");
    return EXIT_SUCCESS;
}
