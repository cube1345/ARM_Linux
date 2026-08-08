#include "browser_log.h"
#include "input_keyboard.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/** @brief 将 pipe 中的一个字节转换为测试动作。 */
static int read_test_input(struct input_manager *manager,
                           struct input_operation *operation,
                           struct browser_input *output)
{
    unsigned char value;

    (void)manager;
    if (read(operation->fd, &value, sizeof(value)) != sizeof(value)) {
        return -1;
    }
    output->action = value == 1U ? INPUT_ACTION_OPEN : INPUT_ACTION_NONE;
    return output->action != INPUT_ACTION_NONE;
}

/** @brief 初始化仅供 poll/reconnect 测试使用的 manager。 */
static void init_test_manager(struct input_manager *manager)
{
    memset(manager, 0, sizeof(*manager));
    manager->keyboard.fd = -1;
    manager->touch.fd = -1;
    manager->stdin_flags = -1;
    manager->screen_width = 640;
    manager->screen_height = 480;
}

/** @brief 验证单个 operation 掉线不会丢失其他输入。 */
static int test_partial_disconnect(void)
{
    struct input_manager manager;
    struct browser_input event;
    int keyboard_pipe[2];
    int pointer_pipe[2];
    unsigned char value = 1U;
    int result;

    if (pipe(keyboard_pipe) < 0 || pipe(pointer_pipe) < 0) return -1;
    init_test_manager(&manager);
    snprintf(manager.keyboard_path, sizeof(manager.keyboard_path),
             "/tmp/media-browser-missing-input");
    manager.keyboard.fd = keyboard_pipe[0];
    manager.keyboard.owns_fd = 1;
    manager.keyboard.name = "keyboard";
    manager.keyboard.read = read_test_input;
    manager.keyboard.next = &manager.touch;
    manager.touch.fd = pointer_pipe[0];
    manager.touch.owns_fd = 1;
    manager.touch.name = "mouse";
    manager.touch.read = read_test_input;
    manager.operations = &manager.keyboard;
    close(keyboard_pipe[1]);
    if (write(pointer_pipe[1], &value, sizeof(value)) != sizeof(value)) {
        close(pointer_pipe[1]);
        input_manager_close(&manager);
        return -1;
    }
    result = input_manager_wait(&manager, &event, 50);
    if (result != 1 || event.action != INPUT_ACTION_OPEN ||
        manager.keyboard.fd >= 0 || manager.touch.fd < 0) {
        close(pointer_pipe[1]);
        input_manager_close(&manager);
        return -1;
    }
    close(pointer_pipe[1]);
    result = input_manager_wait(&manager, &event, 50);
    if (result != 0 || manager.touch.fd >= 0) {
        input_manager_close(&manager);
        return -1;
    }
    input_manager_close(&manager);
    return 0;
}

/** @brief 验证离线显式路径会在重试周期重新打开。 */
static int test_explicit_reconnect(const char *fifo_path)
{
    struct input_manager manager;
    struct browser_input event;
    int keepalive_fd;
    int result;

    if (mkfifo(fifo_path, 0600) < 0) return -1;
    keepalive_fd = open(fifo_path, O_RDWR | O_NONBLOCK);
    if (keepalive_fd < 0) {
        unlink(fifo_path);
        return -1;
    }
    init_test_manager(&manager);
    snprintf(manager.keyboard_path, sizeof(manager.keyboard_path), "%s",
             fifo_path);
    manager.keyboard.name = "keyboard";
    manager.keyboard.read = read_test_input;
    manager.operations = &manager.keyboard;
    result = input_manager_wait(&manager, &event, 0);
    if (result != 0 || manager.keyboard.fd < 0) {
        input_manager_close(&manager);
        close(keepalive_fd);
        unlink(fifo_path);
        return -1;
    }
    input_manager_close(&manager);
    close(keepalive_fd);
    unlink(fifo_path);
    return 0;
}

/** @brief 输入断线重连 smoke test 入口。 */
int main(int argc, char **argv)
{
    if (argc != 2) return EXIT_FAILURE;
    browser_log_set_level(BROWSER_LOG_QUIET);
    if (test_partial_disconnect() < 0 ||
        test_explicit_reconnect(argv[1]) < 0) {
        fprintf(stderr, "FAIL input reconnect\n");
        return EXIT_FAILURE;
    }
    printf("PASS input reconnect\n");
    return EXIT_SUCCESS;
}
