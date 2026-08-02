#ifndef FILE_LIST_H
#define FILE_LIST_H

#include <limits.h>
#include <stddef.h>

#define FILE_LIST_MAX_COUNT 256
#define FILE_LIST_NAME_SIZE 256

/** @brief 浏览器支持的文件类型。 */
enum file_type {
    FILE_TYPE_DIRECTORY = 0,
    FILE_TYPE_BMP,
    FILE_TYPE_JPEG,
    FILE_TYPE_PNG,
    FILE_TYPE_GIF,
    FILE_TYPE_TEXT,
    FILE_TYPE_WAV,
    FILE_TYPE_MP3,
    FILE_TYPE_UNKNOWN
};

/** @brief 文件列表中的一项。 */
struct file_entry {
    char name[FILE_LIST_NAME_SIZE];
    enum file_type type;
};

/** @brief 当前目录及其可浏览条目。 */
struct file_list {
    char directory[PATH_MAX];
    struct file_entry entries[FILE_LIST_MAX_COUNT];
    size_t count;
};

/**
 * @brief 扫描目录并按“目录优先、名称排序”生成文件列表。
 *
 * @param directory 要扫描的目录。
 * @param list 输出文件列表。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan(const char *directory, struct file_list *list);

/**
 * @brief 拼接当前目录与指定条目名称。
 *
 * @param list 文件列表。
 * @param index 条目索引。
 * @param output 输出路径。
 * @param output_size 输出缓冲区大小。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_path(const struct file_list *list, size_t index,
                   char *output, size_t output_size);

/**
 * @brief 获取文件类型的可读名称。
 *
 * @param type 文件类型。
 * @return 静态字符串。
 */
const char *file_type_name(enum file_type type);

#endif
