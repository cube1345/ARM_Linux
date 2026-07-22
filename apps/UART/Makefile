CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2
TARGET := uart_basic uart_loop uart_select

.PHONY: all clean 

all: $(TARGET)
uart_basic: uart_basic.c
	$(CC) $(CFLAGS) $< -o $@
uart_loop: uart_loop.c
	$(CC) $(CFLAGS) $< -o $@
uart_select: uart_select.c
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(TARGET)