# IIC — I²C 设备读写实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机 |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物直接拷入虚拟机执行（读写 `/dev/i2c-*`） |

## 说明

Linux I²C 用户态编程（`linux/i2c-dev.h`）：打开适配器、查询从设备、读写寄存器。对应学习笔记 `docx/04-IIC设备开发.md`。

## 产物

- `iic_basic` — 打开/关闭 I²C 适配器，读取从设备地址探测
- `iic_dump` — 遍历总线 dump 从设备寄存器
- `iic_update_bits` — 读改写指定寄存器某几位（update bits 操作）