#ifndef BROWSER_CONFIG_H
#define BROWSER_CONFIG_H

#include "browser_theme.h"
#include "file_list.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#define BROWSER_PATH_LIST_LIMIT 8U

/** @brief 可持久化的媒体路径列表。 */
struct browser_path_list {
    char paths[BROWSER_PATH_LIST_LIMIT][PATH_MAX];
    size_t count;
};

/** @brief 播放列表到达结尾后的处理方式。 */
enum browser_playback_mode {
    BROWSER_PLAYBACK_ONCE = 0,
    BROWSER_PLAYBACK_REPEAT_ONE,
    BROWSER_PLAYBACK_REPEAT_ALL,
    BROWSER_PLAYBACK_SHUFFLE
};

/** @brief 浏览器可持久化的用户设置。 */
struct browser_config {
    uint32_t font_size;
    int volume;
    enum file_list_sort file_sort;
    enum browser_playback_mode playback_mode;
    enum browser_theme ui_theme;
    char media_root[PATH_MAX];
    char keyboard_path[PATH_MAX];
    char touch_path[PATH_MAX];
    char resume_path[PATH_MAX];
    uint64_t resume_position_ms;
    struct browser_path_list recent_files;
    struct browser_path_list favorite_files;
};

/**
 * @brief 判断路径列表是否包含指定路径。
 * @param list 路径列表。
 * @param path 绝对路径。
 * @return 包含返回 1，否则返回 0。
 */
int browser_path_list_contains(const struct browser_path_list *list,
                               const char *path);

/**
 * @brief 追加路径到列表尾部，超出容量时忽略。
 * @param list 路径列表。
 * @param path 绝对路径。
 */
void browser_path_list_append(struct browser_path_list *list,
                              const char *path);

/**
 * @brief 将路径移动到列表最前方，超出容量时丢弃最旧项。
 * @param list 路径列表。
 * @param path 绝对路径。
 */
void browser_path_list_add_front(struct browser_path_list *list,
                                 const char *path);

/**
 * @brief 从路径列表移除指定路径。
 * @param list 路径列表。
 * @param path 绝对路径。
 * @return 删除了条目返回 1，否则返回 0。
 */
int browser_path_list_remove(struct browser_path_list *list,
                             const char *path);

/**
 * @brief 初始化默认设置。
 * @param config 输出设置。
 */
void browser_config_defaults(struct browser_config *config);

/**
 * @brief 从文本配置文件读取设置。
 * @param path 配置文件路径。
 * @param config 输出设置。
 * @return 成功或文件不存在返回 0，读取错误返回 -1。
 */
int browser_config_load(const char *path, struct browser_config *config);

/**
 * @brief 将设置原子写入文本配置文件。
 * @param path 配置文件路径。
 * @param config 要保存的设置。
 * @return 成功返回 0，失败返回 -1。
 */
int browser_config_save(const char *path,
                        const struct browser_config *config);

/**
 * @brief 从环境变量获取配置路径。
 * @param output 输出路径。
 * @param output_size 输出缓冲区大小。
 * @return 成功返回 0，路径过长返回 -1。
 */
int browser_config_path(char *output, size_t output_size);

#endif
