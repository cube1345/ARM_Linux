#include "browser_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 配置读写 smoke test。 */
int main(int argc, char **argv)
{
    struct browser_config expected;
    struct browser_config loaded;

    if (argc != 2) return EXIT_FAILURE;
    browser_config_defaults(&expected);
    expected.font_size = 34U;
    expected.volume = 37;
    expected.file_sort = FILE_LIST_SORT_SIZE;
    if (browser_config_save(argv[1], &expected) < 0 ||
        browser_config_load(argv[1], &loaded) < 0 ||
        loaded.font_size != expected.font_size ||
        loaded.volume != expected.volume ||
        loaded.file_sort != expected.file_sort) {
        fprintf(stderr, "FAIL config round trip\n");
        return EXIT_FAILURE;
    }
    printf("PASS config round trip\n");
    return EXIT_SUCCESS;
}
