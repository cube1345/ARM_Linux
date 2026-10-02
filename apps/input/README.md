# input — Linux 输入子系统实验

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机 |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物直接拷入虚拟机执行（读 `/dev/input/event*`） |

## 说明

Linux Input 子系统用户态实验：读取输入设备事件、识别按键/触摸、遍历输入节点。

## 产物

- `input_read` — 读取指定 input 设备的 raw 事件
- `input_keyboard` — 键盘事件解析（键码 → 键值）
- `dir_scan` — 扫描 `/dev/input/` 设备列表