#include "audio_metadata.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID3_HEADER_SIZE 10U
#define ID3_MAX_TAG_SIZE (256U * 1024U)

/** @brief 读取 32 位大端整数。 */
static uint32_t read_be32(const unsigned char *value)
{
    return ((uint32_t)value[0] << 24) | ((uint32_t)value[1] << 16) |
           ((uint32_t)value[2] << 8) | value[3];
}

/** @brief 读取 ID3 synchsafe 32 位整数。 */
static uint32_t read_synchsafe(const unsigned char *value)
{
    return ((uint32_t)(value[0] & 0x7fU) << 21) |
           ((uint32_t)(value[1] & 0x7fU) << 14) |
           ((uint32_t)(value[2] & 0x7fU) << 7) |
           (uint32_t)(value[3] & 0x7fU);
}

/** @brief 向输出缓冲追加一个 Unicode codepoint。 */
static void append_utf8(char *output, size_t output_size, size_t *length,
                        uint32_t codepoint)
{
    unsigned char encoded[4];
    size_t count;

    if (codepoint <= 0x7fU) {
        encoded[0] = (unsigned char)codepoint;
        count = 1;
    } else if (codepoint <= 0x7ffU) {
        encoded[0] = (unsigned char)(0xc0U | (codepoint >> 6));
        encoded[1] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 2;
    } else if (codepoint <= 0xffffU) {
        encoded[0] = (unsigned char)(0xe0U | (codepoint >> 12));
        encoded[1] = (unsigned char)(0x80U | ((codepoint >> 6) & 0x3fU));
        encoded[2] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 3;
    } else {
        encoded[0] = (unsigned char)(0xf0U | (codepoint >> 18));
        encoded[1] = (unsigned char)(0x80U | ((codepoint >> 12) & 0x3fU));
        encoded[2] = (unsigned char)(0x80U | ((codepoint >> 6) & 0x3fU));
        encoded[3] = (unsigned char)(0x80U | (codepoint & 0x3fU));
        count = 4;
    }
    if (*length + count >= output_size) return;
    memcpy(output + *length, encoded, count);
    *length += count;
    output[*length] = '\0';
}

/** @brief 解码 ISO-8859-1 或 UTF-8 ID3 文本。 */
static void decode_single_byte(const unsigned char *input, size_t input_size,
                               int latin1, char *output, size_t output_size)
{
    size_t index;
    size_t length = 0;

    for (index = 0; index < input_size && input[index] != 0; index++) {
        if (latin1) {
            append_utf8(output, output_size, &length, input[index]);
        } else if (length + 1U < output_size) {
            output[length++] = (char)input[index];
            output[length] = '\0';
        }
    }
}

/** @brief 解码 UTF-16 ID3 文本。 */
static void decode_utf16(const unsigned char *input, size_t input_size,
                         int big_endian, char *output, size_t output_size)
{
    size_t offset = 0;
    size_t length = 0;

    if (input_size >= 2 && input[0] == 0xffU && input[1] == 0xfeU) {
        big_endian = 0;
        offset = 2;
    } else if (input_size >= 2 && input[0] == 0xfeU && input[1] == 0xffU) {
        big_endian = 1;
        offset = 2;
    }
    while (offset + 1U < input_size) {
        uint32_t codepoint = big_endian ?
            ((uint32_t)input[offset] << 8) | input[offset + 1U] :
            ((uint32_t)input[offset + 1U] << 8) | input[offset];

        offset += 2;
        if (codepoint == 0) break;
        if (codepoint >= 0xd800U && codepoint <= 0xdbffU &&
            offset + 1U < input_size) {
            uint32_t low = big_endian ?
                ((uint32_t)input[offset] << 8) | input[offset + 1U] :
                ((uint32_t)input[offset + 1U] << 8) | input[offset];

            if (low >= 0xdc00U && low <= 0xdfffU) {
                codepoint = 0x10000U + ((codepoint - 0xd800U) << 10) +
                            (low - 0xdc00U);
                offset += 2;
            }
        }
        append_utf8(output, output_size, &length, codepoint);
    }
}

