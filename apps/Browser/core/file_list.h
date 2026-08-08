#ifndef FILE_LIST_H
#define FILE_LIST_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

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
    FILE_TYPE_MP4,
    FILE_TYPE_MOV,
    FILE_TYPE_MKV,
    FILE_TYPE_AVI,
    FILE_TYPE_WEBM,
    FILE_TYPE_M4V,
    FILE_TYPE_AAC,
    FILE_TYPE_M4A,
    FILE_TYPE_FLAC,
    FILE_TYPE_OGG,
    FILE_TYPE_OPUS,
    FILE_TYPE_UNKNOWN
};

/** @brief 文件列表媒体类型过滤位。 */
enum file_list_filter {
    FILE_LIST_FILTER_NONE = 0U,
    FILE_LIST_FILTER_IMAGES = 1U << 0,
    FILE_LIST_FILTER_AUDIO = 1U << 1,
    FILE_LIST_FILTER_TEXT = 1U << 2,
    FILE_LIST_FILTER_VIDEO = 1U << 3,
    FILE_LIST_FILTER_AUDIO_VIDEO = FILE_LIST_FILTER_AUDIO |
                                   FILE_LIST_FILTER_VIDEO,
    FILE_LIST_FILTER_ALL = FILE_LIST_FILTER_IMAGES |
                           FILE_LIST_FILTER_AUDIO |
                           FILE_LIST_FILTER_TEXT |
                           FILE_LIST_FILTER_VIDEO
};

/** @brief 文件列表排序方式。 */
enum file_list_sort {
    FILE_LIST_SORT_NAME = 0,
    FILE_LIST_SORT_TYPE,
    FILE_LIST_SORT_TIME,
    FILE_LIST_SORT_SIZE
};

/** @brief 文件列表中的一项。 */
struct file_entry {
    char name[FILE_LIST_NAME_SIZE];
    enum file_type type;
    uint64_t size_bytes;
    time_t modified_time;
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
 * @brief 扫描目录并只保留指定媒体类型与子目录。
 *
 * @param directory 要扫描的目录。
 * @param list 输出文件列表。
 * @param filter 文件类型过滤位。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan_filtered(const char *directory, struct file_list *list,
                            unsigned int filter);

/**
 * @brief 递归扫描目录中的可浏览普通文件。
 * @param directory 起始目录。
 * @param list 输出文件列表，条目名称使用相对路径。
 * @param filter 文件类型过滤位。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan_recursive_filtered(const char *directory,
                                      struct file_list *list,
                                      unsigned int filter);

/**
 * @brief 按指定方式对已扫描的文件列表排序。
 * @param list 文件列表。
 * @param sort 排序方式，目录始终排在普通文件之前。
 */
void file_list_sort(struct file_list *list, enum file_list_sort sort);

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
 * @brief 根据文件名或路径后缀检测浏览器文件类型。
 * @param name 文件名或路径。
 * @return 浏览器文件类型。
 */
enum file_type file_list_detect_type(const char *name);

/**
 * @brief 获取文件类型的可读名称。
 *
 * @param type 文件类型。
 * @return 静态字符串。
 */
const char *file_type_name(enum file_type type);

#endif
