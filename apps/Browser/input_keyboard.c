#include "input_keyboard.h"

#include "browser_log.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define INPUT_BATCH 32
#define INPUT_MAX_OPERATIONS 4
#define INPUT_NAME_SIZE 128

static int read_keyboard(struct input_manager *manager,
                         struct input_operation *operation,
                         struct browser_input *output);
static int read_touch(struct input_manager *manager,
                      struct input_operation *operation,
                      struct browser_input *output);

/**
 * @brief 判断键盘事件是否可触发浏览器动作。
 * @param event Linux Input 原始事件。
 * @return 可触发返回 1，否则返回 0。
 */
static int key_event_is_active(const struct input_event *event)
{
    return event->type == EV_KEY && (event->value == 1 || event->value == 2);
}

/**
 * @brief 将键盘按下或重复事件映射为浏览器动作。
 * @param event Linux Input 原始事件。
 * @return 浏览器动作。
 */
static enum input_action key_action(const struct input_event *event)
{
    if (!key_event_is_active(event)) {
        return INPUT_ACTION_NONE;
    }
    switch (event->code) {
    case KEY_LEFT: return INPUT_ACTION_PREVIOUS;
    case KEY_RIGHT: return INPUT_ACTION_NEXT;
    case KEY_UP: return INPUT_ACTION_UP;
    case KEY_DOWN: return INPUT_ACTION_DOWN;
    case KEY_ENTER:
    case KEY_KPENTER: return INPUT_ACTION_OPEN;
    case KEY_BACKSPACE:
    case KEY_ESC: return INPUT_ACTION_BACK;
    case KEY_SPACE: return INPUT_ACTION_TOGGLE;
    case KEY_R: return INPUT_ACTION_ROTATE;
    case KEY_EQUAL:
    case KEY_KPPLUS:
    case KEY_VOLUMEUP: return INPUT_ACTION_VOLUME_UP;
    case KEY_MINUS:
    case KEY_KPMINUS:
    case KEY_VOLUMEDOWN: return INPUT_ACTION_VOLUME_DOWN;
    case KEY_Q: return INPUT_ACTION_EXIT;
    default: return INPUT_ACTION_NONE;
    }
}

/**
 * @brief 打开一个非阻塞 Input 节点并打印设备名。
 * @param path 设备路径。
 * @return 成功返回文件描述符，失败返回 -1。
 */
static int open_input(const char *path)
{
    char name[INPUT_NAME_SIZE] = "unknown";
    int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, path);
        return -1;
    }
    (void)ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    browser_log(BROWSER_LOG_INFO, "input device: %s (%s)", path, name);
    return fd;
}

/**
 * @brief 查找触摸设备支持的绝对坐标轴。
 * @param manager 输入管理器。
 * @param fd 触摸设备文件描述符。
 * @return 成功返回 0，失败返回 -1。
 */
static int configure_axes(struct input_manager *manager, int fd)
{
    if (ioctl(fd, EVIOCGABS(ABS_X), &manager->abs_x) == 0 &&
        ioctl(fd, EVIOCGABS(ABS_Y), &manager->abs_y) == 0) {
        manager->abs_x_code = ABS_X;
        manager->abs_y_code = ABS_Y;
    } else if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X),
                     &manager->abs_x) == 0 &&
               ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y),
                     &manager->abs_y) == 0) {
        manager->abs_x_code = ABS_MT_POSITION_X;
        manager->abs_y_code = ABS_MT_POSITION_Y;
    } else {
        browser_log(BROWSER_LOG_ERROR,
                    "touch device has no absolute X/Y axes");
        errno = ENOTSUP;
        return -1;
    }
    if (manager->abs_x.maximum <= manager->abs_x.minimum ||
        manager->abs_y.maximum <= manager->abs_y.minimum) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/**
 * @brief 注册一个输入 operation。
 * @param manager 输入管理器。
 * @param operation 输入 operation。
 * @return 成功返回 0，失败返回 -1。
 */
static int input_manager_register_operation(
    struct input_manager *manager, struct input_operation *operation)
{
    if (manager == NULL || operation == NULL || operation->name == NULL ||
        operation->fd < 0 || operation->read == NULL) {
        errno = EINVAL;
        return -1;
    }
    operation->next = manager->operations;
    manager->operations = operation;
    return 0;
}

/**
 * @brief 关闭输入 operation 的文件描述符。
 * @param operation 输入 operation。
 */
static void close_input_operation(struct input_operation *operation)
{
    if (operation != NULL && operation->fd >= 0) {
        close(operation->fd);
        operation->fd = -1;
    }
}

/**
 * @brief 将原始绝对坐标映射到屏幕坐标。
 * @param value 原始值。
 * @param axis 轴参数。
 * @param extent 屏幕轴长度。
 * @return 范围内屏幕坐标。
 */
static int normalize_axis(int value, const struct input_absinfo *axis,
                          int extent)
{
    int64_t numerator;

    if (value < axis->minimum) {
        value = axis->minimum;
    } else if (value > axis->maximum) {
        value = axis->maximum;
    }
    numerator = (int64_t)(value - axis->minimum) * (extent - 1);
    return (int)(numerator / (axis->maximum - axis->minimum));
}

