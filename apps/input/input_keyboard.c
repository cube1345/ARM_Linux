#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define EVENT_BUF_COUNT 16
#define DEVICE_NAME_SIZE 128

/**
 * @brief 项目内部统一输入动作。
 */
enum input_action {
    INPUT_ACTION_NONE = 0,
    INPUT_ACTION_PREVIOUS,
    INPUT_ACTION_NEXT,
    INPUT_ACTION_EXIT
};

/**
 * @brief 打印程序用法。
 *
 * @param prog 程序名称。
 */

static void usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s <input_event_device>\n", prog);
    printf("\nExample:\n");
    printf("  %s /dev/input/event0\n", prog);
}

/**
 * @brief 获取输入动作对应的字符串。
 *
 * @param action 项目内部输入动作。
 * @return 输入动作对应的字符串。
 */
static const char *input_action_name(enum input_action action)
{
    switch (action) {
    case INPUT_ACTION_PREVIOUS:
        return "previous";
    case INPUT_ACTION_NEXT:
        return "next";
    case INPUT_ACTION_EXIT:
        return "exit";
    case INPUT_ACTION_NONE:
    default:
        return "none";
    }
}

/**
 * @brief 把 Linux Input 原始事件转换为项目内部输入动作。
 *
 * 该函数只处理按键按下事件，忽略释放和长按重复事件。
 *
 * @param event Linux Input 原始输入事件。
 * @return 转换后的项目内部输入动作。
 */
static enum input_action input_event_to_action(
    const struct input_event *event)
{
    if (event->type != EV_KEY || event->value != 1) {
        return INPUT_ACTION_NONE;
    }

    switch (event->code) {
    case KEY_LEFT:
        return INPUT_ACTION_PREVIOUS;
    case KEY_RIGHT:
        return INPUT_ACTION_NEXT;
    case KEY_ESC:
        return INPUT_ACTION_EXIT;
    default:
        return INPUT_ACTION_NONE;
    }
}

/**
 * @brief 打开 Linux Input 输入设备。
 *
 * @param device_path 输入设备路径，例如 /dev/input/event0。
 * @return 成功返回输入设备文件描述符，失败返回 -1。
 */
static int input_open(const char *device_path)
{
    char device_name[DEVICE_NAME_SIZE] = "unknown";
    int fd;

    fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open input device");
        return -1;
    }

    if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
        perror("EVIOCGNAME");
    }

    printf("device: %s\n", device_path);
    printf("name  : %s\n", device_name);

    return fd;
}

/**
 * @brief 等待并读取一个有效的项目输入动作。
 *
 * 函数使用 poll 等待输入设备产生事件，然后读取并解析
 * struct input_event。没有有效动作时继续等待。
 *
 * @param fd 输入设备文件描述符。
 * @param action 输出的项目内部输入动作。
 * @return 成功读取动作返回 0，发生错误返回 -1。
 */
static int input_wait_action(int fd, enum input_action *action)
{
    struct input_event events[EVENT_BUF_COUNT];
    struct pollfd pfd;

    if (action == NULL) {
        errno = EINVAL;
        return -1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        ssize_t bytes;
        size_t event_count;
        int poll_ret;

        pfd.revents = 0;

        poll_ret = poll(&pfd, 1, -1);
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            return -1;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr,
                    "input device error: revents=0x%x\n",
                    pfd.revents);
            return -1;
        }

        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        bytes = read(fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EINTR ||
                errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                continue;
            }

            perror("read input event");
            return -1;
        }

        if (bytes == 0) {
            fprintf(stderr, "input device closed\n");
            return -1;
        }

        if ((size_t)bytes % sizeof(struct input_event) != 0) {
            fprintf(stderr, "incomplete input event data\n");
            continue;
        }

        event_count = (size_t)bytes / sizeof(struct input_event);

        for (size_t i = 0; i < event_count; i++) {
            *action = input_event_to_action(&events[i]);

            if (*action != INPUT_ACTION_NONE) {
                return 0;
            }
        }
    }
}

/**
 * @brief 程序入口，读取键盘事件并输出项目内部动作。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    enum input_action action;
    int fd;

    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    fd = input_open(argv[1]);
    if (fd < 0) {
        return EXIT_FAILURE;
    }

    printf("LEFT : previous\n");
    printf("RIGHT: next\n");
    printf("ESC  : exit\n");

    while (1) {
        if (input_wait_action(fd, &action) < 0) {
            close(fd);
            return EXIT_FAILURE;
        }

        printf("action: %s\n", input_action_name(action));

        if (action == INPUT_ACTION_EXIT) {
            break;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
