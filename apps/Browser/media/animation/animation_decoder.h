#ifndef ANIMATION_DECODER_H
#define ANIMATION_DECODER_H

#include "file_list.h"
#include "gif_animation.h"

/** @brief 动画图片解码器操作接口。 */
struct animation_decoder {
    const char *name;
    int (*supports)(enum file_type type);
    int (*open)(struct gif_animation *animation, const char *path);
    struct animation_decoder *next;
};

/** @brief 动画图片解码器注册管理器。 */
struct animation_decoder_manager {
    struct animation_decoder *head;
};

/**
 * @brief 初始化动画解码器管理器。
 *
 * @param manager 动画解码器管理器。
 */
void animation_decoder_manager_init(struct animation_decoder_manager *manager);

/**
 * @brief 注册一个动画解码器。
 *
 * @param manager 动画解码器管理器。
 * @param decoder 待注册解码器，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int animation_decoder_register(struct animation_decoder_manager *manager,
                               struct animation_decoder *decoder);

/**
 * @brief 注册内置 GIF 动画解码器。
 *
 * @param manager 动画解码器管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int animation_decoder_register_builtin(
    struct animation_decoder_manager *manager);

/**
 * @brief 通过指定管理器打开动画图片。
 *
 * @param manager 动画解码器管理器。
 * @param path 动画图片路径。
 * @param type 文件类型。
 * @param animation 输出动画对象，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int animation_decoder_manager_open(struct animation_decoder_manager *manager,
                                   const char *path, enum file_type type,
                                   struct gif_animation *animation);

#endif
