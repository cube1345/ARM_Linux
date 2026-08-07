#ifndef PAGE_TEXT_H
#define PAGE_TEXT_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制文本页并叠加返回按钮。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_text_page(struct browser_app *app);

/**
 * @brief 处理文本页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_text_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理文本页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_text_touch(struct browser_app *app,
                      const struct browser_input *input);

#endif
