#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

#define BMP_HEADER_SIZE 54
#define BMP_INFO_HEADER_SIZE 40
#define BMP_COMPRESSION_RGB 0

struct fb_context {
  int fd;
  uint8_t *fdb;
  size_t fdb_size;
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
};

// BMP 图片信息结构体
// 包含宽度、高度、位深、数据偏移量、行跨度和是否为自上而下的标志
struct bmp_info {
  int32_t w;
  int32_t h;
  uint16_t bpp;
  uint32_t data_off;
  uint32_t row_stride;
  int top_down;
};

/**
 * @brief 显示使用方法
 * @param prog 程序名
 */
static void usage(const char *prog) {
  printf("Usage: %s <framebuffer device> <bmp file>\n", prog);
}

/**
 * @brief 从小端字节序缓冲区读取 16 位整数。
 *
 * @param buf 指向 2 字节数据的指针。
 * @return 解析后的 16 位无符号整数。
 */
static uint16_t read_le16(const uint8_t *buf) {
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * @brief 从小端字节序缓冲区读取 32 位整数。
 *
 * @param buf 指向 4 字节数据的指针。
 * @return 解析后的 32 位无符号整数。
 */
static uint32_t read_le32(const uint8_t *buf) {
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

/**
 * @brief 从小端字节序缓冲区读取 32 位有符号整数。
 *
 * @param buf 指向 4 字节数据的指针。
 * @return 解析后的 32 位有符号整数。
 */
static int32_t read_le32_signed(const uint8_t *buf) {
  return (int32_t)read_le32(buf);
}

/**
 * @brief 循环读取指定长度的数据。
 *
 * @param fd 文件描述符。
 * @param buf 数据接收缓冲区。
 * @param len 期望读取的字节数。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_full(int fd, void *buf, size_t len) {
  uint8_t *pos = buf;
  while (len > 0) {
    ssize_t ret = read(fd, pos, len);
    if (ret < 0) {
      if (errno == EINTR)
        continue;
      perror("read");
      return -1;
    }

    if (!ret) {
      fprintf(stderr, "Unexpected EOF\n");
      return -1;
    }
    pos += ret;
    len -= ret;
  }
  return 0;
}

/**
 * @brief 按 framebuffer 通道位数缩放 8 位颜色值。
 *
 * @param val 8 位颜色值。
 * @param len framebuffer 中该通道占用的 bit 数。
 * @return 缩放后的通道值。
 */
static uint32_t scale_color(uint8_t val, uint32_t len) {
  uint32_t max;
  if (len == 0)
    return 0;

  max = (1U << len) - 1;
  return (val * max + 127) / 255;
}

/**
 * @brief 根据 framebuffer RGB 位域生成像素值。
 *
 * @param vinfo framebuffer 可变参数。
 * @param r 红色通道。
 * @param g 绿色通道。
 * @param b 蓝色通道。
 * @return framebuffer 可直接写入的像素值。
 */
static uint32_t make_pixel(const struct fb_var_screeninfo *vinfo, uint8_t r,
                           uint8_t g, uint8_t b) {
  uint32_t pixel = 0;
  pixel |= scale_color(r, vinfo->red.length) << vinfo->red.offset;
  pixel |= scale_color(g, vinfo->green.length) << vinfo->green.offset;
  pixel |= scale_color(b, vinfo->blue.length) << vinfo->blue.offset;

  if (vinfo->transp.length > 0) {
    pixel |= scale_color(255, vinfo->transp.length) << vinfo->transp.offset;
  }
  return pixel;
}

/**
 * @brief 打开 framebuffer 设备并映射显存。
 *
 * @param fb framebuffer 上下文。
 * @param fb_path framebuffer 设备路径。
 * @return 成功返回 0，失败返回 -1。
 */
static int fb_open(struct fb_context *fb, const char *fb_path) {
  memset(fb, 0, sizeof(*fb));
  fb->fd = -1;
  fb->fd = open(fb_path, O_RDWR);

  if (fb->fd < 0) {
    perror("Error opening framebuffer device");
    return -1;
  }

  if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo) < 0) {
    perror("Error getting framebuffer fixed screen info");
    close(fb->fd);
    fb->fd = -1;
    return -1;
  }

  if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) < 0) {
    perror("Error getting framebuffer variable screen info");
    close(fb->fd);
    fb->fd = -1;
    return -1;
  }

  fb->fdb_size = fb->finfo.smem_len;
  fb->fdb =
      mmap(NULL, fb->fdb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
  if (fb->fdb == MAP_FAILED) {
    perror("Error mapping framebuffer device to memory");
    close(fb->fd);
    fb->fd = -1;
    return -1;
  }
  return 0;
}

