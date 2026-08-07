#include "image_list.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/**
 * @brief 判断文件名是否具有 BMP 扩展名。
 *
 * @param file_name 文件名。
 * @return 是 BMP 扩展名返回 1，否则返回 0。
 */
static int has_bmp_extension(const char *file_name)
{
    const char *extension;

    if (file_name == NULL) {
        return 0;
    }

    extension = strrchr(file_name, '.');
    return extension != NULL && strcasecmp(extension, ".bmp") == 0;
}

/**
 * @brief 拼接目录路径和文件名。
 *
 * @param output 输出路径缓冲区。
 * @param output_size 输出缓冲区大小。
 * @param directory 目录路径。
 * @param file_name 文件名。
 * @return 成功返回 0，路径过长返回 -1。
 */
static int build_file_path(char *output, size_t output_size,
                           const char *directory, const char *file_name)
{
    size_t directory_length;
    int written;

    directory_length = strlen(directory);

    if (directory_length > 0 && directory[directory_length - 1] == '/') {
        written = snprintf(output, output_size, "%s%s",
                           directory, file_name);
    } else {
        written = snprintf(output, output_size, "%s/%s",
                           directory, file_name);
    }

    if (written < 0 || (size_t)written >= output_size) {
        fprintf(stderr, "path is too long: %s/%s\n",
                directory, file_name);
        return -1;
    }

    return 0;
}

/**
 * @brief 比较两个图片路径。
 *
 * @param left 左侧路径。
 * @param right 右侧路径。
 * @return 返回值语义与 strcmp 相同。
 */
static int compare_path(const void *left, const void *right)
{
    return strcmp((const char *)left, (const char *)right);
}

/**
 * @brief 扫描并排序目录中的 BMP 普通文件。
 *
 * @param directory 要扫描的目录路径。
 * @param list 输出的图片列表。
 * @return 成功返回 0，失败返回 -1。
 */
int image_list_scan(const char *directory, struct image_list *list)
{
    DIR *dir;

    if (directory == NULL || list == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(list, 0, sizeof(*list));

    dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while (1) {
        struct dirent *entry;
        char file_path[IMAGE_LIST_MAX_PATH];
        struct stat st;

        errno = 0;
        entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) {
                perror("readdir");
                closedir(dir);
                return -1;
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0 ||
            !has_bmp_extension(entry->d_name)) {
            continue;
        }

        if (build_file_path(file_path, sizeof(file_path),
                            directory, entry->d_name) < 0) {
            continue;
        }

        if (stat(file_path, &st) < 0) {
            perror(file_path);
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (list->count >= IMAGE_LIST_MAX_COUNT) {
            fprintf(stderr, "image limit reached: %d\n",
                    IMAGE_LIST_MAX_COUNT);
            break;
        }

        snprintf(list->paths[list->count],
                 sizeof(list->paths[list->count]), "%s", file_path);
        list->count++;
    }

    if (closedir(dir) < 0) {
        perror("closedir");
        return -1;
    }

    qsort(list->paths, list->count, sizeof(list->paths[0]), compare_path);
    return 0;
}
