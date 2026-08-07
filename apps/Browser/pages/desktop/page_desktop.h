#ifndef PAGE_DESKTOP_H
#define PAGE_DESKTOP_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制桌面应用启动器。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_desktop_page(struct browser_app *app);

/**
 * @brief 处理桌面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_desktop_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理桌面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_desktop_touch(struct browser_app *app,
                         const struct browser_input *input);

#endif