/**
 * @brief 解除显存映射并关闭 framebuffer 设备。
 *
 * @param fb framebuffer 上下文。
 */
static void fb_close(struct fb_context *fb) {
  if (fb->fdb && fb->fdb != MAP_FAILED) {
    munmap(fb->fdb, fb->fdb_size);
  }
  if (fb->fd >= 0) {
    close(fb->fd);
  }
}

/**
 * @brief 在 framebuffer 上绘制一个像素。
 *
 * @param fb framebuffer 上下文。
 * @param x 目标 X 坐标。
 * @param y 目标 Y 坐标。
 * @param r 红色通道。
 * @param g 绿色通道。
 * @param b 蓝色通道。
 */
static void fb_put_pixel(struct fb_context *fb, int x, int y, uint8_t r,
                         uint8_t g, uint8_t b) {
  size_t bpp;
  size_t offset;
  uint32_t pixel;

  if (x < 0 || y < 0) {
    return;
  }

  if ((uint32_t)x >= fb->vinfo.xres || (uint32_t)y >= fb->vinfo.yres) {
    return;
  }

  bpp = fb->vinfo.bits_per_pixel / 8;
  if (bpp != 2 && bpp != 3 && bpp != 4) {
    return;
  }

  offset = (y * fb->finfo.line_length) + (x * bpp);
  if (offset + bpp > fb->fdb_size) {
    return;
  }

  pixel = make_pixel(&fb->vinfo, r, g, b);
  memcpy(fb->fdb + offset, &pixel, bpp);
}

/**
 * @brief 使用指定颜色清空 framebuffer。
 *
 * @param fb framebuffer 上下文。
 * @param r 红色通道。
 * @param g 绿色通道。
 * @param b 蓝色通道。
 */
static void fb_clear(struct fb_context *fb, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t x, y;
  for (y = 0; y < fb->vinfo.yres; ++y) {
    for (x = 0; x < fb->vinfo.xres; ++x) {
      fb_put_pixel(fb, x, y, r, g, b);
    }
  }
}

/**
 * @brief 读取并校验 BMP 文件头。
 *
 * @param bmp_fd BMP 文件描述符。
 * @param bmp 输出的 BMP 图片信息。
 * @return 成功返回 0，失败返回 -1。
 */
static int read_bmp_header(int bmp_fd, struct bmp_info *bmp) {
  uint8_t header[BMP_HEADER_SIZE];
  uint32_t dib_size;
  uint16_t planes;
  uint32_t compression;
  uint64_t row_stride;

  if (read_full(bmp_fd, header, BMP_HEADER_SIZE) < 0) {
    return -1;
  }

  if (header[0] != 'B' || header[1] != 'M') {
    fprintf(stderr, "Not a BMP file\n");
    return -1;
  }

  bmp->data_off = read_le32(&header[10]);
  bmp->w = read_le32_signed(&header[18]);
  bmp->h = read_le32_signed(&header[22]);
  bmp->bpp = read_le16(&header[28]);
  dib_size = read_le32(&header[14]);
  planes = read_le16(&header[26]);
  compression = read_le32(&header[30]);

  if (dib_size < BMP_INFO_HEADER_SIZE || planes != 1 ||
      compression != BMP_COMPRESSION_RGB) {
    fprintf(stderr, "Unsupported BMP format\n");
    return -1;
  }

  if (bmp->w <= 0 || bmp->h == 0) {
    fprintf(stderr, "Invalid BMP size: %d x %d\n", bmp->w, bmp->h);
    return -1;
  }

  if (bmp->bpp != 24 && bmp->bpp != 32) {
    fprintf(stderr, "Unsupported BMP bpp: %u\n", bmp->bpp);
    return -1;
  }

  bmp->top_down = (bmp->h < 0);

  if (bmp->h < 0) {
    bmp->h = -bmp->h;
  }

  row_stride = (((uint64_t)bmp->w * bmp->bpp + 31) / 32) * 4;
  if (row_stride > UINT32_MAX) {
    fprintf(stderr, "BMP row is too large\n");
    return -1;
  }

  bmp->row_stride = (uint32_t)row_stride;

  return 0;
}

/**
 * @brief 计算图片在指定区域内等比例缩放后的尺寸。
 *
 * @param src_w 原图宽度。
 * @param src_h 原图高度。
 * @param max_w 最大显示宽度。
 * @param max_h 最大显示高度。
 * @param dst_w 输出的缩放后宽度。
 * @param dst_h 输出的缩放后高度。
 */
