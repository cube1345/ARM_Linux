#include "animation_decoder.h"

#include <errno.h>

/** @brief 内置 GIF 动画解码器对象。 */
static struct animation_decoder gif_decoder;

/**
 * @brief 判断 GIF 动画解码器是否支持指定类型。
 *
 * @param type 文件类型。
 * @return 支持返回 1，否则返回 0。
 */
static int supports_gif(enum file_type type)
{
    return type == FILE_TYPE_GIF;
}

/**
 * @brief 初始化动画解码器管理器。
 *
 * @param manager 动画解码器管理器。
 */
void animation_decoder_manager_init(struct animation_decoder_manager *manager)
{
    if (manager != NULL) {
        manager->head = NULL;
    }
}

/**
 * @brief 注册一个动画解码器。
 *
 * @param manager 动画解码器管理器。
 * @param decoder 待注册解码器，生命周期必须长于 manager。
 * @return 成功返回 0，失败返回 -1。
 */
int animation_decoder_register(struct animation_decoder_manager *manager,
                               struct animation_decoder *decoder)
{
    if (manager == NULL || decoder == NULL || decoder->name == NULL ||
        decoder->supports == NULL || decoder->open == NULL) {
        errno = EINVAL;
        return -1;
    }
    decoder->next = manager->head;
    manager->head = decoder;
    return 0;
}

/**
 * @brief 注册内置 GIF 动画解码器。
 *
 * @param manager 动画解码器管理器。
 * @return 成功返回 0，失败返回 -1。
 */
int animation_decoder_register_builtin(
    struct animation_decoder_manager *manager)
{
    gif_decoder.name = "gif";
    gif_decoder.supports = supports_gif;
    gif_decoder.open = gif_animation_open;
    gif_decoder.next = NULL;
    return animation_decoder_register(manager, &gif_decoder);
}

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
                                   struct gif_animation *animation)
{
    struct animation_decoder *decoder;

    if (manager == NULL || path == NULL || animation == NULL) {
        errno = EINVAL;
        return -1;
    }
    for (decoder = manager->head; decoder != NULL; decoder = decoder->next) {
        if (decoder->supports(type)) {
            return decoder->open(animation, path);
        }
    }
    errno = ENOTSUP;
    return -1;
}
