#ifndef INPUT_KEYBOARD_H
#define INPUT_KEYBOARD_H

#include <linux/input.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <termios.h>

/** @brief 浏览器内部命令动作。 */
enum input_action {
    INPUT_ACTION_NONE = 0,
    INPUT_ACTION_PREVIOUS,
    INPUT_ACTION_NEXT,
    INPUT_ACTION_UP,
    INPUT_ACTION_DOWN,
    INPUT_ACTION_OPEN,
    INPUT_ACTION_BACK,
    INPUT_ACTION_TOGGLE,
    INPUT_ACTION_ROTATE,
    INPUT_ACTION_VOLUME_UP,
    INPUT_ACTION_VOLUME_DOWN,
    INPUT_ACTION_SEARCH,
    INPUT_ACTION_SORT,
    INPUT_ACTION_VIEW,
    INPUT_ACTION_EXIT
};

/** @brief 触摸手势类别。 */
enum touch_action {
    TOUCH_ACTION_NONE = 0,
    TOUCH_ACTION_MOVE,
    TOUCH_ACTION_TAP,
    TOUCH_ACTION_SWIPE
};

/** @brief 指针设备类型。 */
enum input_pointer_mode {
    INPUT_POINTER_NONE = 0,
    INPUT_POINTER_ABSOLUTE,
    INPUT_POINTER_RELATIVE
};

/** @brief 一次归一化后的键盘、标准输入或指针设备输入。 */
struct browser_input {
    enum input_action action;
    enum touch_action touch;
    char text[32];
    size_t text_length;
    int x;
    int y;
    int start_x;
    int start_y;
    int dx;
    int dy;
};

struct input_manager;

/** @brief 输入设备 operation 回调集合。 */
struct input_operation {
    const char *name;
    int fd;
    int owns_fd;
    int (*read)(struct input_manager *manager,
                struct input_operation *operation,
                struct browser_input *output);
    void (*close)(struct input_operation *operation);
    struct input_operation *next;
};

/** @brief 键盘、标准输入与指针设备 operation 管理器。 */
struct input_manager {
    struct input_operation *operations;
    struct input_operation keyboard;
    struct input_operation touch;
    enum input_pointer_mode pointer_mode;
    int screen_width;
    int screen_height;
    int abs_x_code;
    int abs_y_code;
    struct input_absinfo abs_x;
    struct input_absinfo abs_y;
    int raw_x;
    int raw_y;
    int x;
    int y;
    int start_x;
    int start_y;
    int last_x;
    int last_y;
    int touching;
    int release_pending;
    int stdin_flags;
    int stdin_raw_enabled;
    int stdin_escape_state;
    struct termios stdin_original;
    char keyboard_path[PATH_MAX];
    char touch_path[PATH_MAX];
    int keyboard_auto;
    int touch_auto;
    uint64_t reconnect_after_ms;
};

/**
 * @brief 打开可选键盘、标准输入和指针设备。
 * @param manager 输入管理器。
 * @param keyboard_path 键盘节点，"stdin" 表示标准输入，"auto" 表示自动查找，
 *                      "-" 表示禁用。
 * @param touch_path 触摸或鼠标节点，"auto" 表示自动查找，NULL 或 "-" 表示禁用。
 * @param screen_width framebuffer 宽度。
 * @param screen_height framebuffer 高度。
 * @return 成功返回 0，失败返回 -1。
 */
int input_manager_open(struct input_manager *manager,
                       const char *keyboard_path, const char *touch_path,
                       int screen_width, int screen_height);

/**
 * @brief 等待输入或超时。
 * @param manager 输入管理器。
 * @param event 输出浏览器输入。
 * @param timeout_ms 最长等待毫秒，-1 表示无限。
 * evdev 临时全部离线时按超时返回 0，并周期重连已配置路径。
 * @return 收到事件返回 1，超时或等待重连返回 0，失败返回 -1。
 */
int input_manager_wait(struct input_manager *manager,
                       struct browser_input *event, int timeout_ms);

/**
 * @brief 关闭全部输入设备。
 * @param manager 输入管理器。
 */
void input_manager_close(struct input_manager *manager);

/**
 * @brief 获取输入动作可读名称。
 * @param action 输入动作。
 * @return 静态字符串。
 */
const char *input_action_name(enum input_action action);

#endif
