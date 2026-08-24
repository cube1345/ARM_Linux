#include "spi_common.h"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <spidev> <speed-hz> <mode> <tx-byte> [tx-byte...]\n", prog);
    fprintf(stderr, "Example: %s /dev/spidev0.0 500000 0 0x9f 0x00 0x00 0x00\n", prog);
}

// SPI设备   SPI频率   SPI模式   发送数据  
int main(int argc, char *argv[])
{
    const char *device;
    uint32_t speed_hz;
    uint32_t mode;
    uint8_t tx[SPI_MAX_BYTES];
    uint8_t rx[SPI_MAX_BYTES];
    struct spi_ioc_transfer transfer;
    int fd;
    int count;
    int ret;
    int i;

    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    if (parse_u32(argv[2], &speed_hz) < 0 || parse_u32(argv[3], &mode) < 0) {
        usage(argv[0]);
        return 1;
    }

    
    count = argc - 4;
    if (count > SPI_MAX_BYTES) {
        fprintf(stderr, "too many bytes, max=%d\n", SPI_MAX_BYTES);
        return 1;
    }

    for (i = 0; i < count; i++) {
        if (parse_byte(argv[i + 4], &tx[i]) < 0) {
            fprintf(stderr, "invalid byte: %s\n", argv[i + 4]);
            return 1;
        }
    }
    memset(rx, 0, sizeof(rx));

    fd = open_spi_device(device);
    if (fd < 0) {
        return 1;
    }

    if (configure_spi(fd, mode, 8, speed_hz) < 0) {
        close(fd);
        return 1;
    }

    memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = (uintptr_t)tx;
    transfer.rx_buf = (uintptr_t)rx;
    transfer.len = (uint32_t)count;
    transfer.speed_hz = speed_hz;
    transfer.bits_per_word = 8;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
    if (ret < 0) {
        perror("SPI_IOC_MESSAGE");
        close(fd);
        return 1;
    }

    dump_bytes("tx", tx, (size_t)count);
    dump_bytes("rx", rx, (size_t)count);

    close(fd);
    return 0;
}
