#include "subtitle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 验证 SRT sidecar 加载和按时间选择字幕。 */
int main(int argc, char **argv)
{
    struct subtitle_track track = {0};
    const char *text;

    if (argc != 3 ||
        subtitle_track_load_for_media(&track, argv[1]) < 0 ||
        track.count != 2U ||
        subtitle_track_text_at(&track, 999U) != NULL) {
        fprintf(stderr, "FAIL subtitle load\n");
        subtitle_track_close(&track);
        return EXIT_FAILURE;
    }
    text = subtitle_track_text_at(&track, 1000U);
    if (text == NULL || strcmp(text, "Hello world") != 0 ||
        subtitle_track_text_at(&track, 2500U) != NULL) {
        fprintf(stderr, "FAIL first subtitle timing\n");
        subtitle_track_close(&track);
        return EXIT_FAILURE;
    }
    text = subtitle_track_text_at(&track, 3250U);
    if (text == NULL || strcmp(text, "Second cue") != 0) {
        fprintf(stderr, "FAIL second subtitle timing\n");
        subtitle_track_close(&track);
        return EXIT_FAILURE;
    }
    subtitle_track_close(&track);
    if (subtitle_track_load_for_media(&track, argv[2]) < 0 ||
        track.count != 0U) {
        fprintf(stderr, "FAIL missing subtitle handling\n");
        subtitle_track_close(&track);
        return EXIT_FAILURE;
    }
    subtitle_track_close(&track);
    printf("PASS subtitle parser\n");
    return EXIT_SUCCESS;
}
