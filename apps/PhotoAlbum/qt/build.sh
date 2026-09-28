#!/usr/bin/env bash
set -Ee
set -o pipefail

PROJECT_DIR="/home/cube/WorkSpace/ARM_Linux/apps/PhotoAlbum/qt"
PROJECT_FILE="${PROJECT_DIR}/photo_album.pro"
BUILD_DIR="/tmp/photoalbum-arm-build"
OUTPUT_FILE="${BUILD_DIR}/photo-album"
DIST_DIR="${PROJECT_DIR}/.dist"
DIST_FILE="${DIST_DIR}/photo-album"
SDK_ENV="/home/cube/WorkSpace/ARM_Linux/sdk/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi"

if [[ ! -f "${SDK_ENV}" ]]; then
    echo "错误：找不到 ARM Qt SDK 环境文件：${SDK_ENV}" >&2
    exit 1
fi

if [[ ! -f "${PROJECT_FILE}" ]]; then
    echo "错误：找不到 Qt 工程文件：${PROJECT_FILE}" >&2
    exit 1
fi

# Qt SDK 脚本可能在加载时读取这些变量，先确保它们已定义。
: "${CFLAGS:=}"
: "${CXXFLAGS:=}"
: "${CPPFLAGS:=}"
: "${LDFLAGS:=}"
source "${SDK_ENV}"

command -v qmake >/dev/null || {
    echo "错误：qmake 不可用，请检查 SDK 环境。" >&2
    exit 1
}

CXX_COMMAND="${CXX%% *}"
command -v "${CXX_COMMAND}" >/dev/null || {
    echo "错误：ARM 交叉编译器不可用：${CXX_COMMAND}" >&2
    exit 1
}

echo "开始编译 ARM Qt 程序..."
echo "qmake: $(command -v qmake)"
echo "编译器: ${CXX}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

cd "${BUILD_DIR}"

qmake "${PROJECT_FILE}"
make -j"$(nproc)"

file "${OUTPUT_FILE}"

if ! file "${OUTPUT_FILE}" | grep -q "ARM"; then
    echo "错误：生成文件不是 ARM 程序。" >&2
    exit 1
fi

cp -f "${OUTPUT_FILE}" "${DIST_FILE}"

echo
echo "编译完成："
echo "构建文件：${OUTPUT_FILE}"
echo "发布文件：${DIST_FILE}"
echo
md5sum "${DIST_FILE}"