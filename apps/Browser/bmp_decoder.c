#include "bmp_decoder.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BMP_HEADER_SIZE 54
#define BMP_INFO_HEADER_SIZE 40
#define BMP_COMPRESSION_RGB 0

/**
 * @brief BMP 文件中解析出的图片信息。
 */
struct bmp_info {
    int32_t width;
    int32_t height;
    uint16_t bits_per_pixel;
    uint32_t data_offset;
    uint32_t row_stride;
    int top_down;
};

/**
 * @brief 从小端字节流读取 16 位无符号整数。
 *
 * @param buffer 至少包含 2 字节的缓冲区。
 * @return 解析后的整数。
 */
static uint16_t read_le16(const uint8_t *buffer)
{
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

/**
 * @brief 从小端字节流读取 32 位无符号整数。
 *
 * @param buffer 至少包含 4 字节的缓冲区。
 * @return 解析后的整数。
 */
static uint32_t read_le32(const uint8_t *buffer)
{
    return (uint32_t)buffer[0] |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

/**
 * @brief 从小端字节流读取 32 位有符号整数。
 *
 * @param buffer 至少包含 4 字节的缓冲区。
 * @return 解析后的整数。
 */
static int32_t read_le32_signed(const uint8_t *buffer)
{
    return (int32_t)read_le32(buffer);
}

/**
 * @brief 完整读取指定数量的字节。
 *
 * @param fd 文件描述符。
 * @param buffer 接收数据的缓冲区。
 * @param length 需要读取的字节数。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_full(int fd, void *buffer, size_t length)
{
    uint8_t *position = buffer;

    while (length > 0) {
        ssize_t bytes = read(fd, position, length);

        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read BMP");
            return -1;
        }

        if (bytes == 0) {
            fprintf(stderr, "unexpected end of BMP file\n");
            errno = EIO;
            return -1;
        }

        position += bytes;
        length -= (size_t)bytes;
    }

    return 0;
}

/**
 * @brief 读取并验证 BMP 文件头。
 *
 * @param fd BMP 文件描述符。
 * @param bmp 输出的 BMP 信息。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_bmp_header(int fd, struct bmp_info *bmp)
{
    uint8_t header[BMP_HEADER_SIZE];
    uint32_t dib_size;
    uint16_t planes;
    uint32_t compression;
    uint64_t minimum_data_offset;
    uint64_t row_stride;

    if (read_full(fd, header, sizeof(header)) < 0) {
        return -1;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "not a BMP file\n");
        errno = EINVAL;
        return -1;
    }

    bmp->data_offset = read_le32(&header[10]);
    dib_size = read_le32(&header[14]);
    bmp->width = read_le32_signed(&header[18]);
    bmp->height = read_le32_signed(&header[22]);
    planes = read_le16(&header[26]);
    bmp->bits_per_pixel = read_le16(&header[28]);
    compression = read_le32(&header[30]);
    minimum_data_offset = 14ULL + dib_size;

    if (dib_size < BMP_INFO_HEADER_SIZE ||
        bmp->data_offset < minimum_data_offset || planes != 1 ||
        compression != BMP_COMPRESSION_RGB) {
        fprintf(stderr, "unsupported BMP format\n");
        errno = ENOTSUP;
        return -1;
    }

    if (bmp->width <= 0 || bmp->height == 0 ||
        bmp->height == INT32_MIN) {
        fprintf(stderr, "invalid BMP dimensions\n");
        errno = EINVAL;
        return -1;
    }

    if (bmp->bits_per_pixel != 24 && bmp->bits_per_pixel != 32) {
        fprintf(stderr, "unsupported BMP bit depth: %u\n",
                bmp->bits_per_pixel);
        errno = ENOTSUP;
        return -1;
    }

    bmp->top_down = bmp->height < 0;
    if (bmp->top_down) {
        bmp->height = -bmp->height;
    }

    row_stride = (((uint64_t)bmp->width * bmp->bits_per_pixel + 31) / 32) * 4;
    if (row_stride == 0 || row_stride > UINT32_MAX) {
        fprintf(stderr, "BMP row is too large\n");
        errno = EOVERFLOW;
        return -1;
    }

    bmp->row_stride = (uint32_t)row_stride;
    return 0;
}

/**
 * @brief 读取 BMP 的一行像素数据。
 *
 * @param fd BMP 文件描述符。
 * @param bmp BMP 图片信息。
 * @param logical_y 从上向下计数的逻辑行号。
 * @param row_buffer 行数据缓冲区。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_bmp_row(int fd, const struct bmp_info *bmp,
                        int logical_y, uint8_t *row_buffer)
{
    int file_y = bmp->top_down ? logical_y :
                 bmp->height - 1 - logical_y;
    off_t offset = (off_t)bmp->data_offset +
                   (off_t)file_y * bmp->row_stride;

    if (lseek(fd, offset, SEEK_SET) < 0) {
        perror("seek BMP row");
        return -1;
    }

    return read_full(fd, row_buffer, bmp->row_stride);
}

/**
 * @brief 将 BMP 行的 BGR/BGRA 像素转换成 RGB888。
 *
 * @param bmp BMP 图片信息。
 * @param source BMP 原始行数据。
 * @param destination RGB888 目标行。
 */
static void convert_bmp_row(const struct bmp_info *bmp,
                            const uint8_t *source, uint8_t *destination)
{
    int source_bytes_per_pixel = bmp->bits_per_pixel / 8;
    int x;

    for (x = 0; x < bmp->width; x++) {
        const uint8_t *source_pixel = source +
                                      (size_t)x * source_bytes_per_pixel;
        uint8_t *destination_pixel = destination + (size_t)x * 3;

        destination_pixel[0] = source_pixel[2];
        destination_pixel[1] = source_pixel[1];
        destination_pixel[2] = source_pixel[0];
    }
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
    struct bmp_info bmp;
    uint8_t *row_buffer = NULL;
    int fd = -1;
    int result = -1;
    int y;

    if (path == NULL || image == NULL || image->pixels != NULL) {
        errno = EINVAL;
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    if (read_bmp_header(fd, &bmp) < 0) {
        goto cleanup;
    }

    if (image_data_create(image, (uint32_t)bmp.width,
                          (uint32_t)bmp.height) < 0) {
        perror("create decoded image");
        goto cleanup;
    }

    row_buffer = malloc(bmp.row_stride);
    if (row_buffer == NULL) {
        perror("allocate BMP row");
        goto cleanup;
    }

    for (y = 0; y < bmp.height; y++) {
        uint8_t *destination = image->pixels +
                               (size_t)y * image->line_length;

        if (read_bmp_row(fd, &bmp, y, row_buffer) < 0) {
            goto cleanup;
        }

        convert_bmp_row(&bmp, row_buffer, destination);
    }

    result = 0;

cleanup:
    free(row_buffer);

    if (close(fd) < 0) {
        perror("close BMP");
        result = -1;
    }

    if (result < 0) {
        image_data_destroy(image);
    }

    return result;
}
