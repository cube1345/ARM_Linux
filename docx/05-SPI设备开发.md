# 一、SPI 是什么

SPI，全称 Serial Peripheral Interface，是一种同步串行通信协议，常用于主控芯片和 Flash、ADC、DAC、屏幕、传感器等外设之间通信。和 IIC 不同，SPI 没有设备地址、没有 ACK/NACK，也没有 START/STOP 这种固定帧结构。SPI 的核心是片选和同步时钟：主机通过 CS 选中某个从机，然后通过 SCK 提供时钟，在 MOSI 和 MISO 上同步移位传输数据。

| 信号线     | 方向               | 作用                                     |
| ---------- | ------------------ | ---------------------------------------- |
| SCK / SCLK | 主机输出           | SPI 时钟线，由主机产生时钟               |
| MOSI       | 主机输出，从机输入 | 主机发送数据到从机                       |
| MISO       | 从机输出，主机输入 | 从机返回数据给主机                       |
| CS / SS    | 主机输出           | 片选线，通常低电平有效，用来选择某个从机 |

SPI 通常是全双工通信：主机每发送 1bit 的同时，也会从 MISO 接收 1bit。即使应用层只想读数据，主机也需要发送 dummy byte 来产生时钟，让从机把数据移出来。

# 二、SPI 数据格式

SPI 的协议层没有统一规定“地址位、读写位、ACK”这些字段。一次 SPI 通信通常可以抽象成以下过程：

1. 主机拉低 CS，从机被选中。
2. 主机输出 SCK 时钟。
3. 每一个有效采样边沿，MOSI 和 MISO 各传输 1bit。
4. 连续传输 bits\_per\_word 个 bit 组成一个 word，常见配置是 8bit。
5. 传输完成后，主机拉高 CS，本次通信结束。

假设发送 0x12345678，并采用常见的 8bit 字长和高字节优先，则线上会按 4 个字节发送：

```text
CS_LOW
MOSI: 0x12 0x34 0x56 0x78
MISO: rx0  rx1  rx2  rx3
CS_HIGH
```

如果外设协议要求低字节优先，则需要应用层自己调整顺序，例如发送 0x78、0x56、0x34、0x12。SPI 本身只负责按时钟移位，不负责解释 32bit 数据的字节序。

# 三、CPOL 和 CPHA

SPI mode 由 CPOL 和 CPHA 共同决定。CPOL 决定 SCK 空闲时是高电平还是低电平；CPHA 决定在第几个时钟边沿采样数据。配置错误时，常见现象是能看到时钟和波形，但读回数据错位、全 0、全 FF 或偶发错误。

| Mode   | CPOL | CPHA | SCK 空闲电平 | 采样边沿 | 数据变化边沿 |
| ------ | ---- | ---- | ------------ | -------- | ------------ |
| mode 0 | 0    | 0    | 低电平       | 上升沿   | 下降沿       |
| mode 1 | 0    | 1    | 低电平       | 下降沿   | 上升沿       |
| mode 2 | 1    | 0    | 高电平       | 下降沿   | 上升沿       |
| mode 3 | 1    | 1    | 高电平       | 上升沿   | 下降沿       |

实际开发中不要凭感觉选择 mode，应优先查外设 datasheet。很多 SPI Flash 支持 mode 0 或 mode 3，传感器、ADC、屏幕控制器则以各自手册为准。

# 四、SPI 和 IIC 的关键区别

| 对比项   | IIC                               | SPI                                         |
| -------- | --------------------------------- | ------------------------------------------- |
| 设备选择 | 7bit 或 10bit 设备地址            | CS 片选线                                   |
| 数据确认 | 每 8bit 后有 ACK/NACK             | 没有 ACK/NACK                               |
| 传输方式 | 半双工为主                        | 通常全双工                                  |
| 电气特性 | SCL/SDA 常用开漏，需要上拉        | 多为推挽输出，协议本身不要求上拉            |
| 数据格式 | START、地址、R/W、ACK、数据、STOP | CS、命令、地址、dummy、数据，具体由外设定义 |

SPI 通常不需要像 IIC 那样给所有通信线加上拉。SCK、MOSI、CS 一般由主机推挽驱动，MISO 由被选中的从机驱动。不过工程上常给 CS 加弱上拉，避免上电复位阶段片选悬空导致误选中外设。

# 五、Linux 用户态 spidev 接口

Linux 用户态访问 SPI 通常通过 spidev 字符设备完成，设备节点形式类似 /dev/spidev0\.0。这里的 0\.0 通常表示 SPI bus 0 上的 chip select 0。应用程序通过 open 打开设备，通过 ioctl 配置 mode、bits\_per\_word、speed\_hz，再用 SPI\_IOC\_MESSAGE\(N\) 发起一次或多次 SPI transfer。

```c
ioctl(fd, SPI_IOC_WR_MODE32, &mode);
ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz);
ioctl(fd, SPI_IOC_MESSAGE(1), &transfer);
```

