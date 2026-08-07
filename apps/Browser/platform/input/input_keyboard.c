#include "input_keyboard.h"

#include "browser_log.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define INPUT_BATCH 32
#define INPUT_MAX_OPERATIONS 4
#define INPUT_NAME_SIZE 128
#define INPUT_BITS_PER_LONG ((int)(sizeof(unsigned long) * CHAR_BIT))
#define INPUT_STDIN_PATH "stdin"
#define INPUT_AUTO_PATH "auto"
#define INPUT_EVENT_SCAN_LIMIT 32

static int read_keyboard(struct input_manager *manager,
                         struct input_operation *operation,
                         struct browser_input *output);
static int read_stdin(struct input_manager *manager,
                      struct input_operation *operation,
                      struct browser_input *output);
static int read_touch(struct input_manager *manager,
                      struct input_operation *operation,
                      struct browser_input *output);
static int read_mouse(struct input_manager *manager,
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
 * @brief 将标准输入字符映射为浏览器动作。
 * @param value 输入字符。
 * @return 浏览器动作。
 */
static enum input_action stdin_char_action(unsigned char value)
{
    switch (value) {
    case 'a':
    case 'A':
    case 'h':
    case 'H': return INPUT_ACTION_PREVIOUS;
    case 'd':
    case 'D':
    case 'l':
    case 'L': return INPUT_ACTION_NEXT;
    case 'w':
    case 'W':
    case 'k':
    case 'K': return INPUT_ACTION_UP;
    case 's':
    case 'S':
    case 'j':
    case 'J': return INPUT_ACTION_DOWN;
    case '\r':
    case '\n':
    case 'e':
    case 'E': return INPUT_ACTION_OPEN;
    case 0x7f:
    case 0x1b:
    case 'b':
    case 'B': return INPUT_ACTION_BACK;
    case ' ': return INPUT_ACTION_TOGGLE;
    case 'r':
    case 'R': return INPUT_ACTION_ROTATE;
    case '+':
    case '=': return INPUT_ACTION_VOLUME_UP;
    case '-':
    case '_': return INPUT_ACTION_VOLUME_DOWN;
    case 'q':
    case 'Q': return INPUT_ACTION_EXIT;
    default: return INPUT_ACTION_NONE;
    }
}

/**
 * @brief 判断 Linux Input 位图中某个 bit 是否置位。
 * @param bits 位图。
 * @param bit 目标 bit。
 * @return 置位返回 1，否则返回 0。
 */
static int input_bit_is_set(const unsigned long *bits, int bit)
{
    return (bits[bit / INPUT_BITS_PER_LONG] &
            (1UL << (bit % INPUT_BITS_PER_LONG))) != 0;
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
 * @brief 判断设备是否支持相对坐标鼠标移动。
 * @param fd 输入设备文件描述符。
 * @return 支持返回 0，否则返回 -1。
 */
static int configure_relative_mouse(int fd)
{
    unsigned long rel_bits[(REL_MAX + INPUT_BITS_PER_LONG) /
                           INPUT_BITS_PER_LONG];

    memset(rel_bits, 0, sizeof(rel_bits));
    if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits) < 0) {
        return -1;
    }
    if (!input_bit_is_set(rel_bits, REL_X) ||
        !input_bit_is_set(rel_bits, REL_Y)) {
        errno = ENOTSUP;
        return -1;
    }
    return 0;
}

/**
 * @brief 判断 Input 节点是否像浏览器键盘。
 * @param fd 输入设备文件描述符。
 * @return 是键盘返回 1，否则返回 0。
 */
static int input_device_is_keyboard(int fd)
{
    unsigned long key_bits[(KEY_MAX + INPUT_BITS_PER_LONG) /
                           INPUT_BITS_PER_LONG];

    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return 0;
    }
    return (input_bit_is_set(key_bits, KEY_ENTER) ||
            input_bit_is_set(key_bits, KEY_KPENTER)) &&
           input_bit_is_set(key_bits, KEY_UP) &&
           input_bit_is_set(key_bits, KEY_DOWN) &&
           input_bit_is_set(key_bits, KEY_LEFT) &&
           input_bit_is_set(key_bits, KEY_RIGHT);
}

/**
 * @brief 判断 Input 节点是否像触摸屏或鼠标。
 * @param fd 输入设备文件描述符。
 * @return 是指针设备返回 1，否则返回 0。
 */
static int input_device_is_pointer(int fd)
{
    struct input_manager probe;

    memset(&probe, 0, sizeof(probe));
    if (configure_axes(&probe, fd) == 0) {
        return 1;
    }
    return configure_relative_mouse(fd) == 0;
}

/**
 * @brief 自动扫描 /dev/input/event* 并选择匹配设备。
 * @param match 设备匹配回调。
 * @param kind 日志中显示的设备类别。
 * @param output 输出路径缓冲区。
 * @param output_size 输出路径缓冲区大小。
 * @return 找到返回 0，失败返回 -1。
 */
