#include "plugin_manager.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 测试桩：注册图片 decoder。 */
int image_decoder_register(struct image_decoder_manager *manager,
                           struct image_decoder *decoder)
{
    if (manager == NULL || decoder == NULL || decoder->name == NULL ||
        decoder->supports == NULL || decoder->decode == NULL) return -1;
    decoder->next = manager->head;
    manager->head = decoder;
    return 0;
}

/** @brief 测试桩：注册音频 backend。 */
int audio_backend_register(struct audio_backend_manager *manager,
                           struct audio_backend_operation *backend)
{
    if (manager == NULL || backend == NULL || backend->name == NULL ||
        backend->supports == NULL || backend->play == NULL) return -1;
    backend->next = manager->head;
    manager->head = backend;
    manager->count++;
    return 0;
}

/** @brief 测试桩：注册页面 operation。 */
int page_manager_register(struct page_manager *manager,
                          struct page_operation *page)
{
    if (manager == NULL || page == NULL || page->name == NULL ||
        page->render == NULL) return -1;
    page->next = manager->head;
    manager->head = page;
    return 0;
}

/** @brief 测试桩：注册桌面应用 operation。 */
int desktop_app_register(struct desktop_app_manager *manager,
                         struct desktop_app_operation *application)
{
    if (manager == NULL || application == NULL || application->name == NULL ||
        application->summary == NULL || application->badge == NULL ||
        application->launch == NULL) return -1;
    application->next = NULL;
    if (manager->tail == NULL) manager->head = application;
    else manager->tail->next = application;
    manager->tail = application;
    manager->count++;
    return 0;
}

/** @brief 测试桩：注册显示 backend。 */
int display_manager_register(struct display_manager *manager,
                             struct display_operation *display)
{
    if (manager == NULL || display == NULL || display->name == NULL ||
        display->open == NULL || display->close == NULL) return -1;
    display->next = NULL;
    if (manager->tail == NULL) manager->head = display;
    else manager->tail->next = display;
    manager->tail = display;
    return 0;
}

/** @brief 判断插件 shutdown marker 是否存在。 */
static int marker_exists(const char *path)
{
    FILE *file = fopen(path, "r");

    if (file == NULL) return 0;
    fclose(file);
    return 1;
}

/** @brief 验证动态插件加载、失败保留和逆序清理语义。 */
int main(int argc, char **argv)
{
    struct image_decoder_manager images = {0};
    struct audio_backend_manager audio = {0};
    struct page_manager pages = {0};
    struct desktop_app_manager desktop = {0};
    struct display_manager displays = {0};
    struct browser_plugin_host host;
    struct browser_plugin_manager manager;
    struct browser_plugin_manager missing;
    struct browser_plugin_manager failing_manager;
    struct image_decoder_manager failing_images = {0};
    struct audio_backend_manager failing_audio = {0};
    struct page_manager failing_pages = {0};
    struct desktop_app_manager failing_desktop = {0};
    struct display_manager failing_displays = {0};
    struct browser_plugin_host failing_host;
    char missing_path[PATH_MAX];

    if (argc != 4 || setenv("BROWSER_PLUGIN_SHUTDOWN_MARKER", argv[2], 1) < 0) {
        fprintf(stderr, "usage: %s <plugin-dir> <marker> <failing-dir>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    file_list_clear_registered_extensions();
    browser_plugin_host_init(&host, &images, &audio, &pages, &desktop,
                             &displays);
    browser_plugin_manager_init(&missing);
    if (snprintf(missing_path, sizeof(missing_path), "%s/missing", argv[1]) >=
            (int)sizeof(missing_path) ||
        browser_plugin_manager_load(&missing, missing_path, &host) < 0) {
        fprintf(stderr, "FAIL missing plugin directory\n");
        return EXIT_FAILURE;
    }
    browser_plugin_manager_destroy(&missing);

    browser_plugin_manager_init(&manager);
    if (browser_plugin_manager_load(&manager, argv[1], &host) < 0 ||
        manager.count != 1U ||
        strcmp(manager.items[0].descriptor.name, "fixture") != 0 ||
        images.head == NULL || strcmp(images.head->name, "fixture-image") != 0 ||
        audio.head == NULL || audio.count != 1U ||
        strcmp(audio.head->name, "fixture-audio") != 0 ||
        pages.head == NULL || strcmp(pages.head->name, "fixture-page") != 0 ||
        desktop.head == NULL || desktop.count != 1U ||
        strcmp(desktop.head->name, "Fixture") != 0 ||
        displays.head == NULL ||
        strcmp(displays.head->name, "fixture-display") != 0 ||
        file_list_detect_type("sample.PLUGIMG") != FILE_TYPE_PLUGIN_IMAGE ||
        file_list_detect_type("sample.plugaud") != FILE_TYPE_PLUGIN_AUDIO) {
        fprintf(stderr, "FAIL plugin registration\n");
        browser_plugin_manager_destroy(&manager);
        return EXIT_FAILURE;
    }
    errno = 0;
    if (file_list_register_extension(".plugimg", FILE_TYPE_PLUGIN_IMAGE) == 0 ||
        errno != EEXIST) {
        fprintf(stderr, "FAIL duplicate plugin extension\n");
        browser_plugin_manager_destroy(&manager);
        return EXIT_FAILURE;
    }
    browser_plugin_manager_destroy(&manager);
    if (!marker_exists(argv[2]) ||
        file_list_detect_type("sample.plugimg") != FILE_TYPE_UNKNOWN) {
        fprintf(stderr, "FAIL plugin shutdown\n");
        return EXIT_FAILURE;
    }
    if (remove(argv[2]) < 0) {
        fprintf(stderr, "FAIL reset shutdown marker\n");
        return EXIT_FAILURE;
    }

    file_list_clear_registered_extensions();
    browser_plugin_host_init(&failing_host, &failing_images, &failing_audio,
                             &failing_pages, &failing_desktop,
                             &failing_displays);
    browser_plugin_manager_init(&failing_manager);
    errno = 0;
    if (browser_plugin_manager_load(&failing_manager, argv[3],
                                    &failing_host) == 0 ||
        errno != EPROTO || failing_manager.count != 1U ||
        failing_images.head == NULL ||
        strcmp(failing_images.head->name, "failing-image") != 0 ||
        !failing_images.head->supports("sample.failimg",
                                       FILE_TYPE_PLUGIN_IMAGE) ||
        file_list_detect_type("sample.failimg") != FILE_TYPE_PLUGIN_IMAGE) {
        fprintf(stderr, "FAIL failed-plugin retention\n");
        browser_plugin_manager_destroy(&failing_manager);
        return EXIT_FAILURE;
    }
    browser_plugin_manager_destroy(&failing_manager);
    if (!marker_exists(argv[2]) ||
        file_list_detect_type("sample.failimg") != FILE_TYPE_UNKNOWN) {
        fprintf(stderr, "FAIL failed-plugin shutdown\n");
        return EXIT_FAILURE;
    }
    printf("PASS dynamic plugin manager\n");
    return EXIT_SUCCESS;
}
