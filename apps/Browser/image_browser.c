#include "bmp_display.h"
#include "image_list.h"
#include "input_keyboard.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief 输出程序使用方法。
 *
 * @param program_name 程序名称。
 */
static void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s <framebuffer device> <input device> <image directory>\n",
            program_name);
}

/**
 * @brief 显示列表中指定位置的图片。
 *
 * @param display 显示设备上下文。
 * @param list 图片列表。
 * @param index 图片索引。
 * @return 成功返回 0，失败返回 -1。
 */
static int show_image(struct bmp_display *display,
                      const struct image_list *list, size_t index)
{
    printf("image %zu/%zu: %s\n", index + 1, list->count,
           list->paths[index]);
    return bmp_display_show(display, list->paths[index]);
}

/**
 * @brief 运行图片浏览器输入与显示循环。
 *
 * @param display 显示设备上下文。
 * @param keyboard 输入设备上下文。
 * @param list 图片列表。
 * @return 成功退出返回 0，发生错误返回 -1。
 */
static int run_browser(struct bmp_display *display,
                       struct input_keyboard *keyboard,
                       const struct image_list *list)
{
    size_t current = 0;

    if (show_image(display, list, current) < 0) {
        return -1;
    }

    while (1) {
        enum input_action action;

        if (input_keyboard_wait(keyboard, &action) < 0) {
            return -1;
        }

        if (action == INPUT_ACTION_EXIT) {
            printf("exit\n");
            return 0;
        }

        if (action == INPUT_ACTION_NEXT) {
            current = (current + 1) % list->count;
        } else if (action == INPUT_ACTION_PREVIOUS) {
            current = (current + list->count - 1) % list->count;
        } else {
            continue;
        }

        printf("action: %s\n", input_action_name(action));
        if (show_image(display, list, current) < 0) {
            fprintf(stderr, "failed to display image; waiting for next input\n");
        }
    }
}

/**
 * @brief 图片浏览器程序入口。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[])
{
    struct image_list list;
    struct bmp_display display;
    struct input_keyboard keyboard = {.fd = -1};
    int result;

    if (argc != 4) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (image_list_scan(argv[3], &list) < 0) {
        return EXIT_FAILURE;
    }

    if (list.count == 0) {
        fprintf(stderr, "no BMP images found in: %s\n", argv[3]);
        return EXIT_FAILURE;
    }

    printf("found %zu BMP image(s)\n", list.count);

    if (bmp_display_open(&display, argv[1]) < 0) {
        return EXIT_FAILURE;
    }

    if (input_keyboard_open(&keyboard, argv[2]) < 0) {
        bmp_display_close(&display);
        return EXIT_FAILURE;
    }

    result = run_browser(&display, &keyboard, &list);

    input_keyboard_close(&keyboard);
    bmp_display_close(&display);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
