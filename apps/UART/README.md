# UART — 串口开发实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机 |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物直接拷入虚拟机执行（访问 `/dev/ttyS*`） |

## 说明

Linux 串口（tty 线路规程）用户态编程：打开串口、设置波特率/数据位/校验、阻塞与超时读写。对应学习笔记 `docx/03-UART设备开发.md`。

## 产物

- `uart_basic` — 打开串口并设置 termios 参数
- `uart_loop` — 阻塞读写回环测试
- `uart_select` — 用 `select()` 做带超时的非阻塞收发