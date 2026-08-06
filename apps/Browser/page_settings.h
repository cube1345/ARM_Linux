#ifndef PAGE_SETTINGS_H
#define PAGE_SETTINGS_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制桌面设置页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_settings_page(struct browser_app *app);

/**
 * @brief 处理设置页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_settings_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理设置页面触摸动作。
 * @param app 浏览器上下文。
 * @param input 输入事件。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_settings_touch(struct browser_app *app,
                          const struct browser_input *input);

#endif
