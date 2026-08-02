#ifndef PAGE_IMAGE_H
#define PAGE_IMAGE_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 释放当前图片或 GIF 资源。
 * @param app 浏览器上下文。
 */
void close_image(struct browser_app *app);

/**
 * @brief 解码当前选择的普通图片或 GIF。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int load_selected_image(struct browser_app *app);

/**
 * @brief 绘制当前普通图片或 GIF 帧及图片工具按钮。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_image_page(struct browser_app *app);

/**
 * @brief 在当前目录选择相邻图片并加载资源。
 * @param app 浏览器上下文。
 * @param direction 正数向后，负数向前。
 * @return 找到并加载返回 1，无图片返回 0，失败返回 -1。
 */
int select_adjacent_image(struct browser_app *app, int direction);

/**
 * @brief 处理图片页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_image_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理图片页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_image_touch(struct browser_app *app,
                       const struct browser_input *input);

#endif