static int find_input_device(int (*match)(int fd), const char *kind,
                             char *output, size_t output_size)
{
    int index;

    if (match == NULL || kind == NULL || output == NULL ||
        output_size == 0) {
        errno = EINVAL;
        return -1;
    }
    for (index = 0; index < INPUT_EVENT_SCAN_LIMIT; index++) {
        char path[PATH_MAX];
        int fd;

        snprintf(path, sizeof(path), "/dev/input/event%d", index);
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        if (match(fd)) {
            close(fd);
            snprintf(output, output_size, "%s", path);
            browser_log(BROWSER_LOG_INFO, "auto %s input: %s", kind,
                        output);
            return 0;
        }
        close(fd);
    }
    browser_log(BROWSER_LOG_WARN, "auto %s input not found", kind);
    errno = ENODEV;
    return -1;
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
        if (operation->owns_fd) {
            close(operation->fd);
        }
        operation->fd = -1;
        operation->owns_fd = 0;
    }
}

/**
 * @brief 打开标准输入 operation，并尽量切换为 raw nonblock 模式。
 * @param manager 输入管理器。
 * @return 成功返回标准输入 fd，失败返回 -1。
 */
static int open_stdin(struct input_manager *manager)
{
    struct termios raw;

    manager->stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (manager->stdin_flags < 0 ||
        fcntl(STDIN_FILENO, F_SETFL,
              manager->stdin_flags | O_NONBLOCK) < 0) {
        browser_log_errno(BROWSER_LOG_ERROR, "stdin");
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &manager->stdin_original) == 0) {
        raw = manager->stdin_original;
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            manager->stdin_raw_enabled = 1;
        }
    }
    browser_log(BROWSER_LOG_INFO, "input device: stdin");
    return STDIN_FILENO;
}

/**
 * @brief 恢复标准输入终端状态。
 * @param manager 输入管理器。
 */
static void restore_stdin(struct input_manager *manager)
{
    if (manager->stdin_raw_enabled) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &manager->stdin_original);
        manager->stdin_raw_enabled = 0;
    }
    if (manager->stdin_flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, manager->stdin_flags);
        manager->stdin_flags = -1;
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
 * @brief 打开可选键盘、标准输入和指针设备。
 * @param manager 输入管理器。
 * @param keyboard_path 键盘节点，"stdin" 表示标准输入，"-" 表示禁用。
 * @param touch_path 触摸或鼠标节点，NULL 或 "-" 表示禁用。
 * @param screen_width framebuffer 宽度。
 * @param screen_height framebuffer 高度。
 * @return 成功返回 0，失败返回 -1。
 */
