#include "spi_common.h"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <spidev> <speed-hz> <mode> <read-len> <write-byte> [write-byte...]\n", prog);
    fprintf(stderr, "Example: %s /dev/spidev0.0 500000 0 3 0x9f\n", prog);
}

int main(int argc, char *argv[])
{
    const char *device;
    uint32_t speed_hz;
    uint32_t mode;
    uint32_t read_len;
    uint8_t tx[SPI_MAX_BYTES];
    uint8_t rx[SPI_MAX_BYTES];
    struct spi_ioc_transfer transfers[2];
    int fd;
    int write_len;
    int ret;
    int i;

    if (argc < 6) {
        usage(argv[0]);
        return 1;
    }

    device = argv[1];
    if (parse_u32(argv[2], &speed_hz) < 0 ||
        parse_u32(argv[3], &mode) < 0 ||
        parse_u32(argv[4], &read_len) < 0) {
        usage(argv[0]);
        return 1;
    }

    write_len = argc - 5;
    if (write_len > SPI_MAX_BYTES || read_len > SPI_MAX_BYTES) {
        fprintf(stderr, "too many bytes, max=%d\n", SPI_MAX_BYTES);
        return 1;
    }

    for (i = 0; i < write_len; i++) {
        if (parse_byte(argv[i + 5], &tx[i]) < 0) {
            fprintf(stderr, "invalid byte: %s\n", argv[i + 5]);
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

    memset(transfers, 0, sizeof(transfers));
    transfers[0].tx_buf = (uintptr_t)tx;
    transfers[0].len = (uint32_t)write_len;
    transfers[0].speed_hz = speed_hz;
    transfers[0].bits_per_word = 8;

    transfers[1].rx_buf = (uintptr_t)rx;
    transfers[1].len = read_len;
    transfers[1].speed_hz = speed_hz;
    transfers[1].bits_per_word = 8;

    ret = ioctl(fd, SPI_IOC_MESSAGE(2), transfers);
    if (ret < 0) {
        perror("SPI_IOC_MESSAGE");
        close(fd);
        return 1;
    }

    dump_bytes("write", tx, (size_t)write_len);
    dump_bytes("read", rx, read_len);

    close(fd);
    return 0;
}
