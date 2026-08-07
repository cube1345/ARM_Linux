#include "animation_decoder.h"
#include "browser_log.h"
#include "file_list.h"
#include "gif_animation.h"
#include "image_data.h"
#include "image_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/** @brief 将文件名后缀映射为浏览器文件类型。 */
static enum file_type type_from_name(const char *name)
{
    const char *extension = strrchr(name, '.');

    if (extension == NULL) return FILE_TYPE_UNKNOWN;
    if (strcasecmp(extension, ".bmp") == 0) return FILE_TYPE_BMP;
    if (strcasecmp(extension, ".jpg") == 0 ||
        strcasecmp(extension, ".jpeg") == 0) return FILE_TYPE_JPEG;
    if (strcasecmp(extension, ".png") == 0) return FILE_TYPE_PNG;
    if (strcasecmp(extension, ".gif") == 0) return FILE_TYPE_GIF;
    if (strcasecmp(extension, ".wav") == 0) return FILE_TYPE_WAV;
    if (strcasecmp(extension, ".mp3") == 0) return FILE_TYPE_MP3;
    if (strcasecmp(extension, ".mp4") == 0) return FILE_TYPE_MP4;
    return FILE_TYPE_UNKNOWN;
}

/** @brief 检查 WAV、MP3 或 MP4 的基本容器签名。 */
static int check_media_signature(const char *path, enum file_type type)
{
    unsigned char header[64] = {0};
    FILE *stream = fopen(path, "rb");
    size_t bytes;
    size_t index;

    if (stream == NULL) return -1;
    bytes = fread(header, 1, sizeof(header), stream);
    fclose(stream);
    if (type == FILE_TYPE_WAV) {
        return bytes >= 12 && memcmp(header, "RIFF", 4) == 0 &&
               memcmp(header + 8, "WAVE", 4) == 0 ? 0 : -1;
    }
    if (type == FILE_TYPE_MP3) {
        if (bytes >= 3 && memcmp(header, "ID3", 3) == 0) return 0;
        return bytes >= 2 && header[0] == 0xffU &&
               (header[1] & 0xe0U) == 0xe0U ? 0 : -1;
    }
    if (type == FILE_TYPE_MP4) {
        for (index = 4; index + 4 <= bytes && index < 48; index++) {
            if (memcmp(header + index, "ftyp", 4) == 0) return 0;
        }
    }
    return -1;
}

/** @brief 检查一个静态图片或 GIF 文件。 */
static int check_image(const char *path, enum file_type type)
{
    struct image_data image = {0};
    struct gif_animation animation = {0};
    struct animation_decoder_manager manager;
    int result;

    if (type == FILE_TYPE_GIF) {
        animation_decoder_manager_init(&manager);
        if (animation_decoder_register_builtin(&manager) < 0 ||
            animation_decoder_manager_open(&manager, path, type,
                                           &animation) < 0 ||
            animation.frame_count == 0 ||
            gif_animation_current(&animation) == NULL) {
            gif_animation_close(&animation);
            return -1;
        }
        gif_animation_close(&animation);
        return 0;
    }
    result = image_decode(path, type, &image);
    if (result == 0 && (image.pixels == NULL || image.width == 0 ||
                        image.height == 0)) {
        result = -1;
    }
    image_data_destroy(&image);
    return result;
}

/** @brief 检查指定目录中的全部浏览器媒体文件。 */
static int check_directory(const char *directory)
{
    struct file_list list;
    size_t index;
    int result = 0;

    if (file_list_scan(directory, &list) < 0) return -1;
    for (index = 0; index < list.count; index++) {
        char path[4096];
        enum file_type type = list.entries[index].type;

        if (file_list_path(&list, index, path, sizeof(path)) < 0) {
            result = -1;
            continue;
        }
        if (type == FILE_TYPE_BMP || type == FILE_TYPE_JPEG ||
            type == FILE_TYPE_PNG || type == FILE_TYPE_GIF) {
            if (check_image(path, type) < 0) {
                fprintf(stderr, "FAIL image %s\n", path);
                result = -1;
            } else {
                printf("PASS image %s\n", path);
            }
        } else if (type == FILE_TYPE_WAV || type == FILE_TYPE_MP3 ||
                   type == FILE_TYPE_MP4) {
            if (check_media_signature(path, type) < 0) {
                fprintf(stderr, "FAIL media %s\n", path);
                result = -1;
            } else {
                printf("PASS media %s\n", path);
            }
        }
    }
    return result;
}

/** @brief 验证损坏图片和空目录的错误处理。 */
static int check_negative_cases(const char *empty_directory,
                                const char *corrupt_path)
{
    struct file_list list;
    struct image_data image = {0};
    int result = 0;

    if (file_list_scan(empty_directory, &list) < 0 || list.count != 0) {
        fprintf(stderr, "FAIL empty directory handling\n");
        result = -1;
    } else {
        printf("PASS empty directory handling\n");
    }
    if (image_decode(corrupt_path, type_from_name(corrupt_path), &image) ==
        0) {
        fprintf(stderr, "FAIL corrupt media accepted: %s\n", corrupt_path);
        result = -1;
    } else {
        printf("PASS corrupt media rejected: %s\n", corrupt_path);
    }
    image_data_destroy(&image);
    return result;
}

/** @brief 媒体解码 smoke test 入口。 */
int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <media-dir> <empty-dir> <corrupt.png>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    browser_log_init_from_env();
    if (check_directory(argv[1]) < 0 ||
        check_negative_cases(argv[2], argv[3]) < 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
