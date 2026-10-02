# SPI — SPI 主设备访问实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机 |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物直接拷入虚拟机执行（访问 `/dev/spidev*`） |

## 说明

Linux SPI 用户态编程（`linux/spi/spidev.h` + `spidev_ioctl`）：查询主设备能力、配置模式/速率、读写传输。对应学习笔记 `docx/05-SPI设备开发.md`。

## 产物

- `spi_info` — 查询 SPI 主设备的模式/位宽/最大速率
- `spi_xfer` — 单向收发一次事务
- `spi_wr_rd` — 读写寄存器（写地址后读数据）