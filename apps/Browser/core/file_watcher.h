#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <limits.h>

/** @brief 当前媒体目录的 inotify 监听器。 */
struct file_watcher {
    int fd;
    int watch_descriptor;
    char directory[PATH_MAX];
};

/**
 * @brief 初始化 nonblocking inotify 文件监听器。
 * @param watcher 输出监听器。
 * @return 成功返回 0，系统不支持或资源不足返回 -1。
 */
int file_watcher_init(struct file_watcher *watcher);

/**
 * @brief 将监听器切换到指定目录。
 * @param watcher 已初始化监听器。
 * @param directory 要监听的目录。
 * @return 成功返回 0，目录不可监听或路径过长返回 -1。
 */
int file_watcher_update(struct file_watcher *watcher, const char *directory);

/**
 * @brief 消费当前已就绪的目录变化事件。
 * @param watcher 已初始化监听器。
 * @return 检测到变化返回 1，无变化返回 0，读取失败返回 -1。
 */
int file_watcher_consume(struct file_watcher *watcher);

/**
 * @brief 移除目录监听并释放内核文件描述符。
 * @param watcher 监听器，可为 NULL。
 */
void file_watcher_destroy(struct file_watcher *watcher);

#endif
