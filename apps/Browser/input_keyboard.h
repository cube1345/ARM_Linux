#ifndef INPUT_KEYBOARD_H
#define INPUT_KEYBOARD_H

#include <linux/input.h>

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
    INPUT_ACTION_EXIT
};

/** @brief 触摸手势类别。 */
enum touch_action {
    TOUCH_ACTION_NONE = 0,
    TOUCH_ACTION_MOVE,
    TOUCH_ACTION_TAP,
    TOUCH_ACTION_SWIPE
};

/** @brief 一次归一化后的键盘或触摸输入。 */
struct browser_input {
    enum input_action action;
    enum touch_action touch;
    int x;
    int y;
    int start_x;
    int start_y;
    int dx;
    int dy;
};

/** @brief 键盘与绝对坐标触摸设备管理器。 */
struct input_manager {
    int keyboard_fd;
    int touch_fd;
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
};

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
                       int screen_width, int screen_height);

/**
 * @brief 等待输入或超时。
 * @param manager 输入管理器。
 * @param event 输出浏览器输入。
 * @param timeout_ms 最长等待毫秒，-1 表示无限。
 * @return 收到事件返回 1，超时返回 0，失败返回 -1。
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
