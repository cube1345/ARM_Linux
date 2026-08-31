---
name: imx6ull-app-dev
description: Use this skill whenever the user asks to create, port, build, debug, or deploy a user-space C application for the i.MX6ULL Linux project, especially programs using /dev, sysfs, GPIO, LED, input, framebuffer, serial, watchdog, I2C, SPI, CAN, socket, ALSA, or CMake. Keep hardware policy in the kernel interface and make the application portable across the board's rootfs.
compatibility: Bash, ARMv7-A cross compiler from the matching BSP/Buildroot SDK, target rootfs headers and libraries, and the project path ARM_Linux/IMX6ll.
---

# i.MX6ULL Linux application development

Read the shared `../imx6ull-bsp/SKILL.md` first for toolchain, kernel, DTB, and deployment constraints. Read `references/application-guide-notes.md` when choosing an API or a chapter-sized learning path.

## Layering rule

An application runs in user space. It consumes a stable kernel interface through system calls or libraries; it should not `mmap` SoC registers or duplicate pinctrl/clock policy. For a device, identify the interface first:

| Hardware function | Prefer in the application |
| --- | --- |
| Character device | `open`, `read`, `write`, `ioctl`, `poll` on `/dev/...` |
| GPIO/LED | `/sys/class/leds`, libgpiod if present, or the custom driver's `/dev` ABI |
| Input | `/dev/input/event*` with Linux input event structures |
| I2C | `/dev/i2c-*`, `I2C_SLAVE`/`I2C_RDWR`, SMBus helpers |
| Serial | `open` + `termios`, explicit baud/parity/flow control |
| Framebuffer | `/dev/fb0`, `FBIOGET_*`, `mmap`, format/stride-aware drawing |
| CAN | SocketCAN raw socket and `struct can_frame` |
| Network | BSD sockets; handle partial I/O and reconnects |
| Watchdog | `/dev/watchdog`, timeout and magic-close policy |

## Build discipline

Use the SDK's exported `CC`/`CXX` or an explicit target-prefixed compiler. Confirm every output with `file`; an i.MX6ULL binary must be 32-bit ARM EABI, not x86-64 or AArch64. Keep warnings enabled and check all system-call return values and `errno`.

For a small Makefile:

```make
CC ?= arm-poky-linux-gnueabi-gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?=

app: app.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@
```

For a multi-file application, use CMake only when it adds value; set a toolchain file with the target compiler and sysroot rather than relying on the host compiler. Link optional libraries explicitly (`-li2c`, ALSA, JPEG/PNG, etc.) and verify they exist in the target rootfs.

## I/O and concurrency patterns

- Treat file descriptors as finite resources; close them on every error path.
- For `read`/`write`, support short transfers and `EINTR`; do not assume one call moves the whole buffer.
- Use `O_NONBLOCK` plus `poll`/`select`/`epoll` when one loop serves multiple devices. A blocking read is appropriate only when the thread is dedicated to that source.
- Use mutexes/condition variables for shared application state; never busy-loop when a kernel wait or timer can block efficiently.
- Define signal handling with `sigaction`; keep handlers async-signal-safe and let the main loop perform cleanup.
- Parse numeric arguments with `strtol`/`strtoul`, validate ranges, and report the exact device path and failing operation.

## Device-oriented workflow

1. Inspect the target: `/proc/device-tree/model`, `/dev`, `/sys/class`, `dmesg`, and installed libraries.
2. Confirm the kernel driver and DTB expose the expected node before changing C code.
3. Build with the matching sysroot and cross compiler; run `file` and, for dynamic binaries, inspect the ELF interpreter and required libraries.
4. Transfer through USB/NFS/SD, run as the required user, and capture stdout/stderr plus `dmesg`.
5. Test both success and failure: missing node, permission denied, timeout, unplug/replug, short read/write, and clean shutdown.

For the repository's `02_led` example, the application contract is `write` one byte to `/dev/led`: `1` requests LED on and `0` requests LED off. The application must not try to access the GPIO registers itself. The stock `sys-led` heartbeat must be disabled or removed from the DTB before judging the result.

## Common failure triage

- `Exec format error`: wrong architecture or ABI; rebuild with target `CC`.
- `No such file or directory` for an existing binary: usually its dynamic loader or a shared library is absent; inspect with `readelf -l` and `ldd` in the target environment.
- `open` fails: verify DTB/driver, node name, permissions, and that the module is loaded.
- I2C reads are nonsense: verify 7-bit address, bus number, register width, byte order, and the chip's timing requirements.
- LED appears to fight the program: another `gpio-leds` trigger or consumer owns the GPIO.