static void calc_fit_size(int src_w, int src_h, int max_w, int max_h,
                          int *dst_w, int *dst_h) {
  int fit_w;
  int fit_h;

  if (src_w <= 0 || src_h <= 0 || max_w <= 0 || max_h <= 0) {
    *dst_w = 1;
    *dst_h = 1;
    return;
  }

  fit_w = max_w;
  fit_h = (int)((int64_t)src_h * fit_w / src_w);

  if (fit_h > max_h) {
    fit_h = max_h;
    fit_w = (int)((int64_t)src_w * fit_h / src_h);
  }

  if (fit_w < 1) {
    fit_w = 1;
  }

  if (fit_h < 1) {
    fit_h = 1;
  }

  *dst_w = fit_w;
  *dst_h = fit_h;
}

/**
 * @brief 读取 BMP 指定逻辑行的数据。
 *
 * @param bmp_fd BMP 文件描述符。
 * @param bmp BMP 图片信息。
 * @param logical_y 图片逻辑 Y 坐标，从上到下递增。
 * @param row_buf 行缓冲区。
 * @return 成功返回 0，失败返回 -1。
 */
static int bmp_read_row(int bmp_fd, const struct bmp_info *bmp, int logical_y,
                        uint8_t *row_buf) {
  int file_y;
  off_t row_offset;

  file_y = bmp->top_down ? logical_y : (bmp->h - 1 - logical_y);
  row_offset = (off_t)bmp->data_off + (off_t)file_y * bmp->row_stride;

  if (lseek(bmp_fd, row_offset, SEEK_SET) < 0) {
    perror("lseek");
    return -1;
  }

  if (read_full(bmp_fd, row_buf, bmp->row_stride) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 将 BMP 图片等比例缩放后绘制到 framebuffer 中央。
 *
 * @param fb framebuffer 上下文。
 * @param bmp_fd BMP 文件描述符。
 * @param bmp BMP 图片信息。
 * @return 成功返回 0，失败返回 -1。
 */
static int draw_bmp_fit_to_fb(struct fb_context *fb, int bmp_fd,
                              const struct bmp_info *bmp) {
  uint8_t *row_buf;
  int dst_w;
  int dst_h;
  int start_x;
  int start_y;
  int dst_y;
  int bytes_per_pixel;

  // 计算适应屏幕的 BMP 尺寸
  calc_fit_size(bmp->w, bmp->h, (int)fb->vinfo.xres, (int)fb->vinfo.yres,
                &dst_w, &dst_h);

  // 计算绘制起始位置，使 BMP 居中显示
  start_x = ((int)fb->vinfo.xres - dst_w) / 2;
  start_y = ((int)fb->vinfo.yres - dst_h) / 2;

  // 计算每个像素的字节数
  bytes_per_pixel = bmp->bpp / 8;

  row_buf = malloc(bmp->row_stride);
  if (row_buf == NULL) {
    perror("malloc");
    return -1;
  }

  printf("fit size: %dx%d -> %dx%d\n", bmp->w, bmp->h, dst_w, dst_h);

  for (dst_y = 0; dst_y < dst_h; dst_y++) {
    int src_y = dst_y * bmp->h / dst_h;
    int dst_x;

    if (bmp_read_row(bmp_fd, bmp, src_y, row_buf) < 0) {
      free(row_buf);
      return -1;
    }

    for (dst_x = 0; dst_x < dst_w; dst_x++) {
      int src_x = dst_x * bmp->w / dst_w;
      const uint8_t *p = row_buf + src_x * bytes_per_pixel;

      uint8_t b = p[0];
      uint8_t g = p[1];
      uint8_t r = p[2];

      fb_put_pixel(fb, start_x + dst_x, start_y + dst_y, r, g, b);
    }
  }

  free(row_buf);
  return 0;
}

/**
 * @brief 程序入口，缩放并显示 BMP 图片。
 *
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 成功返回 EXIT_SUCCESS，失败返回 EXIT_FAILURE。
 */
int main(int argc, char *argv[]) {
  if (argc != 3) {
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  const char *fb_path = argv[1];
  const char *bmp_path = argv[2];

  struct fb_context fb;
  if (fb_open(&fb, fb_path) != 0) {
    return EXIT_FAILURE;
  }

  int bmp_fd = open(bmp_path, O_RDONLY);
  if (bmp_fd < 0) {
    perror("Error opening BMP file");
    fb_close(&fb);
    return EXIT_FAILURE;
  }

  struct bmp_info bmp;
  if (read_bmp_header(bmp_fd, &bmp) != 0) {
    close(bmp_fd);
    fb_close(&fb);
    return EXIT_FAILURE;
  }

  fb_clear(&fb, 0, 0, 0);

  if (draw_bmp_fit_to_fb(&fb, bmp_fd, &bmp) != 0) {
    close(bmp_fd);
    fb_close(&fb);
    return EXIT_FAILURE;
  }

  close(bmp_fd);
  fb_close(&fb);
  return EXIT_SUCCESS;
}
