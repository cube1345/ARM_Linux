#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <i2c-dev> <addr> <reg> <value>\n", prog);
    fprintf(stderr, "Example: %s /dev/i2c-0 0x50 0x00 0x12\n", prog);
}

int main(int argc, char *argv[])
{
    const char *device;
    int address;
    int reg;
    int value;
    int fd;
    int ret;

    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    address = strtol(argv[2], NULL, 0);
    reg = strtol(argv[3], NULL, 0);
    value = strtol(argv[4], NULL, 0);

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, address) < 0) {
        perror("ioctl I2C_SLAVE");
        close(fd);
        return 1;
    }

    ret = i2c_smbus_write_byte_data(fd, reg, value);
    if (ret < 0) {
        perror("i2c_smbus_write_byte_data");
        close(fd);
        return 1;
    }

    printf("write: device=%s addr=0x%02x reg=0x%02x value=0x%02x\n",
            device, address, reg, value);

    ret = i2c_smbus_read_byte_data(fd, reg);
    if (ret < 0) {
        perror("i2c_smbus_read_byte_data");
        close(fd);
        return 1;
    }

    printf("read : device=%s addr=0x%02x reg=0x%02x value=0x%02x\n",
            device, address, reg, ret & 0xff);

    close(fd);
    return 0;
}