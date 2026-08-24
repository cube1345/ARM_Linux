#include "file_list.h"

#include "browser_log.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define FILE_EXTENSION_MAX_COUNT 32U
#define FILE_EXTENSION_MAX_LENGTH 15U

struct file_extension_registration {
    char extension[FILE_EXTENSION_MAX_LENGTH + 1U];
    enum file_type type;
};

static struct file_extension_registration registered_extensions[
    FILE_EXTENSION_MAX_COUNT];
static size_t registered_extension_count;

/**
 * @brief 根据扩展名识别普通文件类型。
 *
 * @param name 文件名。
 * @return 识别出的文件类型。
 */
static enum file_type file_list_detect_builtin_type(const char *name)
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
    if (strcasecmp(extension, ".mp4") == 0) {
        return FILE_TYPE_MP4;
    }
    if (strcasecmp(extension, ".mov") == 0) {
        return FILE_TYPE_MOV;
    }
    if (strcasecmp(extension, ".mkv") == 0) {
        return FILE_TYPE_MKV;
    }
    if (strcasecmp(extension, ".avi") == 0) {
        return FILE_TYPE_AVI;
    }
    if (strcasecmp(extension, ".webm") == 0) {
        return FILE_TYPE_WEBM;
    }
    if (strcasecmp(extension, ".m4v") == 0) {
        return FILE_TYPE_M4V;
    }
    if (strcasecmp(extension, ".aac") == 0) {
        return FILE_TYPE_AAC;
    }
    if (strcasecmp(extension, ".m4a") == 0) {
        return FILE_TYPE_M4A;
    }
    if (strcasecmp(extension, ".flac") == 0) {
        return FILE_TYPE_FLAC;
    }
    if (strcasecmp(extension, ".ogg") == 0) {
        return FILE_TYPE_OGG;
    }
    if (strcasecmp(extension, ".opus") == 0) {
        return FILE_TYPE_OPUS;
    }
    return FILE_TYPE_UNKNOWN;
}

/** @brief 根据内置表和插件注册表识别文件类型。 */
enum file_type file_list_detect_type(const char *name)
{
    enum file_type type = file_list_detect_builtin_type(name);
    const char *extension;
    size_t index;

    if (type != FILE_TYPE_UNKNOWN || name == NULL) {
        return type;
    }
    extension = strrchr(name, '.');
    if (extension == NULL) {
        return FILE_TYPE_UNKNOWN;
    }
    for (index = 0; index < registered_extension_count; index++) {
        if (strcasecmp(extension,
                       registered_extensions[index].extension) == 0) {
            return registered_extensions[index].type;
        }
    }
    return FILE_TYPE_UNKNOWN;
}

/** @brief 注册插件媒体扩展名。 */
int file_list_register_extension(const char *extension, enum file_type type)
{
    size_t length;
    size_t index;

    if (extension == NULL || extension[0] != '.' ||
        (type != FILE_TYPE_PLUGIN_IMAGE &&
         type != FILE_TYPE_PLUGIN_AUDIO)) {
        errno = EINVAL;
        return -1;
    }
    length = strlen(extension);
    if (length < 2U || length > FILE_EXTENSION_MAX_LENGTH) {
        errno = EINVAL;
        return -1;
    }
    for (index = 1; index < length; index++) {
        unsigned char character = (unsigned char)extension[index];

        if (!isalnum(character) && character != '_' && character != '-' &&
            character != '+') {
            errno = EINVAL;
            return -1;
        }
    }
    if (file_list_detect_builtin_type(extension) != FILE_TYPE_UNKNOWN) {
        errno = EEXIST;
        return -1;
    }
    for (index = 0; index < registered_extension_count; index++) {
        if (strcasecmp(extension,
                       registered_extensions[index].extension) == 0) {
            errno = EEXIST;
            return -1;
        }
    }
    if (registered_extension_count >= FILE_EXTENSION_MAX_COUNT) {
        errno = ENOSPC;
        return -1;
    }
    for (index = 0; index <= length; index++) {
        registered_extensions[registered_extension_count].extension[index] =
            (char)tolower((unsigned char)extension[index]);
    }
    registered_extensions[registered_extension_count].type = type;
    registered_extension_count++;
    return 0;
}

/** @brief 清空所有运行时插件扩展名。 */
void file_list_clear_registered_extensions(void)
{
    memset(registered_extensions, 0, sizeof(registered_extensions));
    registered_extension_count = 0;
}

