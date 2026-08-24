#include "spi_common.h"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <spidev>\n", prog);
    fprintf(stderr, "Example: %s /dev/spidev0.0\n", prog);
}

int main(int argc, char *argv[])
{
    const char *device;
    int fd;
    uint32_t mode;
    uint8_t lsb_first;
    uint8_t bits;
    uint32_t speed_hz;

    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    fd = open_spi_device(device);
    if (fd < 0) {
        return 1;
    }

    if (ioctl(fd, SPI_IOC_RD_MODE32, &mode) < 0) {
        perror("SPI_IOC_RD_MODE32");
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb_first) < 0) {
        perror("SPI_IOC_RD_LSB_FIRST");
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0) {
        perror("SPI_IOC_RD_BITS_PER_WORD");
        close(fd);
        return 1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed_hz) < 0) {
        perror("SPI_IOC_RD_MAX_SPEED_HZ");
        close(fd);
        return 1;
    }

    printf("device     : %s\n", device);
    printf("mode       : 0x%x\n", mode);
    printf("lsb_first  : %u\n", lsb_first);
    printf("bits/word  : %u\n", bits);
    printf("max speed  : %u Hz\n", speed_hz);

    close(fd);
    return 0;
}
