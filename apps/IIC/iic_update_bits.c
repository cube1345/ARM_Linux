#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <i2c-dev> <addr> <reg> <mask> <value>\n", prog);
    fprintf(stderr, "Example: %s /dev/i2c-0 0x50 0x00 0x12\n", prog);
}

int main(int argc, char *argv[])
{
    const char *device;
    int address;
    int reg;
    int mask; // 需要修改的位掩码
    int value;
    int fd;
    int old_value;
    int new_value;
    int check_value;
    int ret;

    if (argc < 6) {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    address = strtol(argv[2], NULL, 0);
    reg = strtol(argv[3], NULL, 0);
    mask = strtol(argv[4], NULL, 0);
    value = strtol(argv[5], NULL, 0);

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

    // 读取寄存器的原始值
    old_value = i2c_smbus_read_byte_data(fd, reg);
    if (old_value < 0) {
        perror("i2c_smbus_read_byte_data");
        close(fd);
        return 1;
    }

    new_value = (old_value & ~mask) | (value & mask);

    ret = i2c_smbus_write_byte_data(fd, reg, new_value);
    if (ret < 0) {
        perror("i2c_smbus_write_byte_data");
        close(fd);
        return 1;
    }

    check_value = i2c_smbus_read_byte_data(fd, reg);
    if (check_value < 0) {
        perror("i2c_smbus_read_byte_data");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}