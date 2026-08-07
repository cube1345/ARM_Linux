#ifndef BROWSER_CONFIG_H
#define BROWSER_CONFIG_H

#include "file_list.h"

#include <stddef.h>
#include <stdint.h>

/** @brief 浏览器可持久化的用户设置。 */
struct browser_config {
    uint32_t font_size;
    int volume;
    enum file_list_sort file_sort;
};

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
