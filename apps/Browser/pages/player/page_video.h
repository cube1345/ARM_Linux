#ifndef PAGE_VIDEO_H
#define PAGE_VIDEO_H

#include "browser_app.h"
#include "input_keyboard.h"

/** @brief 绘制 FFmpeg 视频/通用媒体页面。 */
int render_video_page(struct browser_app *app);

/** @brief 处理 FFmpeg 媒体页面键盘动作。 */
int handle_video_key(struct browser_app *app, enum input_action action);

/**
 * @brief 打开当前媒体列表中的下一项。
 * @param app 浏览器上下文。
 * @return 成功返回 0，无下一项或失败返回 -1。
 */
int media_play_next(struct browser_app *app);

/**
 * @brief 打开当前媒体列表中的上一项。
 * @param app 浏览器上下文。
 * @return 成功返回 0，无上一项或失败返回 -1。
 */
int media_play_previous(struct browser_app *app);

/**
 * @brief 根据播放模式处理当前媒体自然结束。
 * @param app 浏览器上下文。
 * @return 已切换或重播返回 0，无动作或失败返回 -1。
 */
int media_handle_completion(struct browser_app *app);

/** @brief 处理 FFmpeg 媒体页面触摸动作。 */
int handle_video_touch(struct browser_app *app,
                       const struct browser_input *input);

/** @brief 更新 FFmpeg 媒体页面帧和进度。 */
int update_video_page(struct browser_app *app, uint64_t now_ms);

#endif