/**
 * @brief 打开可选键盘和触摸设备。
 * @param manager 输入管理器。
 * @param keyboard_path 键盘节点，"-" 表示禁用。
 * @param touch_path 触摸节点，NULL 或 "-" 表示禁用。
 * @param screen_width framebuffer 宽度。
 * @param screen_height framebuffer 高度。
 * @return 成功返回 0，失败返回 -1。
 */
int input_manager_open(struct input_manager *manager,
                       const char *keyboard_path, const char *touch_path,
                       int screen_width, int screen_height)
{
    if (manager == NULL || keyboard_path == NULL || screen_width <= 0 ||
        screen_height <= 0) {
        errno = EINVAL;
        return -1;
    }
    memset(manager, 0, sizeof(*manager));
    manager->keyboard.fd = -1;
    manager->touch.fd = -1;
    manager->screen_width = screen_width;
    manager->screen_height = screen_height;
    if (strcmp(keyboard_path, "-") != 0) {
        manager->keyboard.fd = open_input(keyboard_path);
        if (manager->keyboard.fd < 0) {
            return -1;
        }
        manager->keyboard.name = "keyboard";
        manager->keyboard.read = read_keyboard;
        manager->keyboard.close = close_input_operation;
        if (input_manager_register_operation(manager,
                                             &manager->keyboard) < 0) {
            input_manager_close(manager);
            return -1;
        }
    }
    if (touch_path != NULL && strcmp(touch_path, "-") != 0) {
        manager->touch.fd = open_input(touch_path);
        if (manager->touch.fd < 0 || configure_axes(manager,
                                                    manager->touch.fd) < 0) {
            input_manager_close(manager);
            return -1;
        }
        manager->touch.name = "touch";
        manager->touch.read = read_touch;
        manager->touch.close = close_input_operation;
        if (input_manager_register_operation(manager,
                                             &manager->touch) < 0) {
            input_manager_close(manager);
            return -1;
        }
    }
    if (manager->operations == NULL) {
        errno = ENODEV;
        return -1;
    }
    return 0;
}

/**
 * @brief 在 SYN_REPORT 时生成移动、点击或滑动手势。
 * @param manager 输入管理器。
 * @param output 输出事件。
 * @return 生成事件返回 1，否则返回 0。
 */
static int finish_touch_report(struct input_manager *manager,
                               struct browser_input *output)
{
    int threshold_x = manager->screen_width / 20;
    int threshold_y = manager->screen_height / 20;

    manager->x = normalize_axis(manager->raw_x, &manager->abs_x,
                                manager->screen_width);
    manager->y = normalize_axis(manager->raw_y, &manager->abs_y,
                                manager->screen_height);
    if (threshold_x < 32) {
        threshold_x = 32;
    }
    if (threshold_y < 32) {
        threshold_y = 32;
    }
    if (manager->touching == 2) {
        manager->touching = 1;
        manager->start_x = manager->x;
        manager->start_y = manager->y;
        manager->last_x = manager->x;
        manager->last_y = manager->y;
    }
    if (manager->release_pending) {
        int absolute_x;
        int absolute_y;

        output->x = manager->x;
        output->y = manager->y;
        output->start_x = manager->start_x;
        output->start_y = manager->start_y;
        output->dx = manager->x - manager->start_x;
        output->dy = manager->y - manager->start_y;
        absolute_x = output->dx < 0 ? -output->dx : output->dx;
        absolute_y = output->dy < 0 ? -output->dy : output->dy;
        if (absolute_x <= 15 && absolute_y <= 15) {
            output->touch = TOUCH_ACTION_TAP;
        } else if (absolute_x >= threshold_x || absolute_y >= threshold_y) {
            output->touch = TOUCH_ACTION_SWIPE;
        }
        manager->touching = 0;
        manager->release_pending = 0;
        return output->touch != TOUCH_ACTION_NONE;
    }
    if (manager->touching == 1 &&
        (manager->x != manager->last_x || manager->y != manager->last_y)) {
        output->touch = TOUCH_ACTION_MOVE;
        output->x = manager->x;
        output->y = manager->y;
        output->start_x = manager->start_x;
        output->start_y = manager->start_y;
        output->dx = manager->x - manager->start_x;
        output->dy = manager->y - manager->start_y;
        manager->last_x = manager->x;
        manager->last_y = manager->y;
        return 1;
    }
    return 0;
}

/**
 * @brief 读取并聚合触摸 Input 事件。
 * @param manager 输入管理器。
 * @param operation 触摸输入 operation。
 * @param output 输出归一化事件。
 * @return 生成事件返回 1，无完整事件返回 0，失败返回 -1。
 */
