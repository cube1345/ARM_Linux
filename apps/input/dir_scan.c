#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#define MAX_IMAGE_COUNT 128
#define MAX_PATH_LENGTH 256

struct image_list {
    char paths[MAX_IMAGE_COUNT][MAX_PATH_LENGTH];
    size_t count;
};

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s <directory_path>\n", prog);
    printf("\nExample:\n");
    printf("  %s /path/to/images\n", prog);
}

/**
 * @brief 判断文件名是否以 .bmp 扩展名结尾。
 *
 * 扩展名比较不区分大小写，因此同时支持 .bmp、.BMP 和 .Bmp。
 *
 * @param file_name 文件名称。
 * @return 是 BMP 文件返回 1，否则返回 0。
 */
static int has_bmp_extension(const char *file_name)
{
    const char *extension;

    if (file_name == NULL) {
        return 0;
    }

    extension = strrchr(file_name, '.');
    if (extension == NULL) {
        return 0;
    }

    return strcasecmp(extension, ".bmp") == 0;
}


/**
 * @brief 拼接目录路径和文件名。
 *
 * @param output 输出路径缓冲区。
 * @param output_size 输出缓冲区大小。
 * @param dir_path 目录路径。
 * @param file_name 文件名称。
 * @return 成功返回 0，路径过长返回 -1。
 */
static int build_file_path(char *output,
                            size_t output_size,
                            const char *dir_path,
                            const char *file_name)
{
    size_t dir_length;
    int written;

    if (output == NULL || dir_path == NULL || file_name == NULL) {
        return -1;
    }

    dir_length = strlen(dir_path);

    if (dir_length > 0 && dir_path[dir_length - 1] == '/') {
        written = snprintf(output,
                            output_size,
                            "%s%s",
                            dir_path,
                            file_name);
    } else {
        written = snprintf(output,
                            output_size,
                            "%s/%s",
                            dir_path,
                            file_name);
    }

    if (written < 0 || (size_t)written >= output_size) {
        fprintf(stderr,
                "path is too long: %s/%s\n",
                dir_path,
                file_name);
        return -1;
    }

    return 0;
}

/**
 * @brief 比较两个图片路径，供 qsort 使用。
 *
 * @param left 左侧路径。
 * @param right 右侧路径。
 * @return 小于、等于或大于 0，含义与 strcmp 相同。
 */
static int compare_path(const void *left, const void *right)
{
    const char *left_path = left;
    const char *right_path = right;

    return strcmp(left_path, right_path);
}


static int scan_bmp_dir(const char *dir_path, struct image_list *image_list)
{
    DIR *dir;
    struct dirent *entry;

    if (dir_path == NULL || image_list == NULL) {
        return -1;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    image_list->count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && has_bmp_extension(entry->d_name)) {
            if (image_list->count >= MAX_IMAGE_COUNT) {
                fprintf(stderr,
                        "too many BMP files in directory: %s\n",
                        dir_path);
                closedir(dir);
                return -1;
            }

            if (build_file_path(image_list->paths[image_list->count],
                                MAX_PATH_LENGTH,
                                dir_path,
                                entry->d_name) < 0) {
                closedir(dir);
                return -1;
            }

            image_list->count++;
        }
    }

    closedir(dir);

    qsort(image_list->paths,
          image_list->count,
          sizeof(image_list->paths[0]),
          compare_path);

    return 0;
}

/**
 * @brief 程序入口，扫描并打印目录中的 BMP 文件。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    struct image_list list;
    size_t i;

    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (scan_bmp_dir(argv[1], &list) < 0) {
        return EXIT_FAILURE;
    }

    printf("image count: %zu\n", list.count);

    for (i = 0; i < list.count; i++) {
        printf("[%zu] %s\n", i, list.paths[i]);
    }

    return EXIT_SUCCESS;
}


