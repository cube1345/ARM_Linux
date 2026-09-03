# i.MX6ULL project skills

These local skills are scoped to `ARM_Linux/IMX6ll` and distilled from the two ALIENTEK guides plus verified facts in this repository.

| Skill | Use it for |
| --- | --- |
| `imx6ull-bsp` | toolchain, kernel/DTB, rootfs, USB/SD deployment, module compatibility |
| `imx6ull-app-dev` | user-space C programs and device-facing APIs |
| `imx6ull-driver-dev` | `.ko`, character devices, Device Tree, platform and subsystem drivers |

When a task crosses layers, load `imx6ull-bsp` first, then the application or driver skill. The PDFs remain the detailed reference; the skills provide the project's decision order and checked constraints.
