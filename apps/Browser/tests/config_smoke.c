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
    expected.playback_mode = BROWSER_PLAYBACK_SHUFFLE;
    snprintf(expected.resume_path, sizeof(expected.resume_path),
             "/root/media/album/track 01.mp3");
    expected.resume_position_ms = 123456U;
    browser_path_list_add_front(&expected.recent_files,
                                "/root/media/movie.mp4");
    browser_path_list_add_front(&expected.recent_files,
                                "/root/media/song.mp3");
    browser_path_list_add_front(&expected.favorite_files,
                                "/root/media/photo.png");
    if (browser_config_save(argv[1], &expected) < 0 ||
        browser_config_load(argv[1], &loaded) < 0 ||
        loaded.font_size != expected.font_size ||
        loaded.volume != expected.volume ||
        loaded.file_sort != expected.file_sort ||
        loaded.playback_mode != expected.playback_mode ||
        strcmp(loaded.resume_path, expected.resume_path) != 0 ||
        loaded.resume_position_ms != expected.resume_position_ms ||
        loaded.recent_files.count != expected.recent_files.count ||
        strcmp(loaded.recent_files.paths[0],
               expected.recent_files.paths[0]) != 0 ||
        loaded.favorite_files.count != expected.favorite_files.count ||
        strcmp(loaded.favorite_files.paths[0],
               expected.favorite_files.paths[0]) != 0) {
        fprintf(stderr, "FAIL config round trip\n");
        return EXIT_FAILURE;
    }
    printf("PASS config round trip\n");
    return EXIT_SUCCESS;
}
