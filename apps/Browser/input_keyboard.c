#include "input_keyboard.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define INPUT_EVENT_BATCH 16
#define INPUT_DEVICE_NAME_SIZE 128

/**
 * @brief 将 Linux Input 事件转换为浏览器动作。
 *
 * @param event Linux Input 原始事件。
 * @return 转换后的浏览器动作。
 */
static enum input_action event_to_action(const struct input_event *event)
{
    if (event->type != EV_KEY || event->value != 1) {
        return INPUT_ACTION_NONE;
    }

    switch (event->code) {
    case KEY_LEFT:
        return INPUT_ACTION_PREVIOUS;
    case KEY_RIGHT:
        return INPUT_ACTION_NEXT;
    case KEY_UP:
        return INPUT_ACTION_UP;
    case KEY_DOWN:
        return INPUT_ACTION_DOWN;
    case KEY_ENTER:
    case KEY_KPENTER:
        return INPUT_ACTION_OPEN;
    case KEY_BACKSPACE:
    case KEY_ESC:
        return INPUT_ACTION_BACK;
    case KEY_SPACE:
        return INPUT_ACTION_TOGGLE;
    case KEY_Q:
        return INPUT_ACTION_EXIT;
    default:
        return INPUT_ACTION_NONE;
    }
}

/**
 * @brief 打开 Linux Input 键盘设备。
 *
 * @param keyboard 键盘设备上下文。
 * @param device_path 输入设备路径。
 * @return 成功返回 0，失败返回 -1。
 */
int input_keyboard_open(struct input_keyboard *keyboard,
                        const char *device_path)
{
    char device_name[INPUT_DEVICE_NAME_SIZE] = "unknown";

    if (keyboard == NULL || device_path == NULL) {
        errno = EINVAL;
        return -1;
    }

    keyboard->fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (keyboard->fd < 0) {
        perror("open input device");
        return -1;
    }

    if (ioctl(keyboard->fd,
              EVIOCGNAME(sizeof(device_name)), device_name) < 0) {
        fprintf(stderr, "warning: cannot read input device name: %s\n",
                strerror(errno));
    }

    printf("input device: %s (%s)\n", device_path, device_name);
    return 0;
}

/**
 * @brief 等待一个有效的浏览器输入动作。
 *
 * @param keyboard 键盘设备上下文。
 * @param action 输出的输入动作。
 * @return 成功返回 0，失败返回 -1。
 */
int input_keyboard_wait(struct input_keyboard *keyboard,
                        enum input_action *action)
{
    struct input_event events[INPUT_EVENT_BATCH];
    struct pollfd pfd;

    if (keyboard == NULL || keyboard->fd < 0 || action == NULL) {
        errno = EINVAL;
        return -1;
    }

    pfd.fd = keyboard->fd;
    pfd.events = POLLIN;

    while (1) {
        ssize_t bytes;
        size_t event_count;
        size_t index;
        int poll_result;

        pfd.revents = 0;
        poll_result = poll(&pfd, 1, -1);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll input device");
            return -1;
        }

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "input device error: revents=0x%x\n",
                    pfd.revents);
            return -1;
        }

        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        bytes = read(keyboard->fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EINTR || errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                continue;
            }
            perror("read input device");
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
        for (index = 0; index < event_count; index++) {
            *action = event_to_action(&events[index]);
            if (*action != INPUT_ACTION_NONE) {
                return 0;
            }
        }
    }
}

/**
 * @brief 关闭 Linux Input 键盘设备。
 *
 * @param keyboard 键盘设备上下文。
 */
void input_keyboard_close(struct input_keyboard *keyboard)
{
    if (keyboard != NULL && keyboard->fd >= 0) {
        close(keyboard->fd);
        keyboard->fd = -1;
    }
}

/**
 * @brief 获取输入动作的可读名称。
 *
 * @param action 输入动作。
 * @return 输入动作对应的字符串。
 */
const char *input_action_name(enum input_action action)
{
    switch (action) {
    case INPUT_ACTION_PREVIOUS:
        return "previous";
    case INPUT_ACTION_NEXT:
        return "next";
    case INPUT_ACTION_UP:
        return "up";
    case INPUT_ACTION_DOWN:
        return "down";
    case INPUT_ACTION_OPEN:
        return "open";
    case INPUT_ACTION_BACK:
        return "back";
    case INPUT_ACTION_TOGGLE:
        return "toggle";
    case INPUT_ACTION_EXIT:
        return "exit";
    case INPUT_ACTION_NONE:
    default:
        return "none";
    }
}
