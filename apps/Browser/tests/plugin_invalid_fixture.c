#include "plugin_manager.h"

/** @brief 返回一个不兼容的 ABI 版本。 */
uint32_t browser_plugin_abi_version(void)
{
    return BROWSER_PLUGIN_ABI_VERSION + 1U;
}

/** @brief 提供不应被 loader 调用的初始化入口。 */
int browser_plugin_init(const struct browser_plugin_host *host,
                        struct browser_plugin *plugin)
{
    (void)host;
    (void)plugin;
    return 0;
}
