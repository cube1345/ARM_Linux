
# 一、环境验证

GPIO 环境是否可用，先看两个层面：内核是否注册 GPIO 控制器、用户态是否具备 libgpiod 工具。
ls -l /dev/gpiochip*
dmesg | grep -i gpio
本次验证结果中，/dev/gpiochip0 已出现，并且内核日志显示：


pl061_gpio 9030000.pl061: PL061 GPIO chip registered


这说明 QEMU 的 PL061 GPIO 控制器已经被内核识别并注册。


gpiodetect
gpioinfo
本次输出为：
gpiochip0 [9030000.pl061] (8 lines)


gpiochip0 - 8 lines:
    line   0:      unnamed       unused   input  active-high
    line   1:      unnamed       unused   input  active-high
    line   2:      unnamed       unused   input  active-high
    line   3:      unnamed       unused   input  active-high
    line   4:      unnamed       unused   input  active-high
    line   5:      unnamed       unused   input  active-high
    line   6:      unnamed       unused   input  active-high
    line   7:      unnamed       unused   input  active-high
这里的 8 lines 表示这个虚拟 GPIO 控制器提供 8 根 GPIO line，编号为 0 到 7。默认状态是 unused、input、active-high。

# 二、Buildroot 配置要点

为了让 GPIO 用户态工具可用，Buildroot 中需要打开 libgpiod 和工具包：
BR2_PACKAGE_LIBGPIOD=y
BR2_PACKAGE_LIBGPIOD_TOOLS=y
重新执行 make 生成 rootfs 后，工具会进入目标文件系统：
/usr/bin/find target host -type f \( -name gpiodetect -o -name gpioinfo \)

# 三、用户态 API 基本模型

libgpiod 的基本操作流程可以概括为五步：打开 gpiochip、获取 line、申请 line 的输入或输出模式、读写 value、释放资源。

1. 使用 gpiod_chip_open_by_name() 打开 GPIO 控制器，例如 gpiochip0。
2. 使用 gpiod_chip_get_line() 获取某一根 GPIO line，例如 line 0。
3. 使用 gpiod_line_request_output() 或 gpiod_line_request_input() 申请使用这根 line。
4. 输出方向使用 gpiod_line_set_value() 写电平，输入方向使用 gpiod_line_get_value() 读电平。
5. 程序结束前调用 gpiod_line_release() 和 gpiod_chip_close() 释放资源。
   GPIO line 是独占申请的。一个程序申请某根 line 后，其他程序不能同时再申请同一根 line。运行 gpioinfo 时可以看到 line 当前是否被某个 consumer 占用。
