#ifndef PAGE_AUDIO_H
#define PAGE_AUDIO_H

#include "browser_app.h"
#include "input_keyboard.h"

/**
 * @brief 绘制音频播放页面、进度条和音量条。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int render_audio_page(struct browser_app *app);

/**
 * @brief 按当前音频位置相对跳转。
 * @param app 浏览器上下文。
 * @param delta_percent 百分比增量。
 */
void seek_relative(struct browser_app *app, int delta_percent);

/**
 * @brief 处理音频页面键盘动作。
 * @param app 浏览器上下文。
 * @param action 输入动作。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_audio_key(struct browser_app *app, enum input_action action);

/**
 * @brief 打开当前播放列表中的下一首 WAV/MP3。
 * @param app 浏览器上下文。
 * @return 成功返回 0，无下一首或失败返回 -1。
 */
int audio_play_next(struct browser_app *app);

/**
 * @brief 打开当前播放列表中的上一首 WAV/MP3。
 * @param app 浏览器上下文。
 * @return 成功返回 0，无上一首或失败返回 -1。
 */
int audio_play_previous(struct browser_app *app);

/**
 * @brief 根据播放模式处理当前音频自然结束。
 * @param app 浏览器上下文。
 * @return 已切换或重播返回 0，无动作或失败返回 -1。
 */
int audio_handle_completion(struct browser_app *app);

/**
 * @brief 处理音频页面触摸手势。
 * @param app 浏览器上下文。
 * @param input 触摸输入。
 * @return 继续返回 0，失败返回 -1。
 */
int handle_audio_touch(struct browser_app *app,
                       const struct browser_input *input);

#endif