/**
 * @brief 判断文件类型是否匹配过滤位。
 * @param type 文件类型。
 * @param filter 文件类型过滤位。
 * @return 匹配返回 1，否则返回 0。
 */
static int file_type_matches_filter(enum file_type type,
                                    unsigned int filter)
{
    if (type == FILE_TYPE_DIRECTORY) {
        return 1;
    }
    if (type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG ||
        type == FILE_TYPE_PNG || type == FILE_TYPE_GIF ||
        type == FILE_TYPE_PLUGIN_IMAGE) {
        return (filter & FILE_LIST_FILTER_IMAGES) != 0U;
    }
    if (type == FILE_TYPE_WAV || type == FILE_TYPE_MP3) {
        return (filter & FILE_LIST_FILTER_AUDIO) != 0U;
    }
    if (type == FILE_TYPE_MP4 || type == FILE_TYPE_MOV ||
        type == FILE_TYPE_MKV || type == FILE_TYPE_AVI ||
        type == FILE_TYPE_WEBM || type == FILE_TYPE_M4V) {
        return (filter & FILE_LIST_FILTER_VIDEO) != 0U;
    }
    if (type == FILE_TYPE_AAC || type == FILE_TYPE_M4A ||
        type == FILE_TYPE_FLAC || type == FILE_TYPE_OGG ||
        type == FILE_TYPE_OPUS || type == FILE_TYPE_PLUGIN_AUDIO) {
        return (filter & FILE_LIST_FILTER_AUDIO) != 0U;
    }
    if (type == FILE_TYPE_TEXT) {
        return (filter & FILE_LIST_FILTER_TEXT) != 0U;
    }
    return 0;
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

/** @brief 比较文件条目，目录始终排在普通文件之前。 */
static int compare_entry(const struct file_entry *left_entry,
                         const struct file_entry *right_entry,
                         enum file_list_sort sort)
{
    if (left_entry->type == FILE_TYPE_DIRECTORY &&
        right_entry->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    if (left_entry->type != FILE_TYPE_DIRECTORY &&
        right_entry->type == FILE_TYPE_DIRECTORY) {
        return 1;
    }
    if (sort == FILE_LIST_SORT_TYPE &&
        left_entry->type != right_entry->type) {
        return left_entry->type < right_entry->type ? -1 : 1;
    }
    if (sort == FILE_LIST_SORT_TIME &&
        left_entry->modified_time != right_entry->modified_time) {
        return left_entry->modified_time > right_entry->modified_time ?
               -1 : 1;
    }
    if (sort == FILE_LIST_SORT_SIZE &&
        left_entry->size_bytes != right_entry->size_bytes) {
        return left_entry->size_bytes > right_entry->size_bytes ? -1 : 1;
    }
    return strcasecmp(left_entry->name, right_entry->name);
}

/**
 * @brief 按指定方式对已扫描的文件列表排序。
 * @param list 文件列表。
 * @param sort 排序方式，目录始终排在普通文件之前。
 */
void file_list_sort(struct file_list *list, enum file_list_sort sort)
{
    size_t index;

    if (list == NULL || sort < FILE_LIST_SORT_NAME ||
        sort > FILE_LIST_SORT_SIZE) {
        return;
    }
    for (index = 1; index < list->count; index++) {
        struct file_entry entry = list->entries[index];
        size_t position = index;

        while (position > 0 &&
               compare_entry(&entry, &list->entries[position - 1U], sort) <
               0) {
            list->entries[position] = list->entries[position - 1U];
            position--;
        }
        list->entries[position] = entry;
    }
}

/** @brief 递归收集指定目录中的普通媒体文件。 */
static int scan_recursive_directory(const char *directory,
                                    const char *relative_directory,
                                    struct file_list *list,
                                    unsigned int filter)
{
    DIR *stream = opendir(directory);

    if (stream == NULL) {
        browser_log_errno(BROWSER_LOG_WARN, directory);
        return 0;
    }
    while (list->count < FILE_LIST_MAX_COUNT) {
        struct dirent *entry;
        struct stat status;
        char path[PATH_MAX];
        char relative_name[FILE_LIST_NAME_SIZE];
        enum file_type type;
        int written;

        errno = 0;
        entry = readdir(stream);
        if (entry == NULL) {
            if (errno != 0) {
                browser_log_errno(BROWSER_LOG_WARN, "readdir");
                closedir(stream);
                return -1;
            }
            break;
        }
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }
        if (join_path(directory, entry->d_name, path, sizeof(path)) < 0 ||
            lstat(path, &status) < 0) {
            continue;
        }
        written = snprintf(relative_name, sizeof(relative_name),
                           relative_directory[0] == '\0' ? "%s" : "%s/%s",
                           relative_directory[0] == '\0' ? entry->d_name :
                           relative_directory,
                           relative_directory[0] == '\0' ? "" : entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(relative_name)) {
            continue;
        }
        if (S_ISDIR(status.st_mode)) {
            if (scan_recursive_directory(path, relative_name, list, filter) <
                0) {
                closedir(stream);
                return -1;
            }
            continue;
        }
        if (!S_ISREG(status.st_mode)) continue;
        type = file_list_detect_type(entry->d_name);
        if (type == FILE_TYPE_UNKNOWN ||
            !file_type_matches_filter(type, filter)) {
            continue;
        }
        snprintf(list->entries[list->count].name,
                 sizeof(list->entries[list->count].name), "%s",
                 relative_name);
        list->entries[list->count].type = type;
        list->entries[list->count].size_bytes = status.st_size > 0 ?
            (uint64_t)status.st_size : 0U;
        list->entries[list->count].modified_time = status.st_mtime;
        list->count++;
    }
    if (closedir(stream) < 0) {
        browser_log_errno(BROWSER_LOG_WARN, "closedir");
        return -1;
    }
    return 0;
}

/**
 * @brief 递归扫描目录中的可浏览普通文件。
 * @param directory 起始目录。
 * @param list 输出文件列表，条目名称使用相对路径。
 * @param filter 文件类型过滤位。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan_recursive_filtered(const char *directory,
                                      struct file_list *list,
                                      unsigned int filter)
{
    if (directory == NULL || list == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(list, 0, sizeof(*list));
    if (realpath(directory, list->directory) == NULL) {
        browser_log_errno(BROWSER_LOG_ERROR, directory);
        return -1;
    }
    if (scan_recursive_directory(list->directory, "", list, filter) < 0) {
        return -1;
    }
    file_list_sort(list, FILE_LIST_SORT_NAME);
    return 0;
}

/**
 * @brief 扫描目录并按“目录优先、名称排序”生成文件列表。
 *
 * @param directory 要扫描的目录。
 * @param list 输出文件列表。
 * @return 成功返回 0，失败返回 -1。
 */
int file_list_scan_filtered(const char *directory, struct file_list *list,
                            unsigned int filter)
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
            type = file_list_detect_type(entry->d_name);
        } else {
            continue;
        }
        if (type == FILE_TYPE_UNKNOWN ||
            !file_type_matches_filter(type, filter)) {
            continue;
        }
        snprintf(list->entries[list->count].name,
                 sizeof(list->entries[list->count].name), "%s",
                 entry->d_name);
        list->entries[list->count].type = type;
        list->entries[list->count].size_bytes = S_ISREG(status.st_mode) &&
            status.st_size > 0 ? (uint64_t)status.st_size : 0U;
        list->entries[list->count].modified_time = status.st_mtime;
        list->count++;
    }
    if (closedir(stream) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "closedir");
        return -1;
    }
    file_list_sort(list, FILE_LIST_SORT_NAME);
    return 0;
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
    return file_list_scan_filtered(directory, list, FILE_LIST_FILTER_ALL);
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
    if (list->entries[index].name[0] == '/') {
        int written = snprintf(output, output_size, "%s",
                               list->entries[index].name);

        if (written < 0 || (size_t)written >= output_size) {
            errno = ENAMETOOLONG;
            return -1;
        }
        return 0;
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
    case FILE_TYPE_MP4: return "MP4";
    case FILE_TYPE_MOV: return "MOV";
    case FILE_TYPE_MKV: return "MKV";
    case FILE_TYPE_AVI: return "AVI";
    case FILE_TYPE_WEBM: return "WEBM";
    case FILE_TYPE_M4V: return "M4V";
    case FILE_TYPE_AAC: return "AAC";
    case FILE_TYPE_M4A: return "M4A";
    case FILE_TYPE_FLAC: return "FLAC";
    case FILE_TYPE_OGG: return "OGG";
    case FILE_TYPE_OPUS: return "OPUS";
    case FILE_TYPE_PLUGIN_IMAGE: return "IMAGE";
    case FILE_TYPE_PLUGIN_AUDIO: return "AUDIO";
    case FILE_TYPE_UNKNOWN:
    default: return "?";
    }
}
