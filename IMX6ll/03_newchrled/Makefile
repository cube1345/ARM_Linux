KERNELDIR ?= /home/cube/WorkSpace/iMX6Ull/linux
CURRENT_PATH := $(CURDIR)
ARCH ?= arm
CROSS_COMPILE ?= /home/cube/WorkSpace/iMX6Ull/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
KCFLAGS ?= -march=armv7-a

# Require generated files from the exact kernel that will run on the board.
KERNEL_CONFIG := $(KERNELDIR)/include/config/auto.conf
KERNEL_SYMVERS := $(KERNELDIR)/Module.symvers

obj-m := led.o
build: kernel_modules

all: kernel_modules

kernel_modules:
	@test -r "$(KERNEL_CONFIG)" || { echo "ERROR: $(KERNEL_CONFIG) is missing. Configure the matching board kernel first."; exit 1; }
	@if grep -q "^CONFIG_MODVERSIONS=y" "$(KERNELDIR)/.config" 2>/dev/null && { ! test -s "$(KERNEL_SYMVERS)"; }; then echo "ERROR: $(KERNEL_SYMVERS) is missing or empty while CONFIG_MODVERSIONS=y."; echo "Build the matching kernel (make modules) first."; exit 1; fi
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		KCFLAGS=$(KCFLAGS) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		KCFLAGS=$(KCFLAGS) clean