static int read_touch(struct input_manager *manager,
                      struct input_operation *operation,
                      struct browser_input *output)
{
    struct input_event events[INPUT_BATCH];
    ssize_t bytes = read(operation->fd, events, sizeof(events));
    size_t count;
    size_t index;
    int generated = 0;

    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                      errno == EINTR)) {
        return 0;
    }
    if (bytes <= 0 || (size_t)bytes % sizeof(events[0]) != 0) {
        errno = EIO;
        return -1;
    }
    count = (size_t)bytes / sizeof(events[0]);
    for (index = 0; index < count; index++) {
        const struct input_event *event = &events[index];

        if (event->type == EV_ABS && event->code == manager->abs_x_code) {
            manager->raw_x = event->value;
        } else if (event->type == EV_ABS &&
                   event->code == manager->abs_y_code) {
            manager->raw_y = event->value;
        } else if (event->type == EV_ABS &&
                   event->code == ABS_MT_TRACKING_ID) {
            if (event->value >= 0 && manager->touching == 0) {
                manager->touching = 2;
            } else if (event->value < 0 && manager->touching != 0) {
                manager->release_pending = 1;
            }
        } else if (event->type == EV_KEY &&
                   (event->code == BTN_TOUCH || event->code == BTN_LEFT)) {
            if (event->value != 0 && manager->touching == 0) {
                manager->touching = 2;
            } else if (event->value == 0 && manager->touching != 0) {
                manager->release_pending = 1;
            }
        } else if (event->type == EV_SYN && event->code == SYN_REPORT) {
            if (finish_touch_report(manager, output)) {
                generated = 1;
            }
        }
    }
    return generated;
}

/**
 * @brief 读取一批键盘事件。
 * @param manager 输入管理器。
 * @param operation 键盘输入 operation。
 * @param output 输出浏览器事件。
 * @return 生成动作返回 1，无动作返回 0，失败返回 -1。
 */
static int read_keyboard(struct input_manager *manager,
                         struct input_operation *operation,
                         struct browser_input *output)
{
    struct input_event event;
    ssize_t bytes = read(operation->fd, &event, sizeof(event));

    (void)manager;
    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                      errno == EINTR)) {
        return 0;
    }
    if (bytes <= 0 || (size_t)bytes != sizeof(event)) {
        errno = EIO;
        return -1;
    }
    output->action = key_action(&event);
    return output->action != INPUT_ACTION_NONE;
}

/**
 * @brief 等待输入或超时。
 * @param manager 输入管理器。
 * @param event 输出浏览器输入。
 * @param timeout_ms 最长等待毫秒，-1 表示无限。
 * @return 收到事件返回 1，超时返回 0，失败返回 -1。
 */
int input_manager_wait(struct input_manager *manager,
                       struct browser_input *event, int timeout_ms)
{
    struct pollfd descriptors[INPUT_MAX_OPERATIONS];
    struct input_operation *operations[INPUT_MAX_OPERATIONS];
    struct input_operation *operation;
    int count = 0;
    int poll_result;
    int index;

    if (manager == NULL || event == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(event, 0, sizeof(*event));
    for (operation = manager->operations; operation != NULL;
         operation = operation->next) {
        if (count >= INPUT_MAX_OPERATIONS) {
            errno = E2BIG;
            return -1;
        }
        if (operation->fd < 0) {
            continue;
        }
        descriptors[count].fd = operation->fd;
        descriptors[count].events = POLLIN;
        descriptors[count].revents = 0;
        operations[count++] = operation;
    }
    if (count == 0) {
        errno = ENODEV;
        return -1;
    }
    do {
        poll_result = poll(descriptors, (nfds_t)count, timeout_ms);
    } while (poll_result < 0 && errno == EINTR);
    if (poll_result <= 0) {
        return poll_result;
    }
    for (index = 0; index < count; index++) {
        int result;

        if ((descriptors[index].revents &
             (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            errno = EIO;
            return -1;
        }
        if ((descriptors[index].revents & POLLIN) == 0) {
            continue;
        }
        result = operations[index]->read(manager, operations[index], event);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

/**
 * @brief 关闭全部输入设备。
 * @param manager 输入管理器。
 */
void input_manager_close(struct input_manager *manager)
{
    struct input_operation *operation;

    if (manager == NULL) {
        return;
    }
    for (operation = manager->operations; operation != NULL;
         operation = operation->next) {
        if (operation->close != NULL) {
            operation->close(operation);
        }
    }
    close_input_operation(&manager->keyboard);
    close_input_operation(&manager->touch);
    manager->operations = NULL;
}

/**
 * @brief 获取输入动作可读名称。
 * @param action 输入动作。
 * @return 静态字符串。
 */
const char *input_action_name(enum input_action action)
{
    switch (action) {
    case INPUT_ACTION_PREVIOUS: return "previous";
    case INPUT_ACTION_NEXT: return "next";
    case INPUT_ACTION_UP: return "up";
    case INPUT_ACTION_DOWN: return "down";
    case INPUT_ACTION_OPEN: return "open";
    case INPUT_ACTION_BACK: return "back";
    case INPUT_ACTION_TOGGLE: return "toggle";
    case INPUT_ACTION_ROTATE: return "rotate";
    case INPUT_ACTION_VOLUME_UP: return "volume-up";
    case INPUT_ACTION_VOLUME_DOWN: return "volume-down";
    case INPUT_ACTION_EXIT: return "exit";
    case INPUT_ACTION_NONE:
    default: return "none";
    }
}
