---
name: imx6ull-bsp
description: Use this skill whenever work targets the i.MX6ULL Linux BSP in ARM_Linux/IMX6ll, including cross-compilation, kernel or module builds, Device Tree/DTB selection, rootfs installation, USB/SD/NAND/eMMC deployment, board bring-up, or diagnosing version and toolchain mismatches. Treat the repository-specific facts below as the source of truth and verify the live board before changing them.
compatibility: Bash, GNU Make, Linux 4.1.15 source tree, ARMv7-A cross toolchain, and a serial console or USB transport for board verification.
---

# i.MX6ULL BSP workflow

Use this as the shared foundation for the application and driver skills. The two vendor PDFs are learning references; the checked-out tree and the board's running kernel win whenever they disagree.

## Repository facts to verify first

- Linux source: `/home/cube/WorkSpace/iMX6Ull/linux`.
- Kernel line: 4.1.15; the repository's `linux/build.sh` expects the NXP/ALIENTEK Yocto SDK at `/opt/fsl-imx-x11/4.1.15-2.1.0/` and its `environment-setup-cortexa7hf-neon-poky-linux-gnueabi` file (GCC 5.3-era toolchain).
- The vendor build script uses `make imx_v7_defconfig`, then builds `zImage`, DTBs, and `modules`. Do not invent a new `.config` for a board already running a vendor image.
- `ARCH=arm`; the target is Cortex-A7/ARMv7-A, little-endian, normally hard-float userspace (`cortexa7hf-neon`).
- `ARM_Linux/IMX6ll/` contains teaching examples. `02_led` produces `led.ko` from `led.c` and a user program from `ledapp.c`.

Before compiling an external module, collect the exact target identity:

```sh
uname -r
cat /proc/version
test -r /proc/config.gz && zcat /proc/config.gz | grep CONFIG_MODVERSIONS || true
```

The module must use the same kernel release, configuration, generated headers, and `Module.symvers` as the running kernel. `modules_prepare` creates headers but does not create a usable `Module.symvers` when `CONFIG_MODVERSIONS=y`; build the kernel's `modules` target or obtain the matching file from the BSP build.

## Kernel and DTB selection

Use the DTB that matches the board's storage and display. This tree has ALIENTEK eMMC and NAND variants and many display-specific DTBs. Inspect the actual bootloader environment or `/proc/device-tree/model`; do not assume eMMC from the CPU name alone.

The stock ALIENTEK DTS declares a `gpio-leds` node named `sys-led` on `GPIO1_IO03`, active low, with a heartbeat trigger. A teaching register-level LED module using the same pin must stop that trigger and avoid binding the pin twice. Prefer a Device Tree/platform LED driver for production code.

## External module build pattern

Keep the module Makefile thin and let Kbuild compile against the target kernel:

```make
KERNELDIR ?= /path/to/matching/kernel-or-output
ARCH ?= arm
CROSS_COMPILE ?= arm-poky-linux-gnueabi-
obj-m := example.o

all:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) modules
```

The prefix ends in `-`; it is not the path to `gcc`. Never silently fall back to the host `cc` for a target binary or to a different kernel tree for a module.

Linux 4.1.15 is old. Ubuntu 24.04 host GCC/binutils can fail while building the whole vendor kernel even if a tiny module compiles. Prefer the BSP SDK toolchain; workarounds such as `HOSTCFLAGS=-fcommon` or `KCFLAGS=-march=armv7-a` are diagnostic aids, not proof that a production kernel is reproducible.

## Deployment and acceptance

For a first bring-up, transfer `*.ko`, the ARM user binary, and any DTB/rootfs changes through the available USB OTG/MfgTool, SD card, NFS, or USB network. For iterative module work, copy only the module and application.

On the board:

```sh
insmod ./example.ko       # or modprobe example after depmod
dmesg | tail -n 50
ls -l /dev                 # verify the expected node
```

Check architecture with `file`, check `vermagic` with `modinfo`/`strings`, and capture the first kernel error before changing code. Exercise load, normal operation, repeated open/close, and unload. For GPIO/LED tests, verify electrical polarity and the physical pin from the board schematic, not just the SoC name.

## Source references

- Application guide: `references/application-guide-notes.md` in `imx6ull-app-dev`.
- Driver guide: `references/driver-guide-notes.md` in `imx6ull-driver-dev`.
- This project's checked-in kernel script: `/home/cube/WorkSpace/iMX6Ull/linux/build.sh`.
