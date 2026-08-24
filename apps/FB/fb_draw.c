#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>


struct fb_context 
{
    int fd; // File descriptor for the framebuffer device
    struct fb_var_screeninfo vinfo; // Variable screen information
    struct fb_fix_screeninfo finfo; // Fixed screen information
    unsigned char *fbp; // Pointer to the framebuffer memory
    size_t screensize; // Size of the framebuffer memory
};

static uint32_t scale_color(uint8_t value,uint8_t length)
{
    uint32_t max;

    if(length == 0)
        return 0;

    max = (1U << length) - 1; // 计算最大值

    return (value * max + 127) / 255; // 将8位颜色值缩放到指定长度的范围内
}


/// @brief 制作一个像素值
/// @param vinfo 
/// @param r 
/// @param g 
/// @param b 
/// @return 
static uint32_t make_pixel(const struct fb_var_screeninfo *vinfo, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pixel = 0;

    pixel |= scale_color(r, vinfo->red.length) << vinfo->red.offset;
    pixel |= scale_color(g, vinfo->green.length) << vinfo->green.offset;
    pixel |= scale_color(b, vinfo->blue.length) << vinfo->blue.offset;

    if(vinfo->transp.length > 0)
    {
        pixel |= scale_color(255, vinfo->transp.length) << vinfo->transp.offset; // 设置透明度为最大值
    }
    return pixel;
}

static int fb_open(struct fb_context *fb, const char *fb_path)
{
    memset(fb,0,sizeof(*fb));
    fb->fd = -1;
    fb->fd = open(fb_path, O_RDWR);
    if(fb->fd < 0)
    {
        perror("Error opening framebuffer device");
        return -1;
    }

    if(ioctl(fb->fd, FBIOGET_FSCREENINFO, &fb->finfo) == -1)
    {
        perror("Error reading fixed information");
        close(fb->fd);
        return -1;
    }

    if(ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->vinfo) == -1)
    {
        perror("Error reading variable information");
        close(fb->fd);
        return -1;
    }

    fb->screensize = fb->finfo.smem_len;
    fb->fbp = mmap(NULL,fb->screensize,PROT_READ | PROT_WRITE,MAP_SHARED,fb->fd,0);

    if(fb->fbp == MAP_FAILED)
    {
        perror("Error mapping framebuffer device to memory");
        close(fb->fd);
        return -1;
    }

    return 0;
}

static void fb_close(struct fb_context *fb)
{
    if(fb->fbp && fb->fbp != MAP_FAILED)
    {
        munmap(fb->fbp, fb->screensize);
    }
    if(fb->fd >= 0)
    {
        close(fb->fd);
    }
}

static void fb_put_pixel(struct fb_context *fb, int x,int y,uint8_t r,uint8_t g,uint8_t b)
{
    size_t bytes_per_pixel;
    size_t location;
    uint32_t pixel_value;

    if(x < 0 || y < 0)
    {
        return;
    }

    if((uint32_t)x >= fb->vinfo.xres || (uint32_t)y >= fb->vinfo.yres)
    {
        return;
    }

    bytes_per_pixel = fb->vinfo.bits_per_pixel / 8;
    if(bytes_per_pixel != 2 && bytes_per_pixel != 3 && bytes_per_pixel != 4)
    {
        return;
    }

    location = (y * fb->finfo.line_length) + (x * bytes_per_pixel);
    if(location + bytes_per_pixel > fb->screensize)
    {
        return;
    }

    pixel_value = make_pixel(&fb->vinfo,r,g,b);
    memcpy(fb->fbp + location, &pixel_value, bytes_per_pixel);
}

static void fb_fill_rect(struct fb_context *fb, int x,int y,int width,int height, uint8_t r,uint8_t g,uint8_t b)
{
    if(width <= 0 || height <= 0)
    {
        return;
    }

    for(int i = y; i < y + height; ++i)
    {
        if(i >= (int)fb->vinfo.yres)
        {
            break;
        }

        for(int j = x; j < x + width; ++j)
        {
            if(j >= (int)fb->vinfo.xres)
            {
                break;
            }

            fb_put_pixel(fb,j,i,r,g,b);
        }
    }
}
static void fb_clear(struct fb_context *fb, uint8_t r,uint8_t g,uint8_t b)
{
    fb_fill_rect(fb,0,0,fb->vinfo.xres,fb->vinfo.yres,r,g,b);
}

int main(int argc,char *argv[])
{
    const char *fb_path = "/dev/fb0"; // Default framebuffer device
    struct fb_context fb;

    if(argc > 1)
    {
        fb_path = argv[1];
    }

    if(fb_open(&fb, fb_path) != 0)
    {
        return EXIT_FAILURE;
    }

    printf("resolution: %ux%u\n", fb.vinfo.xres, fb.vinfo.yres);
    printf("bpp       : %u\n", fb.vinfo.bits_per_pixel);
    printf("line bytes: %u\n", fb.finfo.line_length);
    printf("fb size   : %zu\n", fb.screensize);

    // 清屏为黑色
    fb_clear(&fb, 0, 0, 0);

    // 绘制一个红色矩形
    fb_fill_rect(&fb, 100, 100, 200, 150, 255, 0, 0);

    // 绘制一个绿色矩形
    fb_fill_rect(&fb, 400, 300, 150, 200, 0, 255, 0);

    // 绘制一个蓝色矩形
    fb_fill_rect(&fb, 600, 100, 100, 300, 0, 0, 255);

    // 等待用户按下回车键后退出
    printf("Press Enter to exit...");
    getchar();

    fb_close(&fb);
    return EXIT_SUCCESS;
}
