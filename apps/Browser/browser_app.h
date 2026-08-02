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

#endif
