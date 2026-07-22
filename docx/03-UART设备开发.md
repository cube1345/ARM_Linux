
# 一、当前 QEMU 环境

当前虚拟机使用的是 ARM virt 平台，串口控制台是 `/dev/ttyAMA0`。它既是系统 console，也是我们学习 UART 应用开发的测试入口。

```text
-append "rootwait root=/dev/vda console=ttyAMA0"
```

这意味着 `/dev/ttyAMA0` 会被系统登录 shell 占用，所以它适合学习串口 API，但不适合当成独立的外设串口做复杂双向协议测试。

# 二、termios 的核心概念

UART 应用开发最关键的就是 `termios`。它负责设置串口波特率、数据位、校验位、停止位，以及读写的阻塞行为。

| 概念                                | 作用                                         |
| ----------------------------------- | -------------------------------------------- |
| `cfmakeraw()`                     | 关闭行缓冲、回显和特殊字符处理，进入原始模式 |
| `cfsetispeed()`/`cfsetospeed()` | 设置输入和输出波特率                         |
| `CS8`                             | 8 位数据位，常见于 8N1                       |
| `PARENB`                          | 校验位开关，关闭表示无校验                   |
| `CSTOPB`                          | 停止位控制，关闭表示 1 位停止位              |
| `VMIN`/`VTIME`                  | 控制`read()`的返回时机                     |

常用串口格式可以理解为 8N1：8 位数据位、无校验、1 位停止位。

# 三、一个标准串口配置函数

所有程序都共用一套配置逻辑：先 `tcgetattr()` 读取旧配置，再 `cfmakeraw()` 进入 raw mode，接着设置波特率和 8N1，最后 `tcsetattr()` 使配置生效。

```c
tcgetattr(fd, &tio);
cfmakeraw(&tio);
cfsetispeed(&tio, speed);
cfsetospeed(&tio, speed);
tio.c_cflag |= CLOCAL | CREAD;
tio.c_cflag &= ~CSIZE;
tio.c_cflag |= CS8;
tio.c_cflag &= ~PARENB;
tio.c_cflag &= ~CSTOPB;
tio.c_cflag &= ~CRTSCTS;
tcflush(fd, TCIOFLUSH);
tcsetattr(fd, TCSANOW, &tio);
```

这里最容易记错的是 `VMIN` 和 `VTIME`。它们决定 `read()` 是阻塞等待，还是带超时返回。

# 四、三个练习程序

| 程序            | 做什么                             | 关键点                            |
| --------------- | ---------------------------------- | --------------------------------- |
| `uart_basic`  | 一次发送字符串，再尝试读取返回数据 | 验证 open / config / write / read |
| `uart_loop`   | 持续阻塞接收串口数据               | `read()`一直等数据              |
| `uart_select` | 先`select()`等待，再`read()`   | 学会超时机制                      |

`uart_basic` 适合做最小验证；`uart_loop` 适合观察阻塞读；`uart_select` 适合学习“有数据就读，没数据就超时”的真实工程写法。

# 五、阻塞与超时

`uart_loop` 里通常把 `VMIN=1`、`VTIME=0`，这样 `read()` 会一直等到至少 1 个字节到来。

`uart_select` 则用 `select()` 先判断 fd 是否可读，再调用 `read()`。这样程序可以在没数据时打印 timeout，或者继续执行其他逻辑。

```c
FD_ZERO(&readfds);
FD_SET(fd, &readfds);
ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
if (ret == 0) {
    printf("timeout\n");
}
```

# 六、常见坑

| 问题                   | 原因                           | 处理                               |
| ---------------------- | ------------------------------ | ---------------------------------- |
| `read`返回`EAGAIN` | 打开串口时加了`O_NONBLOCK`   | 入门阶段先去掉`O_NONBLOCK`       |
| 输入输出混乱           | `/dev/ttyAMA0`是系统 console | 知道它被占用，别把它当普通外设串口 |
| 程序一直 timeout       | 当前没有外部输入源             | 这是正常现象，说明超时逻辑工作正常 |
| `open`失败           | 设备节点不存在或权限不足       | 先确认`ls -l /dev/ttyAMA*`       |
