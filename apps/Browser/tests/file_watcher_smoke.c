#include "file_watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** @brief 确认监听器已经消费到至少一个目录变化。 */
static int expect_change(struct file_watcher *watcher, const char *label)
{
    int result = file_watcher_consume(watcher);

    if (result == 1) return 0;
    fprintf(stderr, "FAIL %s: %s\n", label,
            result < 0 ? strerror(errno) : "no event");
    return -1;
}

/** @brief 创建一个短测试文件并确保写入完成。 */
static int write_test_file(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0) return -1;
    if (write(fd, "x", 1) != 1) {
        (void)close(fd);
        return -1;
    }
    return close(fd) < 0 ? -1 : 0;
}

/** @brief 验证创建、重命名和删除都会触发媒体目录监听。 */
int main(void)
{
    char directory[] = "/tmp/browser-file-watcher.XXXXXX";
    char first_path[PATH_MAX];
    char second_path[PATH_MAX];
    struct file_watcher watcher;
    int result = EXIT_FAILURE;

    if (mkdtemp(directory) == NULL ||
        snprintf(first_path, sizeof(first_path), "%s/first.txt", directory) >=
            (int)sizeof(first_path) ||
        snprintf(second_path, sizeof(second_path), "%s/second.txt", directory) >=
            (int)sizeof(second_path) || file_watcher_init(&watcher) < 0 ||
        file_watcher_update(&watcher, directory) < 0) {
        fprintf(stderr, "FAIL watcher setup: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (write_test_file(first_path) < 0 ||
        expect_change(&watcher, "create") < 0 ||
        rename(first_path, second_path) < 0 ||
        expect_change(&watcher, "rename") < 0 || unlink(second_path) < 0 ||
        expect_change(&watcher, "delete") < 0) {
        goto cleanup;
    }
    printf("PASS file watcher\n");
    result = EXIT_SUCCESS;
cleanup:
    file_watcher_destroy(&watcher);
    (void)unlink(first_path);
    (void)unlink(second_path);
    (void)rmdir(directory);
    return result;
}
