# i.MX6ULL Linux 字符驱动通用开发流程

本文以 `02_led` 为例，总结从编写内核驱动、编写用户态测试程序，到生成并在 ATK-IMX6U 板卡上加载测试 `.ko` 的完整流程。

## 1. 先分清三个对象

```text
led.c       内核空间驱动源码       -> led.ko
ledapp.c    用户空间测试程序       -> ARM Linux ELF 可执行文件
/dev/led    驱动注册后的设备节点   -> 主设备号 200，次设备号 0
```

应用通过系统调用进入 VFS，VFS 根据设备号找到驱动的 `file_operations`，再由驱动访问硬件：

```text
ledapp -> open/write -> VFS -> led_fops -> led_write -> GPIO1_IO03
```

## 2. 编写字符设备驱动

当前 [02_led/led.c](02_led/led.c) 的主要组成如下：

1. `module_init(led_init)` 和 `module_exit(led_exit)`：模块加载、卸载入口。
2. `register_chrdev(200, "led", &led_fops)`：注册字符设备，固定主设备号 200。
3. `struct file_operations led_fops`：实现 `.open`、`.read`、`.write`、`.release` 回调。
4. `led_write()`：从用户空间复制一个字节，`1` 点亮、`0` 熄灭。
5. `ioremap()`、`readl()`、`writel()`：映射并访问 GPIO/CCM/pinmux 寄存器。
6. `led_exit()`：取消寄存器映射并注销字符设备。

驱动接口约定为：

```text
设备节点：/dev/led
写入 1：LEDON
写入 0：LEDOFF
写入其他值或超过 1 字节：-EINVAL
```

生产驱动应优先使用 Device Tree、GPIO/LED subsystem，避免硬编码物理寄存器。本例是第四篇中的寄存器级教学实验，固定使用 GPIO1_IO03，且该引脚可能同时被系统 `sys-led` 使用。

## 3. 编写用户态测试程序

[02_led/ledapp.c](02_led/ledapp.c) 的流程是：

```c
fd = open("/dev/led", O_RDWR);
databuf[0] = atoi(argv[2]);
write(fd, databuf, 1);
close(fd);
```

调用格式：

```sh
./ledapp /dev/led 1
./ledapp /dev/led 0
```

`ledapp` 必须用 ARM 交叉编译器编译，不能使用主机 x86 GCC 直接编译后传到板卡。

## 4. 准备完全匹配的内核

外部模块必须针对板卡正在运行的内核构建。先在板卡确认：

```sh
uname -r
cat /proc/version
```

本次板卡返回：

```text
4.1.15-ge48931b1
gcc 5.3.0
```

主机侧需要同一份内核源码及其构建产物：

```text
linux/.config
linux/include/config/auto.conf
linux/include/generated/autoconf.h
linux/Module.symvers
```

`Module.symvers` 在 `CONFIG_MODVERSIONS=y` 时尤其重要。仅执行 `modules_prepare` 通常不能生成完整的内建符号 CRC；应使用与板卡相同配置完成内核构建，至少生成 `vmlinux`/内核符号表。

本项目使用的交叉工具链是：

```text
/home/cube/WorkSpace/iMX6Ull/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/
```

验证：

```sh
/home/cube/WorkSpace/iMX6Ull/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc --version
```

## 5. 配置并构建内核

如果确认 `imx_v7_defconfig` 与板卡出厂配置一致，可执行：

```sh
cd /home/cube/WorkSpace/iMX6Ull/linux
TC=/home/cube/WorkSpace/iMX6Ull/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-

make ARCH=arm CROSS_COMPILE=$TC imx_v7_defconfig
make ARCH=arm CROSS_COMPILE=$TC HOSTCFLAGS=-fcommon prepare modules -j16
make ARCH=arm CROSS_COMPILE=$TC HOSTCFLAGS=-fcommon vmlinux -j16
```

