KDIR ?= /home/cube/WorkSpace/Linux/ARM_Linux/build/linux-6.1.44
ARCH ?= arm64
CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2


.PHONY: all clean app

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
app: gpio_set
gpio_set: gpio_set.c
	$(CC) $(CFLAGS) $< -o $@ -lgpiod
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f gpio_set