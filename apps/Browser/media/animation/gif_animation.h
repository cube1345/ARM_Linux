#ifndef GIF_ANIMATION_H
#define GIF_ANIMATION_H

#include "image_data.h"

#include <stddef.h>
#include <stdint.h>

/** @brief 一个已经完成画布合成的 GIF 帧。 */
struct gif_frame {
    struct image_data image;
    unsigned int delay_ms;
};

/** @brief GIF 动画及其运行时播放状态。 */
struct gif_animation {
    struct gif_frame *frames;
    size_t frame_count;
    size_t current;
    unsigned int loop_count;
    unsigned int completed_loops;
    uint64_t next_frame_ms;
    int finished;
};

/**
 * @brief 解码 GIF 并预合成全部动画帧。
 * @param animation 输出动画，调用前必须清零。
 * @param path GIF 文件路径。
 * @return 成功返回 0，失败返回 -1。
 */
int gif_animation_open(struct gif_animation *animation, const char *path);

/**
 * @brief 从第一帧开始计时播放。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 */
void gif_animation_reset(struct gif_animation *animation, uint64_t now_ms);

/**
 * @brief 按当前时间推进动画。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 * @return 帧发生变化返回 1，否则返回 0。
 */
int gif_animation_advance(struct gif_animation *animation, uint64_t now_ms);

/**
 * @brief 获取当前显示帧。
 * @param animation 已打开的动画。
 * @return 当前帧图片，无帧返回 NULL。
 */
const struct image_data *gif_animation_current(
    const struct gif_animation *animation);

/**
 * @brief 计算下一帧之前可等待的时间。
 * @param animation 已打开的动画。
 * @param now_ms 当前单调时钟毫秒值。
 * @param maximum_ms 调用者允许的最长等待时间。
 * @return 等待毫秒数。
 */
int gif_animation_timeout(const struct gif_animation *animation,
                          uint64_t now_ms, int maximum_ms);

/**
 * @brief 释放 GIF 动画全部帧。
 * @param animation 动画对象。
 */
void gif_animation_close(struct gif_animation *animation);

#endif
