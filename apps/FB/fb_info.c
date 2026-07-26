#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <errno.h>



static void print_var_info(const struct fb_var_screeninfo *var)
{
    printf("var screen info:\n");
    printf("  xres           : %u\n", var->xres);
    printf("  yres           : %u\n", var->yres);
    printf("  xres_virtual   : %u\n", var->xres_virtual);
    printf("  yres_virtual   : %u\n", var->yres_virtual);
    printf("  bits_per_pixel : %u\n", var->bits_per_pixel);

    printf("  red            : offset=%u length=%u\n",
            var->red.offset, var->red.length);
    printf("  green          : offset=%u length=%u\n",
            var->green.offset, var->green.length);
    printf("  blue           : offset=%u length=%u\n",
            var->blue.offset, var->blue.length);
    printf("  transp         : offset=%u length=%u\n",
            var->transp.offset, var->transp.length);
}

static void print_fix_info(const struct fb_fix_screeninfo *fix)
{
    printf("fix screen info:\n");
    printf("  id             : %s\n", fix->id);
    printf("  smem_len       : %u\n", fix->smem_len);
    printf("  line_length    : %u\n", fix->line_length);
    printf("  type           : %u\n", fix->type);
    printf("  visual         : %u\n", fix->visual);
}


int main(int argc,char *argv[])
{
    const char *fb_path = "/dev/fb0"; // Default framebuffer device
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    int fd;

    if(argc > 1)
    {
        fb_path = argv[1];
    }

    fd = open(fb_path, O_RDWR);
    if (fd == -1) {
        perror("Error opening framebuffer device");
        return EXIT_FAILURE;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        close(fd);
        return EXIT_FAILURE;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        close(fd);
        return EXIT_FAILURE;
    }

    print_fix_info(&finfo);
    print_var_info(&vinfo);

    close(fd);
    return 0;
}