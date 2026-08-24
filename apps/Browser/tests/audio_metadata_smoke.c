#include "audio_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 写入一个 UTF-8 ID3v2.3 文本帧。 */
static int write_frame(FILE *stream, const char id[4], const char *value)
{
    unsigned char header[10] = {0};
    size_t size = strlen(value) + 1U;

    memcpy(header, id, 4);
    header[4] = (unsigned char)(size >> 24);
    header[5] = (unsigned char)(size >> 16);
    header[6] = (unsigned char)(size >> 8);
    header[7] = (unsigned char)size;
    if (fwrite(header, 1, sizeof(header), stream) != sizeof(header) ||
        fputc(3, stream) == EOF ||
        fwrite(value, 1, strlen(value), stream) != strlen(value)) {
        return -1;
    }
    return 0;
}

/** @brief 创建并验证合成 ID3v2.3 标签。 */
int main(int argc, char **argv)
{
    static const char title[] = "Embedded Song";
    static const char artist[] = "Media Browser";
    static const char album[] = "Framebuffer Sessions";
    struct audio_metadata metadata;
    unsigned char header[10] = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 0};
    size_t tag_size = 3U * 11U + strlen(title) + strlen(artist) +
                      strlen(album);
    FILE *stream;

    if (argc != 2 || tag_size > 0x0fffffffU) return EXIT_FAILURE;
    header[6] = (unsigned char)((tag_size >> 21) & 0x7fU);
    header[7] = (unsigned char)((tag_size >> 14) & 0x7fU);
    header[8] = (unsigned char)((tag_size >> 7) & 0x7fU);
    header[9] = (unsigned char)(tag_size & 0x7fU);
    stream = fopen(argv[1], "wb");
    if (stream == NULL) return EXIT_FAILURE;
    if (fwrite(header, 1, sizeof(header), stream) != sizeof(header) ||
        write_frame(stream, "TIT2", title) < 0 ||
        write_frame(stream, "TPE1", artist) < 0 ||
        write_frame(stream, "TALB", album) < 0) {
        fclose(stream);
        return EXIT_FAILURE;
    }
    if (fclose(stream) != 0) return EXIT_FAILURE;
    if (audio_metadata_read(argv[1], &metadata) < 0 || !metadata.has_tags ||
        strcmp(metadata.title, title) != 0 ||
        strcmp(metadata.artist, artist) != 0 ||
        strcmp(metadata.album, album) != 0) {
        fprintf(stderr, "FAIL audio metadata parser\n");
        return EXIT_FAILURE;
    }
    printf("PASS audio metadata parser\n");
    return EXIT_SUCCESS;
}
