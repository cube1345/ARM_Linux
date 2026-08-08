#ifndef BROWSER_APP_H
#define BROWSER_APP_H

#include "animation_decoder.h"
#include "audio_metadata.h"
#include "audio_player.h"
#include "browser_config.h"
#include "bmp_display.h"
#include "debug_manager.h"
#include "desktop_app.h"
#include "display_manager.h"
#include "file_list.h"
#include "font_manager.h"
#include "font_renderer.h"
#include "file_watcher.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_render.h"
#include "input_keyboard.h"
#include "media_player.h"
#include "subtitle.h"
#include "text_reader.h"

#include <limits.h>
#include <pthread.h>
#include <stdint.h>

/** @brief 默认字体像素高度。 */
#define BROWSER_FONT_DEFAULT_SIZE 24U

/** @brief 用户可调最小字体像素高度。 */
#define BROWSER_FONT_MIN_SIZE 18U

/** @brief 用户可调最大字体像素高度。 */
#define BROWSER_FONT_MAX_SIZE 34U
#define BROWSER_GALLERY_CACHE_COUNT 12U
#define BROWSER_SEARCH_QUERY_SIZE 64U

/** @brief 设置页每次调整的字体像素步长。 */
#define BROWSER_FONT_STEP_SIZE 2U

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
    BROWSER_PAGE_SETTINGS,
    BROWSER_PAGE_PLUGIN_BASE = 1000
};

/** @brief 文件页当前显示的数据来源。 */
enum browser_file_view {
    BROWSER_FILE_VIEW_DIRECTORY = 0,
    BROWSER_FILE_VIEW_RECENT,
    BROWSER_FILE_VIEW_FAVORITES
};

/** @brief 多媒体文件浏览器完整运行上下文。 */
struct browser_app {
    struct display_manager display_devices;
    struct font_manager fonts;
    struct debug_manager debug;
    struct bmp_display display;
    struct input_manager input;
    struct font_renderer font;
    struct file_list files;
    struct file_watcher watcher;
    struct text_reader text;
    struct audio_metadata audio_metadata;
    struct audio_player audio;
    struct media_player media;
    struct subtitle_track subtitles;
    struct image_data media_frame;
    struct browser_config config;
    char config_path[PATH_MAX];
    /** @brief 当前文件页排序方式。 */
    enum file_list_sort file_sort;
    enum browser_playback_mode playback_mode;
    /** @brief 文件页递归搜索状态。 */
    int search_active;
    char search_query[BROWSER_SEARCH_QUERY_SIZE];
    /** @brief Gallery 缩略图缓存槽。 */
    struct {
        char path[PATH_MAX];
        struct image_data image;
        uint64_t last_used;
    } gallery_cache[BROWSER_GALLERY_CACHE_COUNT];
    uint64_t gallery_cache_serial;
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
    enum browser_file_view file_view;
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
    enum image_render_mode video_render_mode;
    int video_fullscreen;
};

/**
 * @brief 判断文件类型是否为图片。
 * @param type 文件类型。
 * @return 是图片返回 1，否则返回 0。
 */
static inline int browser_file_type_is_image(enum file_type type)
{
    return type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG ||
           type == FILE_TYPE_PNG || type == FILE_TYPE_GIF ||
           type == FILE_TYPE_PLUGIN_IMAGE;
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
           type == FILE_TYPE_OPUS || type == FILE_TYPE_PLUGIN_AUDIO;
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
 * @brief 记录当前音频或 FFmpeg 媒体的断点位置。
 * @param app 浏览器上下文。
 */
void browser_app_remember_playback(struct browser_app *app);

/**
 * @brief 恢复指定音频或 FFmpeg 媒体的断点位置。
 * @param app 浏览器上下文。
 * @param path 媒体文件绝对路径。
 * @param page 目标播放器页面。
 */
void browser_app_restore_playback(struct browser_app *app, const char *path,
                                  enum browser_page page);

/**
 * @brief 同时设置传统音频和 FFmpeg 播放器的软件音量。
 * @param app 浏览器上下文。
 * @param volume 音量百分比，自动限制到 0 到 100。
 */
void browser_app_set_volume(struct browser_app *app, int volume);

/**
 * @brief 设置全局 UI 与文本阅读字体大小。
 * @param app 浏览器上下文。
 * @param pixel_size 字体像素高度，自动限制在允许范围。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_set_font_size(struct browser_app *app, uint32_t pixel_size);

/**
 * @brief 按步长调整全局 UI 与文本阅读字体大小。
 * @param app 浏览器上下文。
 * @param delta 像素高度增量。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_adjust_font_size(struct browser_app *app, int delta);

/**
 * @brief 保存当前运行状态与全部可持久化用户设置。
 * @param app 浏览器上下文。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_app_save_config(struct browser_app *app);

#endif
