#ifndef IMAGE_LIST_H
#define IMAGE_LIST_H

#include <stddef.h>

#define IMAGE_LIST_MAX_COUNT 128
#define IMAGE_LIST_MAX_PATH 512

/**
 * @brief BMP 图片路径列表。
 */
struct image_list {
    char paths[IMAGE_LIST_MAX_COUNT][IMAGE_LIST_MAX_PATH];
    size_t count;
};

/**
 * @brief 扫描并排序目录中的 BMP 普通文件。
 *
 * @param directory 要扫描的目录路径。
 * @param list 输出的图片列表。
 * @return 成功返回 0，失败返回 -1。
 */
int image_list_scan(const char *directory, struct image_list *list);

#endif
