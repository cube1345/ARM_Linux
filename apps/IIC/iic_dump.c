#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Uasge: %s <iic device> <iic address> <start-reg> <len>\n", prog);
}       

int main(int argc,char *argv[])
{
    const char *device;
    int address;
    int start_reg;
    int len;
    int fd;

    if(argc < 5)
    {
        usage(argv[0]);
        return -1;
    }

    device = argv[1];

    // strol 可以解析十进制、十六进制
    address = strtol(argv[2], NULL, 0);
    start_reg = strtol(argv[3], NULL, 0);
    len = strtol(argv[4], NULL, 0);


    // 打开iic设备
    fd = open(device, O_RDWR);
    if(fd < 0)
    {
        perror("open iic device failed");
        return -1;
    }

    // 设置IIC地址
    if(ioctl(fd,I2C_SLAVE,address) < 0)
    {
        perror("ioctl I2C_SLAVE failed");
        close(fd);
        return -1;
    }

    for(int i = 0; i < len; i++)
    {
        int reg;
        int value;

        reg = start_reg + i;
        
        if(reg > 0xff)
        {
            fprintf(stderr, "reg 0x%x out of range\n", reg);
            close(fd);
            return -1;
        }

        value = i2c_smbus_read_byte_data(fd, reg);
        if(value < 0)
        {
            perror("i2c_smbus_read_byte_data failed");
            close(fd);
            return -1;
        }

        if(i % 16 == 0)
        {
            printf("\n0x%02x: ", reg);
        }
        printf("%02x ", value & 0xff);
    }
    printf("\n");

    close(fd);
    return 0;   
}