read 和 write 更适合简单的半双工收发；如果要做全双工传输，或者要把“先写命令、再读数据”合成一次片选周期内的事务，应使用 SPI\_IOC\_MESSAGE\(N\)。

# 六、当前应用程序结构

当前工程的 SPI 应用位于 apps/SPI，采用交叉编译工具链生成 AArch64 目标程序。公共头文件 spi\_common\.h 封装了参数解析、打开设备、配置 SPI 和打印收发 buffer 的逻辑，三个应用程序分别覆盖查看配置、单段全双工传输、写后读组合传输。

| 程序        | 作用                                                           | 典型用途                                     |
| ----------- | -------------------------------------------------------------- | -------------------------------------------- |
| spi\_info   | 读取 spidev 当前 mode、lsb\_first、bits\_per\_word、max\_speed | 确认设备节点和当前 SPI 配置是否可访问        |
| spi\_xfer   | 一次 SPI\_IOC\_MESSAGE\(1\) 全双工传输                         | 发送命令和 dummy byte，同时接收返回数据      |
| spi\_wr\_rd | 一次 SPI\_IOC\_MESSAGE\(2\) 组合事务，先写再读                 | 适合“写寄存器地址后读取寄存器值”的外设模型 |

```bash
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/SPI
make clean
make
```

Makefile 当前会生成三个目标程序：spi\_info、spi\_xfer、spi\_wr\_rd。编译时使用 Buildroot 生成的 aarch64\-buildroot\-linux\-gnu\-gcc，不额外依赖第三方库。

# 七、运行示例

运行前必须确认目标系统存在 /dev/spidevB\.C。如果目标系统没有该设备节点，应用程序本身可以编译，但无法完成 SPI 通信验证。

```bash
ls /dev/spidev*
```

```bash
/root/spi_info /dev/spidev0.0
```

```bash
/root/spi_xfer /dev/spidev0.0 500000 0 0x9f 0x00 0x00 0x00
```

上面的例子常用于 SPI Flash 的 JEDEC ID 读取模型：0x9f 是命令字，后面的 0x00 是 dummy byte，用来产生时钟。主机发出 4 个字节的同时，也会接收 4 个字节。

```bash
/root/spi_wr_rd /dev/spidev0.0 500000 0 3 0x9f
```

这个例子表示：先写 1 字节命令 0x9f，再读取 3 字节数据。它使用两个 spi\_ioc\_transfer 组成一个 SPI\_IOC\_MESSAGE\(2\)。

# 八、系统配置注意事项

SPI 应用能否运行，不只取决于用户态代码，还取决于内核和设备模型是否提供 SPI master 与 spidev 设备。常见内核配置如下：

```text
CONFIG_SPI=y
CONFIG_SPI_SPIDEV=y
```

如果配置为模块，则可能是：

```text
CONFIG_SPI=m
CONFIG_SPI_SPIDEV=m
```

仅启用 spidev 还不够，系统还必须有具体 SPI 控制器驱动，以及一个和 spidev 绑定的 SPI device。QEMU virt 机器默认不一定直接提供可用 SPI 设备，因此不能把“应用编译成功”等同于“SPI 已完成硬件通信验证”。当前阶段最稳妥的验证前提是：目标系统中可以看到 /dev/spidev0\.0 这类设备节点，并且后面接有真实或可模拟的 SPI 外设。

# 九、常见问题

**找不到 /dev/spidev0\.0。** 说明当前系统没有暴露 spidev 设备。需要检查内核 SPI、SPIDEV、SPI 控制器驱动、设备树或平台设备绑定。

**ioctl SPI\_IOC\_MESSAGE 失败。** 先检查设备节点是否正确、mode 和 bits\_per\_word 是否被控制器支持、speed\_hz 是否超过控制器或外设能力。

**读回数据全 0 或全 FF。** 常见原因包括 CS 没有正确选中、MISO 悬空、mode 配错、外设未供电、命令字不对、dummy byte 数量不对。

**波形有时正常有时错误。** 优先降低 speed\_hz，再检查线长、地线、边沿过冲、CS 时序和外设最大频率。

**SPI 是否需要上拉。** SPI 协议本身不像 IIC 那样依赖上拉。CS 可以加弱上拉，避免复位阶段误选中；SCK、MOSI、MISO 是否加上下拉应按芯片手册和板级信号质量决定。

# 十、后续验证方向

完成用户态 SPI 应用后，下一步应优先补齐可验证的 SPI 设备环境。真实开发板上可以接 SPI Flash、ADC、OLED 或传感器；如果继续使用 QEMU，需要先确认所选 machine 是否能提供 SPI master，或者改用更适合 SPI 外设模拟的平台。完成 /dev/spidevB\.C 出现、spi\_info 能读取配置、spi\_xfer 或 spi\_wr\_rd 能读到稳定返回值后，才算 SPI 应用通信验证闭环。
