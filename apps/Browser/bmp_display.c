#include "bmp_display.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define BMP_HEADER_SIZE 54
#define BMP_INFO_HEADER_SIZE 40
#define BMP_COMPRESSION_RGB 0

/**
 * @brief 已解析的 BMP 图片信息。
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
            return -1;
        }

        position += bytes;
        length -= (size_t)bytes;
    }

    return 0;
}

/**
 * @brief 将 8 位颜色分量缩放到 framebuffer 位域。
 *
 * @param value 0 到 255 的颜色值。
 * @param bit_length 目标位域长度。
 * @return 缩放后的颜色值。
 */
static uint32_t scale_color(uint8_t value, uint32_t bit_length)
{
    uint64_t maximum;

    if (bit_length == 0) {
        return 0;
    }

    if (bit_length >= 32) {
        maximum = UINT32_MAX;
    } else {
        maximum = ((uint64_t)1 << bit_length) - 1;
    }

    return (uint32_t)(((uint64_t)value * maximum + 127) / 255);
}

/**
 * @brief 按 framebuffer RGB 位域生成目标像素值。
 *
 * @param info framebuffer 可变参数。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 * @return 可写入 framebuffer 的像素值。
 */
static uint32_t make_pixel(const struct fb_var_screeninfo *info,
                           uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel = 0;

    pixel |= scale_color(red, info->red.length) << info->red.offset;
    pixel |= scale_color(green, info->green.length) << info->green.offset;
    pixel |= scale_color(blue, info->blue.length) << info->blue.offset;

    if (info->transp.length > 0) {
        pixel |= scale_color(255, info->transp.length)
                 << info->transp.offset;
    }

    return pixel;
}

/**
 * @brief 向 framebuffer 写入一个像素。
 *
 * @param display 显示设备上下文。
 * @param x 目标 X 坐标。
 * @param y 目标 Y 坐标。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 */
static void put_pixel(struct bmp_display *display, int x, int y,
                      uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel;

    pixel = make_pixel(&display->variable_info, red, green, blue);
    video_buffer_put_pixel(&display->back_buffer, x, y, pixel);
}

/**
 * @brief 使用指定颜色清空整个可见画面。
 *
 * @param display 显示设备上下文。
 * @param red 红色分量。
 * @param green 绿色分量。
 * @param blue 蓝色分量。
 */
static void clear_display(struct bmp_display *display,
                          uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t pixel = make_pixel(&display->variable_info, red, green, blue);

    video_buffer_clear(&display->back_buffer, pixel);
}

/**
 * @brief 读取并验证 BMP 文件头。
 *
 * @param bmp_fd BMP 文件描述符。
 * @param bmp 输出的 BMP 信息。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_bmp_header(int bmp_fd, struct bmp_info *bmp)
{
    uint8_t header[BMP_HEADER_SIZE];
    uint32_t dib_size;
    uint16_t planes;
    uint32_t compression;
    uint64_t row_stride;

    if (read_full(bmp_fd, header, sizeof(header)) < 0) {
        return -1;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "not a BMP file\n");
        return -1;
    }

    bmp->data_offset = read_le32(&header[10]);
    bmp->width = read_le32_signed(&header[18]);
    bmp->height = read_le32_signed(&header[22]);
    bmp->bits_per_pixel = read_le16(&header[28]);
    dib_size = read_le32(&header[14]);
    planes = read_le16(&header[26]);
    compression = read_le32(&header[30]);

    if (dib_size < BMP_INFO_HEADER_SIZE || planes != 1 ||
        compression != BMP_COMPRESSION_RGB) {
        fprintf(stderr, "unsupported BMP format\n");
        return -1;
    }

    if (bmp->width <= 0 || bmp->height == 0 || bmp->height == INT32_MIN) {
        fprintf(stderr, "invalid BMP dimensions\n");
        return -1;
    }

    if (bmp->bits_per_pixel != 24 && bmp->bits_per_pixel != 32) {
        fprintf(stderr, "unsupported BMP bit depth: %u\n",
                bmp->bits_per_pixel);
        return -1;
    }

    bmp->top_down = bmp->height < 0;
    if (bmp->top_down) {
        bmp->height = -bmp->height;
    }

    row_stride = (((uint64_t)bmp->width * bmp->bits_per_pixel + 31) / 32) * 4;
    if (row_stride == 0 || row_stride > UINT32_MAX) {
        fprintf(stderr, "BMP row is too large\n");
        return -1;
    }

    bmp->row_stride = (uint32_t)row_stride;
    return 0;
}

/**
 * @brief 计算保持宽高比的最大显示尺寸。
 *
 * @param source_width 原图宽度。
 * @param source_height 原图高度。
 * @param maximum_width 最大显示宽度。
 * @param maximum_height 最大显示高度。
 * @param target_width 输出的显示宽度。
 * @param target_height 输出的显示高度。
 */
