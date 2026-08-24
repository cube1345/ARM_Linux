CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := iic_basic iic_dump iic_update_bits

.PHONY: all clean 

all: $(TARGET)

iic_basic: iic_basic.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c
iic_dump: iic_dump.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c
iic_update_bits: iic_update_bits.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c
clean:
	rm -f $(TARGET)