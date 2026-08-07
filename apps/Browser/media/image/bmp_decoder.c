#include "bmp_decoder.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BMP_RGB 0U
#define BMP_RLE8 1U
#define BMP_RLE4 2U

/** @brief 已验证的 BMP 元数据。 */
struct bmp_info {
    int32_t width;
    int32_t height;
    uint16_t bpp;
    uint32_t compression;
    uint32_t data_offset;
    uint32_t palette_offset;
    uint32_t palette_count;
    uint32_t row_stride;
    int top_down;
};

/**
 * @brief 读取小端 16 位整数。
 * @param data 输入字节。
 * @return 解析值。
 */
static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

/**
 * @brief 读取小端 32 位整数。
 * @param data 输入字节。
 * @return 解析值。
 */
static uint32_t le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

/**
 * @brief 将整个 BMP 文件读入内存。
 * @param path 文件路径。
 * @param data 输出数据指针。
 * @param size 输出文件大小。
 * @return 成功返回 0，失败返回 -1。
 */
static int load_file(const char *path, uint8_t **data, size_t *size)
{
    struct stat status;
    uint8_t *buffer;
    size_t done = 0;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0 || fstat(fd, &status) < 0 || status.st_size < 54) {
        if (fd >= 0) {
            close(fd);
        }
        errno = EINVAL;
        return -1;
    }
    if ((uintmax_t)status.st_size > SIZE_MAX) {
        close(fd);
        errno = EOVERFLOW;
        return -1;
    }
    *size = (size_t)status.st_size;
    buffer = malloc(*size);
    if (buffer == NULL) {
        close(fd);
        return -1;
    }
    while (done < *size) {
        ssize_t count = read(fd, buffer + done, *size - done);

        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            free(buffer);
            close(fd);
            errno = EIO;
            return -1;
        }
        done += (size_t)count;
    }
    close(fd);
    *data = buffer;
    return 0;
}

/**
 * @brief 解析并验证 BMP 头。
 * @param data 文件数据。
 * @param size 文件大小。
 * @param info 输出元数据。
 * @return 成功返回 0，失败返回 -1。
 */
static int parse_header(const uint8_t *data, size_t size,
                        struct bmp_info *info)
{
    uint32_t dib_size;
    uint32_t colors_used;
    uint64_t stride;
    uint64_t palette_end;

    memset(info, 0, sizeof(*info));
    if (data[0] != 'B' || data[1] != 'M') {
        errno = EINVAL;
        return -1;
    }
    info->data_offset = le32(data + 10);
    dib_size = le32(data + 14);
    if (dib_size < 40 || (uint64_t)14 + dib_size > size) {
        errno = ENOTSUP;
        return -1;
    }
    info->width = (int32_t)le32(data + 18);
    info->height = (int32_t)le32(data + 22);
    info->bpp = le16(data + 28);
    info->compression = le32(data + 30);
    colors_used = le32(data + 46);
    if (le16(data + 26) != 1 || info->width <= 0 || info->height == 0 ||
        info->height == INT32_MIN || info->data_offset > size) {
        errno = EINVAL;
        return -1;
    }
    info->top_down = info->height < 0;
    if (info->top_down) {
        info->height = -info->height;
    }
    if (!((info->compression == BMP_RGB &&
           (info->bpp == 4 || info->bpp == 8 || info->bpp == 24 ||
            info->bpp == 32)) ||
          (info->compression == BMP_RLE8 && info->bpp == 8) ||
          (info->compression == BMP_RLE4 && info->bpp == 4)) ||
        (info->top_down && info->compression != BMP_RGB)) {
        errno = ENOTSUP;
        return -1;
    }
    info->palette_offset = 14U + dib_size;
    if (info->bpp <= 8) {
        info->palette_count = colors_used != 0 ? colors_used :
                              (1U << info->bpp);
        if (info->palette_count > (1U << info->bpp)) {
            errno = EINVAL;
            return -1;
        }
        palette_end = (uint64_t)info->palette_offset +
                      (uint64_t)info->palette_count * 4U;
        if (palette_end > info->data_offset || palette_end > size) {
            errno = EINVAL;
            return -1;
        }
    }
    stride = (((uint64_t)info->width * info->bpp + 31U) / 32U) * 4U;
    if (stride == 0 || stride > UINT32_MAX ||
        (info->compression == BMP_RGB &&
         (uint64_t)info->data_offset + stride * (uint32_t)info->height > size)) {
        errno = EINVAL;
        return -1;
    }
    info->row_stride = (uint32_t)stride;
    return 0;
}

/**
 * @brief 将调色板索引写入 RGB888 图片。
 * @param image 目标图片。
 * @param info BMP 元数据。
 * @param palette BGRA 调色板。
 * @param x 文件坐标 X。
 * @param file_y 从底向上计数的文件坐标 Y。
 * @param index 调色板索引。
 * @return 成功返回 0，越界返回 -1。
 */
static int put_index(struct image_data *image, const struct bmp_info *info,
                     const uint8_t *palette, int x, int file_y,
                     unsigned int index)
{
    int logical_y;
    uint8_t *pixel;

    if (x < 0 || x >= info->width || file_y < 0 || file_y >= info->height ||
        index >= info->palette_count) {
        errno = EINVAL;
        return -1;
    }
    logical_y = info->height - 1 - file_y;
    pixel = image->pixels + (size_t)logical_y * image->line_length +
            (size_t)x * 3U;
    pixel[0] = palette[index * 4U + 2U];
    pixel[1] = palette[index * 4U + 1U];
    pixel[2] = palette[index * 4U];
    return 0;
}