static void calculate_fit_size(int source_width, int source_height,
                               int maximum_width, int maximum_height,
                               int *target_width, int *target_height)
{
    int width = maximum_width;
    int height = (int)((int64_t)source_height * width / source_width);

    if (height > maximum_height) {
        height = maximum_height;
        width = (int)((int64_t)source_width * height / source_height);
    }

    *target_width = width > 0 ? width : 1;
    *target_height = height > 0 ? height : 1;
}

/**
 * @brief 读取 BMP 的一行像素数据。
 *
 * @param bmp_fd BMP 文件描述符。
 * @param bmp BMP 信息。
 * @param logical_y 从上向下计数的逻辑行号。
 * @param row_buffer 行数据缓冲区。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_bmp_row(int bmp_fd, const struct bmp_info *bmp,
                        int logical_y, uint8_t *row_buffer)
{
    int file_y = bmp->top_down ? logical_y : bmp->height - 1 - logical_y;
    off_t offset = (off_t)bmp->data_offset +
                   (off_t)file_y * bmp->row_stride;

    if (lseek(bmp_fd, offset, SEEK_SET) < 0) {
        perror("seek BMP row");
        return -1;
    }

    return read_full(bmp_fd, row_buffer, bmp->row_stride);
}

/**
 * @brief 缩放 BMP 并绘制到屏幕中央。
 *
 * @param display 显示设备上下文。
 * @param bmp_fd BMP 文件描述符。
 * @param bmp BMP 信息。
 * @return 成功返回 0，失败返回 -1。
 */
static int draw_bmp(struct bmp_display *display, int bmp_fd,
                    const struct bmp_info *bmp)
{
    uint8_t *row_buffer;
    int target_width;
    int target_height;
    int start_x;
    int start_y;
    int target_y;
    int source_bytes_per_pixel = bmp->bits_per_pixel / 8;

    calculate_fit_size(bmp->width, bmp->height,
                       (int)display->variable_info.xres,
                       (int)display->variable_info.yres,
                       &target_width, &target_height);

    start_x = ((int)display->variable_info.xres - target_width) / 2;
    start_y = ((int)display->variable_info.yres - target_height) / 2;

    row_buffer = malloc(bmp->row_stride);
    if (row_buffer == NULL) {
        perror("allocate BMP row");
        return -1;
    }

    clear_display(display, 0, 0, 0);

    for (target_y = 0; target_y < target_height; target_y++) {
        int source_y = (int)((int64_t)target_y * bmp->height /
                             target_height);
        int target_x;

        if (read_bmp_row(bmp_fd, bmp, source_y, row_buffer) < 0) {
            free(row_buffer);
            return -1;
        }

        for (target_x = 0; target_x < target_width; target_x++) {
            int source_x = (int)((int64_t)target_x * bmp->width /
                                 target_width);
            const uint8_t *pixel = row_buffer +
                                   source_x * source_bytes_per_pixel;

            put_pixel(display, start_x + target_x, start_y + target_y,
                      pixel[2], pixel[1], pixel[0]);
        }
    }

    if (video_buffer_flush(&display->back_buffer, display->memory,
                           display->memory_size) < 0) {
        perror("flush video buffer");
        free(row_buffer);
        return -1;
    }

    free(row_buffer);
    printf("displayed: %dx%d -> %dx%d\n", bmp->width, bmp->height,
           target_width, target_height);
    return 0;
}

