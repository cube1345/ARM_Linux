
# 一、IIC 基础

I2C（Inter-Integrated Circuit）是由 SCL 和 SDA 两根信号线组成的同步串行总线。SCL 是时钟线，SDA 是数据线，主设备通过从设备地址访问目标器件。Linux 中通常将 IIC 称为 I2C，应用层通过 I2C character device 和 SMBus 接口访问设备。

一次典型的用户态访问包含四步：打开 `/dev/i2c-X`，使用 `ioctl(I2C_SLAVE)` 设置从设备地址，调用 SMBus 或 I2C_RDWR 接口完成读写，最后关闭文件描述符。

# 二、当前实验环境

由于 QEMU 虚拟机没有真实 I2C 外设，本章使用 Linux 内核的 `i2c-stub` 模拟从设备。它不模拟具体芯片协议，但可以提供一个可访问的虚拟从设备，用于验证用户态打开设备、设置地址、寄存器读写和位操作流程。

| 项目           | 配置或结果                                             |
| -------------- | ------------------------------------------------------ |
| CPU/系统       | ARM64 Buildroot Linux                                  |
| 内核           | Linux 6.1.44                                           |
| I2C 总线设备   | `/dev/i2c-0`                                         |
| 虚拟从设备地址 | `0x50`                                               |
| 访问库         | `libi2c`，链接参数为`-li2c`                        |
| 命令行工具     | `i2cdetect`、`i2cget`、`i2cset`、`i2ctransfer` |

# 三、内核与 Buildroot 配置

内核至少需要启用 I2C 核心、I2C 字符设备接口和虚拟设备模块：

```text
CONFIG_I2C=y
CONFIG_I2C_CHARDEV=y
CONFIG_I2C_STUB=m
```

Buildroot 需要加入 I2C 工具，便于在目标系统中扫描总线和验证设备：

```text
BR2_PACKAGE_BUSYBOX_SHOW_OTHERS=y
BR2_PACKAGE_I2C_TOOLS=y
```

重新构建内核后安装模块，并将生成的 `Image` 复制到 `images/Image`。重新构建 Buildroot 后，目标文件系统中应包含 I2C 工具和 `i2c-stub.ko`。

# 四、启动虚拟 I2C 设备

进入 QEMU 后加载虚拟设备模块：

```sh
modprobe i2c-stub chip_addr=0x50
ls -l /dev/i2c-0
```

如果模块加载成功，系统会创建 I2C 总线设备。使用工具扫描总线：

```sh
i2cdetect -y 0
```

扫描结果中应能看到从设备地址 `50`。注意，`0x50` 是 7-bit 从设备地址，用户程序传给 `I2C_SLAVE` 时也使用这个地址，不要再左移一位。

# 五、SMBus 用户态访问流程

本项目使用 Buildroot 生成的交叉编译器和 `libi2c`。程序头文件主要包括 `linux/i2c-dev.h`、`i2c/smbus.h`、`fcntl.h` 和 `sys/ioctl.h`。

```c
int fd;
int ret;

fd = open("/dev/i2c-0", O_RDWR);
if (fd < 0) {
    perror("open");
    return 1;
}

if (ioctl(fd, I2C_SLAVE, 0x50) < 0) {
    perror("ioctl I2C_SLAVE");
    close(fd);
    return 1;
}

ret = i2c_smbus_write_byte_data(fd, 0x00, 0x12);
if (ret < 0) {
    perror("i2c_smbus_write_byte_data");
}

ret = i2c_smbus_read_byte_data(fd, 0x00);
if (ret < 0) {
    perror("i2c_smbus_read_byte_data");
}

close(fd);
```

`i2c_smbus_write_byte_data(fd, reg, value)` 适合向一个寄存器写入一个字节；`i2c_smbus_read_byte_data(fd, reg)` 先指定寄存器，再读取一个字节。返回值小于 0 表示访问失败，读取成功时返回值范围为 0 到 255。

# 六、基础读写程序 iic_basic

