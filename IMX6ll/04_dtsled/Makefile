KERNELDIR ?= /home/cube/WorkSpace/iMX6Ull/linux
CURRENT_PATH := $(CURDIR)
BUILD_DIR := $(CURRENT_PATH)/build
ARCH ?= arm
CROSS_COMPILE ?= /home/cube/WorkSpace/iMX6Ull/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
KCFLAGS ?= -march=armv7-a
KERNEL_CONFIG := $(KERNELDIR)/include/config/auto.conf
KERNEL_SYMVERS := $(KERNELDIR)/Module.symvers
obj-m := dtsled.o
all: modules

.PHONY: prepare-output
prepare-output:
	@mkdir -p $(BUILD_DIR)
	@ln -sf $(CURRENT_PATH)/dtsled.c $(BUILD_DIR)/dtsled.c
	@ln -sf $(CURRENT_PATH)/Kbuild $(BUILD_DIR)/Kbuild

modules: prepare-output

	@test -r "$(KERNEL_CONFIG)" || { echo "ERROR: $(KERNEL_CONFIG) is missing."; exit 1; }
	@if grep -q "^CONFIG_MODVERSIONS=y" "$(KERNELDIR)/.config" 2>/dev/null && { ! test -s "$(KERNEL_SYMVERS)"; }; then echo "ERROR: $(KERNEL_SYMVERS) is missing or empty."; exit 1; fi
	$(MAKE) -C $(KERNELDIR) M=$(BUILD_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) KCFLAGS=$(KCFLAGS) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(BUILD_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	@rm -f $(BUILD_DIR)/dtsled.c $(BUILD_DIR)/Kbuild
