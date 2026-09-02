KERNELDIR := /home/dell/WorkSpace/cube_ws/IMX6ll/linux
CURRENT_PATH := $(shell pwd)

obj-m := chrdevbase.o
build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
