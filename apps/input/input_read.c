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
 * @brief 获取输入事件类型名称。
 *
 * @param type Linux Input 事件类型。
 * @return 事件类型对应的字符串。
 */
static const char *event_type_name(unsigned int type)
{
    switch (type) {
    case EV_SYN:
        return "EV_SYN";
    case EV_KEY:
        return "EV_KEY";
    case EV_REL:
        return "EV_REL";
    case EV_ABS:
        return "EV_ABS";
    case EV_MSC:
        return "EV_MSC";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 获取按键事件值对应的状态名称。
 *
 * @param value 按键事件值。
 * @return 按键状态对应的字符串。
 */
static const char *key_state_name(int value)
{
    switch (value) {
    case 0:
        return "release";
    case 1:
        return "press";
    case 2:
        return "repeat";
    default:
        return "unknown";
    }
}

/**
 * @brief 打印一条 Linux Input 输入事件。
 *
 * @param event 输入事件结构体。
 */
static void print_input_event(const struct input_event *event)
{
    printf("time=%ld.%06ld type=%s(%u) code=%u value=%d",
            (long)event->time.tv_sec,
            (long)event->time.tv_usec,
            event_type_name(event->type),
            event->type,
            event->code,
            event->value);

    if (event->type == EV_KEY) {
        printf(" state=%s", key_state_name(event->value));
    }

    printf("\n");
}

/**
 * @brief 打开输入设备并循环读取输入事件。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    struct input_event events[EVENT_BUF_COUNT];
    struct pollfd pfd;
    char device_name[DEVICE_NAME_SIZE] = "unknown";
    const char *device_path;
    int fd;

    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    device_path = argv[1];

    fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open input device");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
        perror("EVIOCGNAME");
    }

    printf("device: %s\n", device_path);
    printf("name  : %s\n", device_name);
    printf("press ESC to exit\n");

    pfd.fd = fd;
    pfd.events = POLLIN;

    while (1) {
        ssize_t bytes;
        size_t event_count;
        int poll_ret;

        poll_ret = poll(&pfd, 1, -1);
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("poll");
            close(fd);
            return EXIT_FAILURE;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "input device error: revents=0x%x\n",
                    pfd.revents);
            close(fd);
            return EXIT_FAILURE;
        }

        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        bytes = read(fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK ||
                errno == EINTR) {
                continue;
            }

            perror("read input event");
            close(fd);
            return EXIT_FAILURE;
        }

        if ((size_t)bytes % sizeof(struct input_event) != 0) {
            fprintf(stderr, "incomplete input event data\n");
            continue;
        }

        event_count = (size_t)bytes / sizeof(struct input_event);

        for (size_t i = 0; i < event_count; i++) {
            print_input_event(&events[i]);

            if (events[i].type == EV_KEY &&
                events[i].code == KEY_ESC &&
                events[i].value == 1) {
                close(fd);
                return EXIT_SUCCESS;
            }
        }
    }
}

