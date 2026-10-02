# Application guide notes

Source: `ARM_Linux/docx/【正点原子】I.MX6U嵌入式Linux C应用编程指南V1.6.pdf` (1115 pages; vendor beginner-to-intermediate guide).

## Learning architecture

The guide explicitly separates application work from driver work. Its progression is:

1. System calls and C library concepts.
2. File descriptors and file I/O (`open/read/write/close`), file metadata and directories.
3. Processes, environment, signals, IPC, threads, synchronization, and daemon/file-lock patterns.
4. Blocking/non-blocking I/O and multiplexing.
5. Hardware-facing user APIs: LED (around p.513), GPIO (p.522), input (p.537), framebuffer (p.583), serial (p.740), watchdog (p.765), socket (p.908), and SocketCAN (p.924).
6. CMake and cross-compilation (CMake cross-compilation around p.1010), followed by MQTT/video-oriented projects.

## API principles to carry into code

- A library function is a user-space convenience layer; a system call is the kernel boundary. Choose the simplest interface that preserves needed control and error reporting.
- Device files are the application/driver contract. Open the node, configure it, perform I/O, check returns, and close it.
- Blocking is a policy decision. Use it for a single-purpose worker; use non-blocking plus `poll` when coordinating several sources.
- For register-like protocols (I2C/SPI), preserve unrelated bits with read-modify-write masks and respect the peripheral datasheet; do not infer semantics from a successful system call alone.
- Cross-compile applications for the target ABI and deploy their runtime libraries with the rootfs. Host builds are useful for logic tests, not hardware acceptance.

## Scope caution

The guide is board-oriented and introductory. Its sample paths, device names, active levels, library availability, and display/audio assumptions must be checked against this repository's DTB and rootfs. Prefer the project code and `project-facts.md` for current paths.
