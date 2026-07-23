CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := spi_info spi_xfer spi_wr_rd

.PHONY: all clean 

all: $(TARGET)

spi_info: spi_info.c spi_common.h
	$(CC) $(CFLAGS) -o $@ spi_info.c

spi_xfer: spi_xfer.c spi_common.h
	$(CC) $(CFLAGS) -o $@ spi_xfer.c

spi_wr_rd: spi_wr_rd.c spi_common.h
	$(CC) $(CFLAGS) -o $@ spi_wr_rd.c

clean:
	rm -f $(TARGET)
