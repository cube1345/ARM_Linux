# Verified project facts

This note records facts checked while creating the local skills (2026-08-31).

- `linux/Makefile`: VERSION 4, PATCHLEVEL 1, SUBLEVEL 15.
- `linux/build.sh` sources `/opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi`, calls `imx_v7_defconfig`, builds `zImage`, DTBs, `modules`, and installs modules under a temporary root.
- The expected SDK file is currently absent on the host; do not claim a full vendor-kernel build succeeded until it is installed or another matching SDK is explicitly selected.
- `linux/arch/arm/boot/dts/imx6ull-alientek-emmc.dts` defines `leds/led1`, label `sys-led`, GPIO `<&gpio1 3 GPIO_ACTIVE_LOW>`, and `linux,default-trigger = "heartbeat"`.
- `imx6ull-alientek-nand.dts` includes the eMMC DTS and changes storage nodes; choose DTB/storage variant from the actual board.
- `02_led/Makefile` is an external Kbuild Makefile. Its current teaching defaults are `ARCH=arm`, `CROSS_COMPILE=arm-linux-gnueabihf-`, `KCFLAGS=-march=armv7-a`, and `obj-m := led.o`. Override these defaults when using the vendor SDK.
- The `02_led` register-level example uses `CCM_CCGR1`, IOMUXC GPIO1_IO03, GPIO1 DR/GDIR, static major 200, and `/dev/led`; it is intentionally pedagogical and should not be treated as a production GPIO driver API.
