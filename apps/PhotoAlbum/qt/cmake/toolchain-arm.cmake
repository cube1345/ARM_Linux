# NXP fsl-imx-x11 4.1.15-2.1.0 交叉编译工具链
# 用法: cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(SDK_ROOT "/home/cube/WorkSpace/Linux/ARM_Linux_WS/sdk/fsl-imx-x11/4.1.15-2.1.0")
set(TARGET_SYSROOT "${SDK_ROOT}/sysroots/cortexa7hf-neon-poky-linux-gnueabi")
set(TOOLCHAIN_BIN "${SDK_ROOT}/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi")

set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/arm-poky-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/arm-poky-linux-gnueabi-g++")
set(CMAKE_SYSROOT "${TARGET_SYSROOT}")

# Yocto SDK 的 Qt5Config.cmake 依赖此变量定位宿主工具 (moc/uic/rcc)
set(OE_QMAKE_PATH_EXTERNAL_HOST_BINS "${SDK_ROOT}/sysroots/x86_64-pokysdk-linux/usr/bin" CACHE PATH "Qt host tools")

set(ARCH_FLAGS "-march=armv7ve;-mfpu=neon;-mfloat-abi=hard;-mcpu=cortex-a7")
add_compile_options(${ARCH_FLAGS})
add_link_options(${ARCH_FLAGS})

set(CMAKE_FIND_ROOT_PATH "${TARGET_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
