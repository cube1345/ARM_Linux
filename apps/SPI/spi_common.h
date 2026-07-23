#ifndef SPI_COMMON_H
#define SPI_COMMON_H

#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_MAX_BYTES 256
#define SPI_UNUSED __attribute__((unused))

static int SPI_UNUSED parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int SPI_UNUSED parse_byte(const char *text, uint8_t *value)
{
    uint32_t parsed;

    if (parse_u32(text, &parsed) < 0 || parsed > 0xff) {
        return -1;
    }

    *value = (uint8_t)parsed;
    return 0;
}

static int SPI_UNUSED open_spi_device(const char *device)
{
    int fd;

    fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    return fd;
}

static int SPI_UNUSED configure_spi(int fd, uint32_t mode, uint8_t bits, uint32_t speed_hz)
{
    uint32_t read_mode;
    uint8_t read_bits;
    uint32_t read_speed;

    if (ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0) {
        perror("SPI_IOC_WR_MODE32");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MODE32, &read_mode) < 0) {
        perror("SPI_IOC_RD_MODE32");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &read_bits) < 0) {
        perror("SPI_IOC_RD_BITS_PER_WORD");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &read_speed) < 0) {
        perror("SPI_IOC_RD_MAX_SPEED_HZ");
        return -1;
    }

    printf("spi config: mode=0x%x bits=%u speed=%u Hz\n",
           read_mode, read_bits, read_speed);
    return 0;
}

static void SPI_UNUSED dump_bytes(const char *label, const uint8_t *buf, size_t len)
{
    size_t i;

    printf("%s:", label);
    for (i = 0; i < len; i++) {
        if (i % 16 == 0) {
            printf("\n0x%04zx: ", i);
        }
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

#endif
