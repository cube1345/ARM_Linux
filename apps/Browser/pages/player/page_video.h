#ifndef PAGE_VIDEO_H
#define PAGE_VIDEO_H

#include "browser_app.h"
#include "input_keyboard.h"

/** @brief 绘制 FFmpeg 视频/通用媒体页面。 */
int render_video_page(struct browser_app *app);

/** @brief 处理 FFmpeg 媒体页面键盘动作。 */
int handle_video_key(struct browser_app *app, enum input_action action);

/** @brief 处理 FFmpeg 媒体页面触摸动作。 */
int handle_video_touch(struct browser_app *app,
                       const struct browser_input *input);

/** @brief 更新 FFmpeg 媒体页面帧和进度。 */
int update_video_page(struct browser_app *app, uint64_t now_ms);

#endif
