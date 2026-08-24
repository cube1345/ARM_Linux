CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := file_basic file_copy file_stat dev_basic nonblock_fifo poll_fifo

.PHONY: all clean 

all: $(TARGET)
file_basic: file_basic.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
file_copy: file_copy.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
file_stat: file_stat.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
dev_basic: dev_basic.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
nonblock_fifo: nonblock_fifo.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
poll_fifo: poll_fifo.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
clean:
	rm -f $(TARGET)
