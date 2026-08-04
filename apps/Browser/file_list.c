#include "file_list.h"

#include "browser_log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/**
 * @brief 根据扩展名识别普通文件类型。
 *
 * @param name 文件名。
 * @return 识别出的文件类型。
 */
static enum file_type detect_file_type(const char *name)
{
    const char *extension = strrchr(name, '.');

    if (extension == NULL) {
        return FILE_TYPE_UNKNOWN;
    }
    if (strcasecmp(extension, ".bmp") == 0) {
        return FILE_TYPE_BMP;
    }
    if (strcasecmp(extension, ".jpg") == 0 ||
        strcasecmp(extension, ".jpeg") == 0) {
        return FILE_TYPE_JPEG;
    }
    if (strcasecmp(extension, ".png") == 0) {
        return FILE_TYPE_PNG;
    }
    if (strcasecmp(extension, ".gif") == 0) {
        return FILE_TYPE_GIF;
    }
    if (strcasecmp(extension, ".txt") == 0) {
        return FILE_TYPE_TEXT;
    }
    if (strcasecmp(extension, ".wav") == 0) {
        return FILE_TYPE_WAV;
    }
    if (strcasecmp(extension, ".mp3") == 0) {
        return FILE_TYPE_MP3;
    }
    return FILE_TYPE_UNKNOWN;
}

/**
 * @brief 拼接两个路径分量。
 *
 * @param directory 父目录。
 * @param name 子项名称。
 * @param output 输出路径。
 * @param output_size 输出缓冲区大小。
 * @return 成功返回 0，路径过长返回 -1。
 */
static int join_path(const char *directory, const char *name,
                     char *output, size_t output_size)
{
    int written;
    size_t length = strlen(directory);

    written = snprintf(output, output_size,
                       length > 0 && directory[length - 1] == '/' ?
                       "%s%s" : "%s/%s", directory, name);
    if (written < 0 || (size_t)written >= output_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

/**
 * @brief 比较文件条目，目录排在文件之前。
 *
 * @param left 左条目。
 * @param right 右条目。
 * @return 小于、等于或大于零。
 */
static int compare_entry(const void *left, const void *right)
{
    const struct file_entry *left_entry = left;
    const struct file_entry *right_entry = right;

    if (left_entry->type == FILE_TYPE_DIRECTORY &&
        right_entry->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    if (left_entry->type != FILE_TYPE_DIRECTORY &&
        right_entry->type == FILE_TYPE_DIRECTORY) {
        return 1;
    }
    return strcasecmp(left_entry->name, right_entry->name);
}

/**
 * @brief 扫描目录并按“目录优先、名称排序”生成文件列表。
 *
 * @param directory 要扫描的目录。
 * @param list 输出文件列表。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan(const char *directory, struct file_list *list)
{
    DIR *stream;

    if (directory == NULL || list == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(list, 0, sizeof(*list));
    if (realpath(directory, list->directory) == NULL) {
        browser_log_errno(BROWSER_LOG_ERROR, directory);
        return -1;
    }
    stream = opendir(list->directory);
    if (stream == NULL) {
        browser_log_errno(BROWSER_LOG_ERROR, list->directory);
        return -1;
    }

    while (list->count < FILE_LIST_MAX_COUNT) {
        struct dirent *entry;
        struct stat status;
        char path[PATH_MAX];
        enum file_type type;

        errno = 0;
        entry = readdir(stream);
        if (entry == NULL) {
            if (errno != 0) {
                browser_log_errno(BROWSER_LOG_ERROR, "readdir");
                closedir(stream);
                return -1;
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (join_path(list->directory, entry->d_name,
                      path, sizeof(path)) < 0 || lstat(path, &status) < 0) {
            continue;
        }
        if (S_ISDIR(status.st_mode)) {
            type = FILE_TYPE_DIRECTORY;
        } else if (S_ISREG(status.st_mode)) {
            type = detect_file_type(entry->d_name);
        } else {
            continue;
        }
        if (type == FILE_TYPE_UNKNOWN) {
            continue;
        }
        snprintf(list->entries[list->count].name,
                 sizeof(list->entries[list->count].name), "%s",
                 entry->d_name);
        list->entries[list->count].type = type;
        list->count++;
    }
    if (closedir(stream) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "closedir");
        return -1;
    }
    qsort(list->entries, list->count, sizeof(list->entries[0]), compare_entry);
    return 0;
}

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
                   char *output, size_t output_size)
{
    if (list == NULL || index >= list->count || output == NULL) {
        errno = EINVAL;
        return -1;
    }
    return join_path(list->directory, list->entries[index].name,
                     output, output_size);
}

/**
 * @brief 获取文件类型的可读名称。
 *
 * @param type 文件类型。
 * @return 静态字符串。
 */
const char *file_type_name(enum file_type type)
{
    switch (type) {
    case FILE_TYPE_DIRECTORY: return "DIR";
    case FILE_TYPE_BMP: return "BMP";
    case FILE_TYPE_JPEG: return "JPG";
    case FILE_TYPE_PNG: return "PNG";
    case FILE_TYPE_GIF: return "GIF";
    case FILE_TYPE_TEXT: return "TXT";
    case FILE_TYPE_WAV: return "WAV";
    case FILE_TYPE_MP3: return "MP3";
    case FILE_TYPE_UNKNOWN:
    default: return "?";
    }
}