构建完整 `zImage` 时，旧内核还需要主机安装 `lzop`。当前模块构建已经使用生成的 `vmlinux` 和 446950 字节的 `Module.symvers` 完成。

## 6. 编译外部驱动模块

[02_led/Makefile](02_led/Makefile) 已设置：

```make
ARCH=arm
CROSS_COMPILE=.../arm-linux-gnueabihf-
obj-m := led.o
```

编译：

```sh
cd /home/cube/WorkSpace/iMX6Ull/ARM_Linux/IMX6ll/02_led
make
```

Makefile 会在内核生成文件缺失或 `Module.symvers` 不可用时主动失败，防止生成看似成功但无法加载的模块。

检查结果应包括：

```sh
file led.ko
modinfo led.ko
readelf -S led.ko | grep -E '__versions|\.modinfo'
```

本次生成的模块为 ARM ELF，vermagic 为：

```text
4.1.15-ge48931b1 SMP preempt mod_unload modversions ARMv7 p2v8
```

## 7. 串口传输到板卡

板卡只有串口时可使用 ZMODEM：

```sh
mkdir -p /tmp/upload
cd /tmp/upload
rz -y
```

主机终端选择发送：

```text
/home/cube/WorkSpace/iMX6Ull/ARM_Linux/IMX6ll/02_led/led.ko
```

`rz` 显示的 `B010000...` 是正常握手数据；传输完成后用 `ls` 确认文件存在。

用户态 `ledapp` 也需要用同样方式传到板卡，且必须是 ARM ELF。

## 8. 板卡加载和测试

```sh
cd /tmp/upload
insmod led.ko
dmesg | tail -n 50
lsmod | grep led
```

驱动注册后手动创建设备节点：

```sh
mknod /dev/led c 200 0
ls -l /dev/led
```

如果系统自带 LED heartbeat 正在使用同一 GPIO，先停用：

```sh
echo none > /sys/class/leds/sys-led/trigger
```

测试：

```sh
printf '\1' > /dev/led
printf '\0' > /dev/led
./ledapp /dev/led 1
./ledapp /dev/led 0
```

卸载：

```sh
rmmod led
```

卸载后 `/dev/led` 节点可能仍存在，但此时没有驱动处理它；重新加载前不要继续访问，必要时可删除节点：

```sh
rm -f /dev/led
```

## 9. `/dev` 中的数字是不是主次设备号？

是的。对字符设备或块设备，`ls -l /dev` 中设备文件类型后面的两个数字就是：

```text
主设备号, 次设备号
```

例如：

```text
crw-r--r-- 1 root root 200, 0 ... led
```

含义是：

```text
200 = 主设备号，选择 led 驱动
0   = 次设备号，选择该驱动的第 0 个设备实例
```

其他例子：

```text
fb0       29, 0    字符设备
mmcblk1   179, 0   块设备
console   5, 1     字符设备
```

这些数字是 Linux VFS 的逻辑路由编号，不是 GPIO 编号，也不是物理地址。当前 `200:0` 是驱动源码中的 `LED_MAJOR=200` 与命令 `mknod /dev/led c 200 0` 共同确定的。主设备号 200 只负责找到 `led_fops`，真正访问哪个 GPIO 由 `led.c` 中的寄存器地址和位号决定。

目录、符号链接、管道等没有这种主次设备号显示。例如 `/dev/fb` 是符号链接，不应把链接目标名称和设备号混淆。

## 10. 常见错误定位

```text
Kernel configuration is invalid
    缺少 auto.conf；准备匹配的内核配置。

Invalid module format
    对比 uname -r、modinfo vermagic、.config 和 Module.symvers。

Unknown symbol
    内核符号表或配置不匹配，重新针对同一内核构建。

没有 /dev/led
    驱动注册成功后执行 mknod，或检查 udev 规则。

insmod 成功但 LED 不亮
    停止 sys-led heartbeat，检查 active-low 极性、GPIO1_IO03 物理连接和设备树占用。
```