/**
 * @brief 打开 framebuffer 设备并映射显存。
 *
 * @param display 显示设备上下文。
 * @param framebuffer_path framebuffer 设备节点路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_open(struct bmp_display *display,
                     const char *framebuffer_path)
{
    size_t bytes_per_pixel;

    if (display == NULL || framebuffer_path == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(display, 0, sizeof(*display));
    display->fd = -1;
    display->memory = MAP_FAILED;

    display->fd = open(framebuffer_path, O_RDWR);
    if (display->fd < 0) {
        perror("open framebuffer");
        return -1;
    }

    if (ioctl(display->fd, FBIOGET_FSCREENINFO,
              &display->fixed_info) < 0 ||
        ioctl(display->fd, FBIOGET_VSCREENINFO,
              &display->variable_info) < 0) {
        perror("get framebuffer information");
        bmp_display_close(display);
        return -1;
    }

    bytes_per_pixel = display->variable_info.bits_per_pixel / 8;
    if (bytes_per_pixel != 2 && bytes_per_pixel != 3 &&
        bytes_per_pixel != 4) {
        fprintf(stderr, "unsupported framebuffer depth: %u bpp\n",
                display->variable_info.bits_per_pixel);
        bmp_display_close(display);
        return -1;
    }

    display->memory_size = display->fixed_info.smem_len;
    if (display->memory_size == 0) {
        fprintf(stderr, "framebuffer memory size is zero\n");
        bmp_display_close(display);
        return -1;
    }

    display->memory = mmap(NULL, display->memory_size,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           display->fd, 0);
    if (display->memory == MAP_FAILED) {
        perror("map framebuffer");
        bmp_display_close(display);
        return -1;
    }

    if (video_buffer_create(&display->back_buffer,
                            display->variable_info.xres,
                            display->variable_info.yres,
                            display->variable_info.bits_per_pixel,
                            display->fixed_info.line_length) < 0) {
        perror("create video buffer");
        bmp_display_close(display);
        return -1;
    }

    printf("framebuffer: %ux%u, %u bpp\n",
           display->variable_info.xres,
           display->variable_info.yres,
           display->variable_info.bits_per_pixel);
    return 0;
}

/**
 * @brief 将 BMP 图片等比例缩放并居中显示。
 *
 * @param display 已打开的显示设备上下文。
 * @param bmp_path BMP 图片路径。
 * @return 成功返回 0，失败返回 -1。
 */
int bmp_display_show(struct bmp_display *display, const char *bmp_path)
{
    struct bmp_info bmp;
    int bmp_fd;
    int result;

    if (display == NULL || display->fd < 0 ||
        display->memory == MAP_FAILED || bmp_path == NULL) {
        errno = EINVAL;
        return -1;
    }

    bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd < 0) {
        perror(bmp_path);
        return -1;
    }

    result = read_bmp_header(bmp_fd, &bmp);
    if (result == 0) {
        result = draw_bmp(display, bmp_fd, &bmp);
    }

    if (close(bmp_fd) < 0) {
        perror("close BMP");
        if (result == 0) {
            result = -1;
        }
    }

    return result;
}

/**
 * @brief 解除显存映射并关闭 framebuffer 设备。
 *
 * @param display 显示设备上下文。
 */
void bmp_display_close(struct bmp_display *display)
{
    if (display == NULL) {
        return;
    }

    video_buffer_destroy(&display->back_buffer);

    if (display->memory != NULL && display->memory != MAP_FAILED) {
        munmap(display->memory, display->memory_size);
    }

    if (display->fd >= 0) {
        close(display->fd);
    }

    display->memory = MAP_FAILED;
    display->memory_size = 0;
    display->fd = -1;
}