`iic_basic` 接收设备节点、从设备地址、寄存器地址和写入值，完成一次寄存器写入，然后读取同一寄存器并打印结果。参数通过 `strtol` 解析，因此地址可以使用十进制或 `0x` 开头的十六进制形式。

```sh
cd /root/IIC
make

./iic_basic /dev/i2c-0 0x50 0x00 0x12
```

程序的核心验证点是：能够打开 I2C 设备、设置从设备地址、写入寄存器、读回寄存器，并正确处理系统调用失败。

# 七、连续寄存器读取 iic_dump

`iic_dump` 从指定寄存器开始连续读取多个字节，每行最多打印 16 个字节，适合观察一段寄存器空间。

```sh
./iic_dump /dev/i2c-0 0x50 0x00 16
```

参数含义依次为设备节点、从设备地址、起始寄存器和读取长度。程序会检查寄存器地址是否超过 `0xff`，防止 8-bit 寄存器地址溢出。

# 八、按位更新寄存器 iic_update_bits

修改芯片寄存器中的单个位或一组位时，不能直接覆盖整个寄存器，否则可能破坏其他位。常用方法是先读出原值，再使用掩码计算新值：

```c
new_value = (old_value & ~mask) | (value & mask);
```

其中 `mask` 表示允许修改的位，`value` 提供这些位的新值，未被掩码选中的位保持原值。程序写入后再次读取寄存器进行校验，这种模式也是实际传感器、RTC 和 EEPROM 外设驱动中常见的寄存器操作方式。

# 九、Makefile 与交叉编译

本项目使用 ARM64 Buildroot 工具链，并通过 `-li2c` 链接 SMBus 辅助函数：

```make
CROSS_COMPILE ?= /home/cube/WorkSpace/Linux/ARM_Linux/host/bin/aarch64-buildroot-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -O2

TARGET := iic_basic iic_dump iic_update_bits

all: $(TARGET)

iic_basic: iic_basic.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c

iic_dump: iic_dump.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c

iic_update_bits: iic_update_bits.c
	$(CC) $(CFLAGS) -o $@ $^ -li2c
```

如果出现 `undefined reference to i2c_smbus_*`，通常是遗漏了 `-li2c`。如果出现头文件找不到，需要确认交叉编译器的 sysroot 中包含 `i2c/smbus.h` 和 Linux I2C 头文件。

# 十、i2c-tools 常用命令

| 命令            | 作用                   | 示例                                   |
| --------------- | ---------------------- | -------------------------------------- |
| `i2cdetect`   | 扫描总线上的从设备地址 | `i2cdetect -y 0`                     |
| `i2cget`      | 读取指定设备的寄存器   | `i2cget -y 0 0x50 0x00`              |
| `i2cset`      | 写入指定设备的寄存器   | `i2cset -y 0 0x50 0x00 0x12`         |
| `i2ctransfer` | 执行组合读写事务       | `i2ctransfer -y 0 w2@0x50 0x00 0x12` |

工具适合快速验证总线和设备，但正式应用应根据目标芯片协议编写明确的程序，处理寄存器宽度、字节序、延时、重试和错误恢复。

# 十一、常见问题

 **找不到**  **`/dev/i2c-0`**  **。** 检查 `CONFIG_I2C_CHARDEV` 是否启用，确认 `i2c-dev` 驱动已经加载，并检查 QEMU 启动后的内核日志。

 **打开设备失败。** 检查设备节点是否存在、程序是否为 ARM64 架构，以及目标文件系统是否包含动态链接所需的库。

 **设置地址失败。** 确认地址是 7-bit 地址，确认总线编号正确，并确认目标设备已被内核注册或由 `i2c-stub` 创建。

 **读写返回负值。** 检查从设备是否响应、寄存器地址是否有效、SMBus 访问方式是否符合芯片手册，以及是否需要等待芯片内部操作完成。

**运行时提示找不到 ** **`libi2c.so`**  **。** 确认目标系统中存在对应动态库；临时调试时可以检查 `LD_LIBRARY_PATH`，产品系统应将库正确安装到标准库目录。
