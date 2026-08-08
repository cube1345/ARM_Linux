#include "file_watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define FILE_WATCHER_EVENT_BUFFER_SIZE 4096U
#define FILE_WATCHER_MASK (IN_CREATE | IN_DELETE | IN_MOVED_FROM | \
                           IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB | \
                           IN_DELETE_SELF | IN_MOVE_SELF)

/** @brief 判断 inotify mask 是否意味着文件列表需要重扫。 */
static int file_watcher_event_changed(uint32_t mask)
{
    return (mask & (FILE_WATCHER_MASK | IN_Q_OVERFLOW | IN_IGNORED)) != 0U;
}

/** @brief 初始化 nonblocking inotify 文件监听器。 */
int file_watcher_init(struct file_watcher *watcher)
{
    if (watcher == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(watcher, 0, sizeof(*watcher));
    watcher->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    watcher->watch_descriptor = -1;
    return watcher->fd < 0 ? -1 : 0;
}

/** @brief 将监听器切换到指定目录。 */
int file_watcher_update(struct file_watcher *watcher, const char *directory)
{
    int descriptor;

    if (watcher == NULL || watcher->fd < 0 || directory == NULL ||
        directory[0] == '\0') {
        errno = EINVAL;
        return -1;
    }
    if (watcher->watch_descriptor >= 0 &&
        strcmp(watcher->directory, directory) == 0) {
        return 0;
    }
    if (watcher->watch_descriptor >= 0) {
        (void)inotify_rm_watch(watcher->fd, watcher->watch_descriptor);
        watcher->watch_descriptor = -1;
    }
    descriptor = inotify_add_watch(watcher->fd, directory, FILE_WATCHER_MASK);
    if (descriptor < 0) return -1;
    if (snprintf(watcher->directory, sizeof(watcher->directory), "%s",
                 directory) >= (int)sizeof(watcher->directory)) {
        (void)inotify_rm_watch(watcher->fd, descriptor);
        errno = ENAMETOOLONG;
        return -1;
    }
    watcher->watch_descriptor = descriptor;
    return 0;
}

/** @brief 消费当前已就绪的目录变化事件。 */
int file_watcher_consume(struct file_watcher *watcher)
{
    unsigned char buffer[FILE_WATCHER_EVENT_BUFFER_SIZE];
    int changed = 0;

    if (watcher == NULL || watcher->fd < 0) {
        errno = EINVAL;
        return -1;
    }
    while (1) {
        ssize_t bytes = read(watcher->fd, buffer, sizeof(buffer));
        size_t offset = 0;

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return changed;
            if (errno == EINTR) continue;
            return -1;
        }
        if (bytes == 0) return changed;
        while (offset + sizeof(struct inotify_event) <= (size_t)bytes) {
            const struct inotify_event *event =
                (const struct inotify_event *)(buffer + offset);
            size_t event_size = sizeof(*event) + event->len;

            if (event_size > (size_t)bytes - offset) {
                errno = EIO;
                return -1;
            }
            if (event->wd == watcher->watch_descriptor ||
                (event->mask & IN_Q_OVERFLOW) != 0U) {
                if (file_watcher_event_changed(event->mask)) changed = 1;
                if ((event->mask & IN_IGNORED) != 0U) {
                    watcher->watch_descriptor = -1;
                    watcher->directory[0] = '\0';
                }
            }
            offset += event_size;
        }
    }
}

/** @brief 移除目录监听并释放内核文件描述符。 */
void file_watcher_destroy(struct file_watcher *watcher)
{
    if (watcher == NULL) return;
    if (watcher->fd >= 0 && watcher->watch_descriptor >= 0) {
        (void)inotify_rm_watch(watcher->fd, watcher->watch_descriptor);
    }
    if (watcher->fd >= 0) (void)close(watcher->fd);
    memset(watcher, 0, sizeof(*watcher));
    watcher->fd = -1;
    watcher->watch_descriptor = -1;
}
