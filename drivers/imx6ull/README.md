# imx6ull — i.MX6ULL 内核驱动实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | i.MX6ULL 真机 |
| 工具链 | 内核 Kbuild（`obj-m`），需本机内核源码 |
| 构建 | 每子目录 `make`（默认 `build` 目标） |
| 部署 / 运行 | `insmod` / `rmmod`，再用配套 `xxxApp/xxapp` 验证 |

## 实验序列（入门 → 进阶）

| 目录 | 实验 | 说明 |
|---|---|---|
| `01_chrdevbase` | 字符设备基础 | cdev 注册，open/read/write/释放 |
| `02_led` | LED 灯驱动（寄存器直操） | CCM/GPIO 寄存器点亮板载 LED |
| `03_newchrled` | 新版字符驱动改写 | 统一为新字符设备框架 |
| `04_dtsled` | 设备树 LED 驱动 | `dtsi` 描述引脚 + of 接口解析 |
| `06_beep` | 蜂鸣器驱动 | GPIO 控制蜂鸣器开关 |
| `GPIO/` | 汇编 GPIO | `asm_led.s` 用汇编直接操作寄存器 |
| `sdk/` | NXP 寄存器定义头文件 | `MCIMX6Y2.h` / `fsl_*` |

## 注意

- 各 Makefile 的 `KERNELDIR` 需指向本机 i.MX6ULL 内核源码路径（01 残留教程作者路径、02~06 指向另一目录，编译前确认）。
- dtb 由 `dtsi` 经 SDK 自带 `dtc` 编译，见 `04_dtsled/README.md`。
- `.codex/skills/` 是驱动 / 应用 / BSP 三方向的 AI 协作技能定义。