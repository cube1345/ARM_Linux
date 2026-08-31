---
name: imx6ull-driver-dev
description: Use this skill whenever the user asks to design, port, build, debug, review, or deploy a Linux kernel driver for the i.MX6ULL project, including .ko modules, character devices, Device Tree, pinctrl/GPIO, interrupts, platform devices, I2C/SPI, input, LED, framebuffer, USB, CAN, PWM, RTC, audio, block, or network drivers. Always follow the repository's Linux 4.1.15 BSP constraints and use the shared i.MX6ULL build facts.
compatibility: Bash, GNU Make/Kbuild, the matching i.MX6ULL kernel source and generated artifacts, ARMv7-A cross toolchain, Device Tree compiler, and a target board with serial/USB access.
---

# i.MX6ULL Linux driver development

Read `../imx6ull-bsp/SKILL.md` first and `references/driver-guide-notes.md` for the vendor's chapter map. This project contains both bare-metal/SDK experiments and Linux modules; do not mix their startup, linker, interrupt, or register-access assumptions.

## Select the right driver architecture

- Use a standalone module only for a tightly scoped teaching experiment or an existing kernel interface with no hardware discovery.
- For a real board peripheral, describe resources in Device Tree and bind a `platform_driver` (or the appropriate bus driver). Put hardware setup in `probe`, teardown in `remove`, and keep per-device state in a structure referenced by `dev_set_drvdata`/`platform_set_drvdata`.
- Prefer existing kernel subsystems: `gpio-leds`/LED class, input, I2C, SPI, RTC, PWM, framebuffer/DRM, ALSA, CAN, USB, and the regulator/clock/pinctrl frameworks. A subsystem gives user space a stable ABI and avoids duplicating policy.
- The 4.1.15 tree uses legacy APIs (`gpio_request`, `of_get_named_gpio`, older `file_operations` fields). Match this tree exactly; do not paste a current 6.x example without checking signatures.

## Character-device lifecycle

For a new character device, prefer dynamic numbers and a complete rollback path:

```text
alloc_chrdev_region
  -> cdev_init / cdev_add
  -> class_create
  -> device_create        -> /dev/<name>
  -> file_operations: open/read/write/ioctl/poll/release
  -> reverse order on every failure and in remove/exit
```

Use `copy_to_user` for kernel-to-user data and `copy_from_user` for user-to-kernel data. Bound every length before copying. Return the number of bytes transferred on success and a negative errno on failure; do not return success after a partial/failed copy. Keep `private_data` for per-open or per-device state.

## Device Tree and GPIO

Define a stable `compatible` string and named properties, then match it through `of_match_table`. Obtain pinctrl states and GPIOs from the device rather than hard-coding SoC physical addresses. Check active-low flags, default state, pull/bias, and whether another node already consumes the pin.

For the repository's LED pin, the stock ALIENTEK DTS uses `GPIO1_IO03` as `sys-led` with `GPIO_ACTIVE_LOW` and a heartbeat trigger. A custom register-level LED module must disable that consumer for an isolated experiment; a production driver should instead extend or replace the DT node and use the LED/GPIO subsystem.

## Concurrency, blocking, and interrupts

- Protect shared state with the smallest suitable lock; never sleep while holding a spinlock or while a path needed by the wake-up producer is locked.
- For blocking `read`, use a wait queue and a condition; wake it after publishing data. Honor `O_NONBLOCK` with `-EAGAIN`.
- Implement `.poll` so applications can multiplex the device with `poll`/`select`/`epoll`.
- In interrupt handlers do minimal, non-sleeping work; defer longer work to a threaded IRQ, workqueue, or tasklet appropriate to this kernel.
- Check and propagate `request_irq`, `gpio_request`, `ioremap`, DMA, clock, regulator, and pinctrl failures. Every successful acquisition needs a release path.

## Build and test

Build against the exact target kernel output, not merely the source directory:

```sh
make -C /path/to/kernel M=$PWD ARCH=arm \
  CROSS_COMPILE=arm-poky-linux-gnueabi- modules
```

The output is valid only when generated headers, `CONFIG_*`, `Module.symvers`, compiler ABI, and `vermagic` match the board. Build the kernel's `modules` target when `CONFIG_MODVERSIONS=y`; `modules_prepare` alone is insufficient. Inspect with `file`, `modinfo`, and `dmesg`.

On target, test in this order:

```sh
insmod ./driver.ko
dmesg | tail -n 80
ls -l /sys/bus/platform/drivers /sys/class/<subsystem> /dev
# run the user-space exerciser, including invalid input and repeated open/close
rmmod driver
```

If binding fails, inspect the live DTB (`/proc/device-tree`), the driver's match table, pin ownership, clocks, and the first probe error. If loading fails, compare `uname -r`, `vermagic`, config, architecture, and `Module.symvers` before editing C code.

## Quality bar

Keep error paths explicit, make unload safe, avoid fixed major numbers for new production devices, avoid direct register pokes when a subsystem exists, and document the user-space ABI (node, commands, data format, active level, blocking behavior, and permissions).