/**
 * @brief 解码未压缩 BMP 像素。
 * @param data 文件数据。
 * @param info BMP 元数据。
 * @param image 输出图片。
 * @return 成功返回 0，失败返回 -1。
 */
static int decode_rgb(const uint8_t *data, const struct bmp_info *info,
                      struct image_data *image)
{
    const uint8_t *palette = data + info->palette_offset;
    int y;

    for (y = 0; y < info->height; y++) {
        int file_y = info->top_down ? y : info->height - 1 - y;
        const uint8_t *row = data + info->data_offset +
                             (size_t)file_y * info->row_stride;
        uint8_t *destination = image->pixels +
                               (size_t)y * image->line_length;
        int x;

        for (x = 0; x < info->width; x++) {
            if (info->bpp == 24 || info->bpp == 32) {
                size_t bytes = info->bpp / 8U;
                const uint8_t *source = row + (size_t)x * bytes;

                destination[(size_t)x * 3U] = source[2];
                destination[(size_t)x * 3U + 1U] = source[1];
                destination[(size_t)x * 3U + 2U] = source[0];
            } else {
                unsigned int index = info->bpp == 8 ? row[x] :
                    ((x & 1) == 0 ? row[x / 2] >> 4 : row[x / 2] & 0x0fU);
                const uint8_t *color;

                if (index >= info->palette_count) {
                    errno = EINVAL;
                    return -1;
                }
                color = palette + index * 4U;
                destination[(size_t)x * 3U] = color[2];
                destination[(size_t)x * 3U + 1U] = color[1];
                destination[(size_t)x * 3U + 2U] = color[0];
            }
        }
    }
    return 0;
}

/**
 * @brief 解码一个 RLE 绝对模式数据段。
 * @param cursor 当前输入位置，成功后前移。
 * @param end 输入末尾。
 * @param count 像素数量。
 * @param info BMP 元数据。
 * @param image 输出图片。
 * @param x 当前 X，成功后前移。
 * @param y 当前文件 Y。
 * @param palette BGRA 调色板。
 * @return 成功返回 0，失败返回 -1。
 */
static int decode_absolute(const uint8_t **cursor, const uint8_t *end,
                           unsigned int count, const struct bmp_info *info,
                           struct image_data *image, int *x, int y,
                           const uint8_t *palette)
{
    size_t packed = info->bpp == 8 ? count : (count + 1U) / 2U;
    size_t stored = (packed + 1U) & ~(size_t)1U;
    unsigned int i;

    if ((size_t)(end - *cursor) < stored) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < count; i++) {
        unsigned int index = info->bpp == 8 ? (*cursor)[i] :
            ((i & 1U) == 0 ? (*cursor)[i / 2U] >> 4 :
             (*cursor)[i / 2U] & 0x0fU);

        if (put_index(image, info, palette, *x, y, index) < 0) {
            return -1;
        }
        (*x)++;
    }
    *cursor += stored;
    return 0;
}

/**
 * @brief 解码 BI_RLE4 或 BI_RLE8 像素流。
 * @param data 文件数据。
 * @param size 文件大小。
 * @param info BMP 元数据。
 * @param image 输出图片。
 * @return 成功返回 0，失败返回 -1。
 */
static int decode_rle(const uint8_t *data, size_t size,
                      const struct bmp_info *info, struct image_data *image)
{
    const uint8_t *cursor = data + info->data_offset;
    const uint8_t *end = data + size;
    const uint8_t *palette = data + info->palette_offset;
    int x = 0;
    int y = 0;

    while ((size_t)(end - cursor) >= 2U) {
        unsigned int count = *cursor++;
        unsigned int value = *cursor++;

        if (count != 0) {
            unsigned int i;

            for (i = 0; i < count; i++) {
                unsigned int index = info->bpp == 8 ? value :
                    ((i & 1U) == 0 ? value >> 4 : value & 0x0fU);

                if (put_index(image, info, palette, x, y, index) < 0) {
                    return -1;
                }
                x++;
            }
        } else if (value == 0) {
            x = 0;
            y++;
            if (y > info->height) {
                errno = EINVAL;
                return -1;
            }
        } else if (value == 1) {
            return 0;
        } else if (value == 2) {
            if ((size_t)(end - cursor) < 2U) {
                break;
            }
            x += *cursor++;
            y += *cursor++;
            if (x > info->width || y >= info->height) {
                errno = EINVAL;
                return -1;
            }
        } else if (decode_absolute(&cursor, end, value, info, image,
                                   &x, y, palette) < 0) {
            return -1;
        }
    }
    errno = EINVAL;
    return -1;
}

/**
 * @brief 将 BMP 文件解码成顶行优先的 RGB888 图片。
 *
 * @param path BMP 文件路径。
 * @param image 输出图片，调用前必须清零。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_decode(const char *path, struct image_data *image)
{
    struct bmp_info info;
    uint8_t *data = NULL;
    size_t size = 0;
    int result;

    if (path == NULL || image == NULL || image->pixels != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (load_file(path, &data, &size) < 0 ||
        parse_header(data, size, &info) < 0 ||
        image_data_create(image, (uint32_t)info.width,
                          (uint32_t)info.height) < 0) {
        free(data);
        return -1;
    }
    memset(image->pixels, 0, image->size);
    result = info.compression == BMP_RGB ?
             decode_rgb(data, &info, image) :
             decode_rle(data, size, &info, image);
    free(data);
    if (result < 0) {
        image_data_destroy(image);
    }
    return result;
}
