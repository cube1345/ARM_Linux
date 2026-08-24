KDIR ?= /home/cube/WorkSpace/Linux/ARM_Linux/build/linux-6.1.44
ARCH ?= arm64
CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2
obj-m += hello_char.o

.PHONY: all clean app

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
app: hello_test hello_poll_test
hello_test: hello_test.c
	$(CC) $(CFLAGS) $< -o $@
hello_poll_test: hello_poll_test.c
	$(CC) $(CFLAGS) $< -o $@
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f hello_test hello_poll_test