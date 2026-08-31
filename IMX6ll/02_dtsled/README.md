# 02_dtsled：Device Tree GPIO LED 驱动

本例保留 `02_led` 寄存器版驱动，新增 `platform_driver + of_match_table + GPIO API` 实现。驱动从 DTS 的 `gpios` 属性取得 GPIO，并动态分配设备号，成功后由 `device_create()` 创建 `/dev/dtsled`。

## DTS 修改

将 [dtsled.dtsi](dtsled.dtsi) 的节点合入实际启动使用的板级 DTS，并确保 GPIO1_IO03 不再同时由原有 `gpio-leds/sys-led` 使用。重新编译并替换板卡实际加载的 DTB；仅加载 `.ko` 而不更新 DTB，不会触发驱动 `probe()`。

## 编译

```sh
cd /home/cube/WorkSpace/iMX6Ull/ARM_Linux/IMX6ll/02_dtsled
make
file build/dtsled.ko
modinfo build/dtsled.ko
```

## 板卡测试

```sh
insmod build/dtsled.ko
dmesg | tail -n 30
ls -l /dev/dtsled
printf '\1' > /dev/dtsled
printf '\0' > /dev/dtsled
rmmod dtsled
```

设备号由 `alloc_chrdev_region()` 动态分配，日志会打印实际 major/minor，不要预先写死 `mknod` 号码。