/** @brief 解码一个 ID3 文本帧。 */
static void decode_text_frame(const unsigned char *data, size_t size,
                              char *output, size_t output_size)
{
    if (size <= 1 || output_size == 0) return;
    output[0] = '\0';
    if (data[0] == 0) {
        decode_single_byte(data + 1, size - 1U, 1, output, output_size);
    } else if (data[0] == 3) {
        decode_single_byte(data + 1, size - 1U, 0, output, output_size);
    } else if (data[0] == 1 || data[0] == 2) {
        decode_utf16(data + 1, size - 1U, data[0] == 2,
                     output, output_size);
    }
}

/** @brief 解析 ID3v2.3/v2.4 帧。 */
static void parse_id3v2(const unsigned char *data, size_t size, int version,
                        struct audio_metadata *metadata)
{
    size_t offset = 0;

    while (offset + ID3_HEADER_SIZE <= size) {
        const unsigned char *frame = data + offset;
        uint32_t frame_size;
        char *destination = NULL;

        if (frame[0] == 0) break;
        frame_size = version == 4 ? read_synchsafe(frame + 4) :
                                   read_be32(frame + 4);
        offset += ID3_HEADER_SIZE;
        if (frame_size == 0 || frame_size > size - offset) break;
        if (memcmp(frame, "TIT2", 4) == 0) destination = metadata->title;
        else if (memcmp(frame, "TPE1", 4) == 0) destination = metadata->artist;
        else if (memcmp(frame, "TALB", 4) == 0) destination = metadata->album;
        if (destination != NULL) {
            decode_text_frame(data + offset, frame_size, destination,
                              AUDIO_METADATA_TEXT_SIZE);
        }
        offset += frame_size;
    }
}

/** @brief 去掉 ID3v1 固定宽度字段尾部空格。 */
static void copy_id3v1_field(char *output, const unsigned char *input,
                             size_t size)
{
    while (size > 0 && (input[size - 1U] == 0 || input[size - 1U] == ' ')) {
        size--;
    }
    if (size >= AUDIO_METADATA_TEXT_SIZE) size = AUDIO_METADATA_TEXT_SIZE - 1U;
    memcpy(output, input, size);
    output[size] = '\0';
}

/** @brief 读取 MP3 的 ID3v2/ID3v1 文本标签。 */
int audio_metadata_read(const char *path, struct audio_metadata *metadata)
{
    unsigned char header[ID3_HEADER_SIZE];
    unsigned char id3v1[128];
    unsigned char *tag = NULL;
    FILE *stream;
    uint32_t tag_size = 0;

    if (path == NULL || metadata == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(metadata, 0, sizeof(*metadata));
    stream = fopen(path, "rb");
    if (stream == NULL) return -1;
    if (fread(header, 1, sizeof(header), stream) == sizeof(header) &&
        memcmp(header, "ID3", 3) == 0 &&
        (header[3] == 3 || header[3] == 4)) {
        tag_size = read_synchsafe(header + 6);
        if (tag_size > 0 && tag_size <= ID3_MAX_TAG_SIZE) {
            tag = malloc(tag_size);
            if (tag == NULL || fread(tag, 1, tag_size, stream) != tag_size) {
                free(tag);
                fclose(stream);
                return -1;
            }
            parse_id3v2(tag, tag_size, header[3], metadata);
            free(tag);
        }
    }
    if (fseek(stream, -128L, SEEK_END) == 0 &&
        fread(id3v1, 1, sizeof(id3v1), stream) == sizeof(id3v1) &&
        memcmp(id3v1, "TAG", 3) == 0) {
        if (metadata->title[0] == '\0')
            copy_id3v1_field(metadata->title, id3v1 + 3, 30);
        if (metadata->artist[0] == '\0')
            copy_id3v1_field(metadata->artist, id3v1 + 33, 30);
        if (metadata->album[0] == '\0')
            copy_id3v1_field(metadata->album, id3v1 + 63, 30);
    }
    metadata->has_tags = metadata->title[0] != '\0' ||
                         metadata->artist[0] != '\0' ||
                         metadata->album[0] != '\0';
    return fclose(stream) < 0 ? -1 : 0;
}