int input_manager_open(struct input_manager *manager,
                       const char *keyboard_path, const char *touch_path,
                       int screen_width, int screen_height)
{
    char keyboard_auto_path[PATH_MAX];
    char touch_auto_path[PATH_MAX];
    const char *resolved_keyboard_path;
    const char *resolved_touch_path = touch_path;

    if (manager == NULL || keyboard_path == NULL || screen_width <= 0 ||
        screen_height <= 0) {
        errno = EINVAL;
        return -1;
    }
    memset(manager, 0, sizeof(*manager));
    manager->keyboard.fd = -1;
    manager->touch.fd = -1;
    manager->stdin_flags = -1;
    manager->screen_width = screen_width;
    manager->screen_height = screen_height;
    resolved_keyboard_path = keyboard_path;
    if (strcmp(keyboard_path, INPUT_AUTO_PATH) == 0) {
        if (find_input_device(input_device_is_keyboard, "keyboard",
                              keyboard_auto_path,
                              sizeof(keyboard_auto_path)) < 0) {
            return -1;
        }
        resolved_keyboard_path = keyboard_auto_path;
    }
    if (touch_path != NULL && strcmp(touch_path, INPUT_AUTO_PATH) == 0) {
        if (find_input_device(input_device_is_pointer, "pointer",
                              touch_auto_path, sizeof(touch_auto_path)) == 0) {
            resolved_touch_path = touch_auto_path;
        } else {
            resolved_touch_path = NULL;
        }
    }
    if (strcmp(resolved_keyboard_path, "-") != 0) {
        if (strcmp(resolved_keyboard_path, INPUT_STDIN_PATH) == 0) {
            manager->keyboard.fd = open_stdin(manager);
            manager->keyboard.name = "stdin";
            manager->keyboard.read = read_stdin;
            manager->keyboard.close = close_input_operation;
        } else {
            manager->keyboard.fd = open_input(resolved_keyboard_path);
            manager->keyboard.name = "keyboard";
            manager->keyboard.read = read_keyboard;
            manager->keyboard.close = close_input_operation;
            manager->keyboard.owns_fd = 1;
        }
        if (manager->keyboard.fd < 0) {
            return -1;
        }
        if (input_manager_register_operation(manager,
                                             &manager->keyboard) < 0) {
            input_manager_close(manager);
            return -1;
        }
    }
    if (resolved_touch_path != NULL && strcmp(resolved_touch_path, "-") != 0) {
        manager->touch.fd = open_input(resolved_touch_path);
        if (manager->touch.fd < 0) {
            input_manager_close(manager);
            return -1;
        }
        manager->touch.owns_fd = 1;
        manager->touch.close = close_input_operation;
        manager->x = screen_width / 2;
        manager->y = screen_height / 2;
        manager->raw_x = manager->x;
        manager->raw_y = manager->y;
        if (configure_axes(manager, manager->touch.fd) == 0) {
            manager->pointer_mode = INPUT_POINTER_ABSOLUTE;
            manager->touch.name = "touch";
            manager->touch.read = read_touch;
            manager->raw_x = manager->abs_x.minimum +
                             (manager->abs_x.maximum -
                              manager->abs_x.minimum) / 2;
            manager->raw_y = manager->abs_y.minimum +
                             (manager->abs_y.maximum -
                              manager->abs_y.minimum) / 2;
        } else if (configure_relative_mouse(manager->touch.fd) == 0) {
            manager->pointer_mode = INPUT_POINTER_RELATIVE;
            manager->touch.name = "mouse";
            manager->touch.read = read_mouse;
        } else {
            browser_log(BROWSER_LOG_ERROR,
                        "pointer device has no absolute or relative X/Y axes");
            input_manager_close(manager);
            errno = ENOTSUP;
            return -1;
        }
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
 * @brief 将相对鼠标坐标限制在屏幕范围内。
 * @param value 候选坐标。
 * @param extent 屏幕轴长度。
 * @return 屏幕范围内坐标。
 */
static int clamp_pointer_position(int value, int extent)
{
    if (value < 0) {
        return 0;
    }
    if (value >= extent) {
        return extent - 1;
    }
    return value;
}

/**
 * @brief 在相对鼠标 SYN_REPORT 时生成点击、移动或滑动手势。
 * @param manager 输入管理器。
 * @param output 输出事件。
 * @return 生成事件返回 1，否则返回 0。
 */
static int finish_mouse_report(struct input_manager *manager,
                               struct browser_input *output)
{
    int threshold_x = manager->screen_width / 20;
    int threshold_y = manager->screen_height / 20;

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
 * @brief 读取并聚合相对坐标鼠标事件。
 * @param manager 输入管理器。
 * @param operation 鼠标输入 operation。
 * @param output 输出归一化事件。
 * @return 生成事件返回 1，无完整事件返回 0，失败返回 -1。
 */
static int read_mouse(struct input_manager *manager,
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

        if (event->type == EV_REL && event->code == REL_X) {
            manager->x = clamp_pointer_position(manager->x + event->value,
                                                manager->screen_width);
        } else if (event->type == EV_REL && event->code == REL_Y) {
            manager->y = clamp_pointer_position(manager->y + event->value,
                                                manager->screen_height);
        } else if (event->type == EV_KEY &&
                   (event->code == BTN_LEFT || event->code == BTN_TOUCH)) {
            if (event->value != 0 && manager->touching == 0) {
                manager->touching = 2;
            } else if (event->value == 0 && manager->touching != 0) {
                manager->release_pending = 1;
            }
        } else if (event->type == EV_SYN && event->code == SYN_REPORT) {
            if (finish_mouse_report(manager, output)) {
                generated = 1;
            }
        }
    }
    return generated;
}

/**
 * @brief 读取标准输入字符并映射为浏览器动作。
 * @param manager 输入管理器。
 * @param operation 标准输入 operation。
 * @param output 输出浏览器事件。
 * @return 生成动作返回 1，无动作返回 0，失败返回 -1。
 */
static int read_stdin(struct input_manager *manager,
                      struct input_operation *operation,
                      struct browser_input *output)
{
    unsigned char buffer[INPUT_BATCH];
    ssize_t bytes = read(operation->fd, buffer, sizeof(buffer));
    ssize_t index;

    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
                      errno == EINTR)) {
        return 0;
    }
    if (bytes < 0) {
        return -1;
    }
    if (bytes == 0) {
        return 0;
    }
    for (index = 0; index < bytes; index++) {
        unsigned char value = buffer[index];
        enum input_action action = INPUT_ACTION_NONE;

        if (manager->stdin_escape_state == 1) {
            if (value == '[') {
                manager->stdin_escape_state = 2;
                continue;
            }
            manager->stdin_escape_state = 0;
            action = stdin_char_action(value);
        } else if (manager->stdin_escape_state == 2) {
            manager->stdin_escape_state = 0;
            switch (value) {
            case 'A': action = INPUT_ACTION_UP; break;
            case 'B': action = INPUT_ACTION_DOWN; break;
            case 'C': action = INPUT_ACTION_NEXT; break;
            case 'D': action = INPUT_ACTION_PREVIOUS; break;
            default: action = INPUT_ACTION_NONE; break;
            }
        } else if (value == 0x1b) {
            manager->stdin_escape_state = 1;
            continue;
        } else {
            action = stdin_char_action(value);
        }
        if (action != INPUT_ACTION_NONE) {
            output->action = action;
            return 1;
        }
    }
    return 0;
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
    restore_stdin(manager);
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
