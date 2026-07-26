CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := fb_info fb_draw fb_bmp 

.PHONY: all clean 

all: $(TARGET)
fb_info: fb_info.c
	$(CC) $(CFLAGS) -o $@ $^ 
fb_draw: fb_draw.c
	$(CC) $(CFLAGS) -o $@ $^
fb_bmp: fb_bmp.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
