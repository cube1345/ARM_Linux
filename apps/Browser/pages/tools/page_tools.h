#ifndef PAGE_TOOLS_H
#define PAGE_TOOLS_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制外部 Linux 工具启动页。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_tools_page(struct browser_app *app);

/**
 * @brief 处理外部 Linux 工具页键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_tools_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理外部 Linux 工具页触摸动作。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_tools_touch(struct browser_app *app,
                       const struct browser_input *input);

#endif
