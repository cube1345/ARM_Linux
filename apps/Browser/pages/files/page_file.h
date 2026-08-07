#ifndef PAGE_FILE_H
#define PAGE_FILE_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制文件列表页面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_file_page(struct browser_app *app);

/**
 * @brief 打开当前选择的目录或媒体。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int open_selected(struct browser_app *app);

/**
 * @brief 返回父目录但不越过启动根目录。
 * @param app 浏览器上下文。
 * @return 已进入父目录返回 1，已在根目录返回 0，失败返回 -1。
 */
int enter_parent(struct browser_app *app);

/**
 * @brief 处理文件列表键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int handle_file_key(struct browser_app *app, enum input_action action);

/**
 * @brief 处理文件列表触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，退出返回 1，失败返回 -1。
 */
int handle_file_touch(struct browser_app *app,
                      const struct browser_input *input);

#endif
