# update_agent — OTA 升级客户端

| 元信息 | 值 |
|---|---|
| 目标板 / 环境 | QEMU AArch64 虚拟机 |
| 工具链 | aarch64-buildroot-linux-gnu |
| 构建 | `make` |
| 部署 / 运行 | 产物 `update_agent` 拷入虚拟机执行 |

## 说明

嵌入式设备的在线升级（OTA）客户端：负责拉取升级包并完成固件替换流程。单文件实现（`main.c`），启动参数与协议约定见源码头注释。