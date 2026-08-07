#ifndef BROWSER_APP_H
#define BROWSER_APP_H

#include "animation_decoder.h"
#include "audio_player.h"
#include "media_player.h"
#include "bmp_display.h"
#include "desktop_app.h"
#include "file_list.h"
#include "font_renderer.h"
#include "gif_animation.h"
#include "image_data.h"
#include "input_keyboard.h"
#include "text_reader.h"

#include <limits.h>
#include <pthread.h>
#include <stdint.h>

/** @brief 浏览器当前页面。 */
enum browser_page {
    BROWSER_PAGE_DESKTOP = 0,
    BROWSER_PAGE_FILES,
    BROWSER_PAGE_IMAGE,
    BROWSER_PAGE_TEXT,
    BROWSER_PAGE_AUDIO,
    BROWSER_PAGE_VIDEO,
    BROWSER_PAGE_DIAGNOSTICS,
    BROWSER_PAGE_TOOLS,
    BROWSER_PAGE_SETTINGS
};

/** @brief 多媒体文件浏览器完整运行上下文。 */
struct browser_app {
    struct bmp_display display;
    struct input_manager input;
    struct font_renderer font;
    struct file_list files;
    struct text_reader text;
    struct audio_player audio;
    struct media_player media;
    struct image_data media_frame;
    struct animation_decoder_manager animations;
    struct image_data image;
    struct image_data preloaded_image;
    struct gif_animation gif;
    struct desktop_app_manager desktop_apps;
    char root[PATH_MAX];
    char current_path[PATH_MAX];
    char preloaded_path[PATH_MAX];
    const char *alsa_device;
    size_t selected;
    size_t preloaded_index;
    enum file_type preloaded_type;
    enum browser_page page;
    enum desktop_app_id active_app;
    unsigned int file_filter;
    size_t desktop_selected;
    size_t tool_selected;
    char tool_output[2048];
    char tool_status[128];
    unsigned int rotation;
    int slideshow_enabled;
    int preload_thread_created;
    int preload_ready;
    int preload_result;
    pthread_t preload_thread;
    uint64_t next_slideshow_ms;
    uint64_t last_audio_refresh_ms;
    uint64_t last_media_refresh_ms;
    uint64_t media_frame_serial;
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
    return type == FILE_TYPE_WAV || type == FILE_TYPE_MP3 ||
           type == FILE_TYPE_AAC || type == FILE_TYPE_M4A ||
           type == FILE_TYPE_FLAC || type == FILE_TYPE_OGG ||
           type == FILE_TYPE_OPUS;
}

/**
 * @brief 判断文件类型是否为视频。
 * @param type 文件类型。
 * @return 是视频返回 1，否则返回 0。
 */
static inline int browser_file_type_is_video(enum file_type type)
{
    return type == FILE_TYPE_MP4 || type == FILE_TYPE_MOV ||
           type == FILE_TYPE_MKV || type == FILE_TYPE_AVI ||
           type == FILE_TYPE_WEBM || type == FILE_TYPE_M4V;
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

/**
 * @brief 关闭当前应用并返回桌面。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_return_to_desktop(struct browser_app *app);

/**
 * @brief 同时设置传统音频和 FFmpeg 播放器的软件音量。
 * @param app 浏览器上下文。
 * @param volume 音量百分比，自动限制到 0 到 100。
 */
void browser_app_set_volume(struct browser_app *app, int volume);

#endif
