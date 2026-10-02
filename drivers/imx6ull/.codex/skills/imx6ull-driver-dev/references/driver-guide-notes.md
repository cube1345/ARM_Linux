# Driver guide notes

Source: `ARM_Linux/docx/【正点原子】I.MX6U嵌入式Linux驱动开发指南V2.0.1.pdf` (1963 pages; ALIENTEK i.MX6U/ULL guide).

## Architecture map

The guide's progression is intentionally bottom-up:

- Ch.4 (p.156): development environment and cross compiler.
- Ch.30–34 (p. roughly 800+): U-Boot use, build files, boot flow, and porting.
- Ch.35–39 (p.929–1034): Linux Makefiles, kernel boot/porting, rootfs, and system flashing.
- Ch.40–42 (p.1054 onward): classic and newer character-device patterns.
- Ch.43–45 (p.1109–1162): Device Tree, DT LED, pinctrl, and GPIO subsystem.
- Ch.47–55 (p.1205–1361): locking, key/input, timers, interrupts, blocking/non-blocking I/O, platform, and DT platform drivers.
- Ch.56–76 (p.1371 onward): in-tree LED, MISC, INPUT, LCD, RTC, I2C, SPI, serial, touch, audio, CAN, USB, block, network, Wi-Fi/4G, HDMI, PWM, regmap, IIO, and ADC.

## Patterns worth reusing

1. A character driver exposes a `file_operations` ABI, copies data through the user-copy helpers, and owns a clear init/exit rollback path.
2. A DT/platform driver moves hardware setup from module init into `probe`, uses an `of_device_id` table for `compatible`, and releases resources in `remove`.
3. GPIO/pinctrl data belongs in DT. The driver consumes named GPIOs and active-level flags rather than scattering board addresses through C.
4. Kernel-provided LED support (`compatible = "gpio-leds"`) is a platform driver and should be preferred over a bespoke register driver for ordinary LEDs.
5. Blocking/non-blocking I/O, wait queues, poll, asynchronous notification, and locking are separate mechanisms; select one deliberately based on the application contract.
6. Subsystem drivers (I2C/SPI/input/USB/PWM/etc.) should register with the subsystem and let the kernel expose the conventional user ABI.

## Vendor-specific cautions

The guide uses sample paths and sometimes static major numbers or legacy GPIO APIs because it targets Linux 4.1.15 and teaching boards. Preserve those only when matching an existing exercise. For new production code in this tree, verify the exact 4.1.15 API and prefer dynamic allocation, `devm_*` cleanup where available, and the relevant subsystem.

## Bring-up evidence

The guide's expected evidence is not just “module loaded”: check the kernel log, the driver and device directories under `/sys`, the created `/dev` node, a user-space test application, and clean removal. Capture the active DTB and kernel configuration when diagnosing a board-specific failure.
