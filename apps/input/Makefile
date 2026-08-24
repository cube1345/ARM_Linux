CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := input_read input_keyboard  dir_scan

.PHONY: all clean 

all: $(TARGET)
input_read: input_read.c
	$(CC) $(CFLAGS) -o $@ $^
input_keyboard: input_keyboard.c
	$(CC) $(CFLAGS) -o $@ $^
dir_scan: dir_scan.c
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f $(TARGET)
