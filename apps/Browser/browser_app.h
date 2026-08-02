#ifndef BROWSER_APP_H
#define BROWSER_APP_H

#include "animation_decoder.h"
#include "audio_player.h"
#include "bmp_display.h"
#include "file_list.h"
#include "font_renderer.h"
#include "gif_animation.h"
#include "image_data.h"
#include "input_keyboard.h"
#include "text_reader.h"

#include <limits.h>
#include <stdint.h>

/** @brief 浏览器当前页面。 */
enum browser_page {
    BROWSER_PAGE_FILES = 0,
    BROWSER_PAGE_IMAGE,
    BROWSER_PAGE_TEXT,
    BROWSER_PAGE_AUDIO
};

/** @brief 多媒体文件浏览器完整运行上下文。 */
struct browser_app {
    struct bmp_display display;
    struct input_manager input;
    struct font_renderer font;
    struct file_list files;
    struct text_reader text;
    struct audio_player audio;
    struct animation_decoder_manager animations;
    struct image_data image;
    struct gif_animation gif;
    char root[PATH_MAX];
    char current_path[PATH_MAX];
    const char *alsa_device;
    size_t selected;
    enum browser_page page;
    unsigned int rotation;
    uint64_t last_audio_refresh_ms;
};

/**
 * @brief 判断文件类型是否为图片。
 * @param type 文件类型。
 * @return 是图片返回 1，否则返回 0。
 */
static inline int browser_file_type_is_image(enum file_type type)
{
    return type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG ||
           type == FILE_TYPE_PNG || type == FILE_TYPE_GIF;
}

/**
 * @brief 判断文件类型是否为音频。
 * @param type 文件类型。
 * @return 是音频返回 1，否则返回 0。
 */
static inline int browser_file_type_is_audio(enum file_type type)
{
    return type == FILE_TYPE_WAV || type == FILE_TYPE_MP3;
}

/**
 * @brief 获取单调时钟毫秒值。
 * @return 单调时钟毫秒值。
 */
uint64_t monotonic_ms(void);

/**
 * @brief 关闭当前媒体资源并返回文件列表页。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_close_media_page(struct browser_app *app);

#endif
