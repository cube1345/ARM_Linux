#ifndef INPUT_KEYBOARD_H
#define INPUT_KEYBOARD_H

/**
 * @brief 图片浏览器内部输入动作。
 */
enum input_action {
    INPUT_ACTION_NONE = 0,
    INPUT_ACTION_PREVIOUS,
    INPUT_ACTION_NEXT,
    INPUT_ACTION_EXIT
};

/**
 * @brief 键盘输入设备上下文。
 */
struct input_keyboard {
    int fd;
};

/**
 * @brief 打开 Linux Input 键盘设备。
 *
 * @param keyboard 键盘设备上下文。
 * @param device_path 输入设备路径。
 * @return 成功返回 0，失败返回 -1。
 */
int input_keyboard_open(struct input_keyboard *keyboard,
                        const char *device_path);

/**
 * @brief 等待一个有效的浏览器输入动作。
 *
 * @param keyboard 键盘设备上下文。
 * @param action 输出的输入动作。
 * @return 成功返回 0，失败返回 -1。
 */
int input_keyboard_wait(struct input_keyboard *keyboard,
                        enum input_action *action);

/**
 * @brief 关闭 Linux Input 键盘设备。
 *
 * @param keyboard 键盘设备上下文。
 */
void input_keyboard_close(struct input_keyboard *keyboard);

/**
 * @brief 获取输入动作的可读名称。
 *
 * @param action 输入动作。
 * @return 输入动作对应的字符串。
 */
const char *input_action_name(enum input_action action);

#endif
